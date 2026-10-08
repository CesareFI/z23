/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: dev.agent.outcomes usage_log — per-event model token usage read
 *          from Muse host session logs and Claude Code transcripts,
 *          deduplicated by event id, with each format's counter meaning kept.
 *
 * ── CONTRACT ──────────────────────────────────────────────────────────────
 *
 * WHY. A unit receipt records one token number per attempt, and that number
 * cannot say what it contains: the Muse wire has no cache field, so a
 * receipt's "billed" total silently includes cache reads, and a refused
 * attempt can record 0 while its model calls really ran. The model host's
 * own log records every call with its raw counters. This reader folds those
 * logs so usage is read from what the host observed, per event.
 *
 * INPUT. dev.agent.outcomes gains one optional key, usage_log: a path to one
 * JSONL file, or a directory walked for *.jsonl (depth <= DVU_MAX_DEPTH,
 * symlinks never followed). The outcomes model/since filters apply to usage
 * events too (since compares the event's UTC hour).
 *
 * FORMATS, recognized per line; any other line is ignored:
 *   muse    payload.event.kind == "model_completed". Event id = record id.
 *           Hour from recorded_at (microseconds). Counters from
 *           payload.event.usage: input_tokens (INCLUDES cache reads),
 *           output_tokens, cache_read_tokens, cache_write_tokens,
 *           reasoning_tokens.
 *   claude  type == "assistant" with message.usage. Event id = message.id;
 *           the transcript repeats one message on several lines, and the
 *           LAST line for an id wins (its counters replace, never add).
 *           Hour from timestamp. Counters: input_tokens (EXCLUDES cache
 *           reads), output_tokens, cache_read_input_tokens,
 *           cache_creation_input_tokens, output_tokens_details
 *           .thinking_tokens.
 *   codex   event_msg payload.type == "token_count" with info.total_token_usage.
 *           Account creator_account_id + session_meta payload.id identify a
 *           rollout generation; ordinal orders cumulative checkpoints, including
 *           copied files. Keep the last checkpoint, never add last_token_usage.
 *           Unknown identity/ordinal is unkeyed. Resets, changed counter coverage
 *           and conflicting ordinals refuse with USAGE_CONFLICT. Model remains
 *           unknown. Only explicit root sessions (session_id == id, no parent)
 *           qualify; inherited or missing lineage refuses with USAGE_CONFLICT.
 *           Malformed records invalidate an active Codex namespace and refuse.
 *           Session totals may span model switches. Counter semantics
 *           are session_cumulative_latest; events count generations and by_hour
 *           assigns the whole snapshot to its observation hour. This is not
 *           interval usage, billed cost, or task attribution.
 *   uncached_input_tokens is derived per event so both formats compare:
 *   muse/codex input - cache_read, claude input.
 *
 * COUNTERS. A missing, negative or non-numeric counter is UNREPORTED: it is
 * never summed as 0, and each output row carries unreported[field] = the
 * number of its events that did not report it. An event with no id is
 * counted in `unkeyed` and not summed, since it cannot be deduplicated.
 *
 * OUTPUT. reply data gains "usage": {usage_log, files, lines, usage_lines,
 * malformed (a physical line containing NUL or invalid UTF-8, or a line
 * mentioning usage that is not one JSON object),
 * unreadable, unkeyed, events (distinct ids kept), duplicate_lines,
 * truncated, by_model:[{format, model, input_includes_cache_read, events,
 * <counters>, unreported:{...}}], hours_total, by_hour: newest
 * DVU_MAX_HOURS {hour, format, model, events, <counters>}}.
 *
 * FAILURE. usage_log present but not a nonempty UTF-8 string is BAD_INPUT; a path
 * that does not exist is USAGE_LOG_NOT_FOUND; one that is neither a file
 * nor a directory is BAD_INPUT. Unreadable files inside a directory are
 * counted, never fatal. JSON construction failure is ALLOC and clears reply
 * data rather than exposing a partial summary. Invalid derived UTF-8 also
 * refuses summary construction. Counter-total overflow is USAGE_OVERFLOW
 * and leaves reply data an empty object. The reader runs no process
 * and writes nothing.
 *
 * Tests: tests/harness/src/test_devagent_outcomes.c, usage section.
 */

#include "command/native_devagent_usage_internal.h"

#include "base/safe_alloc.h"
#include "zutf8/zutf8.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef ZCL_TESTING
static _Thread_local struct dvu_test_ops dvu_test_calls;

void dvu_test_set_ops(const struct dvu_test_ops *ops)
{
    dvu_test_calls = ops ? *ops : (struct dvu_test_ops){0};
}

/* Defaults call the same JSON and sort functions as a non-test build. */
#define qsort(...) \
    (dvu_test_calls.sort ? dvu_test_calls.sort(__VA_ARGS__) : qsort(__VA_ARGS__))
#define json_push_kv(...) \
    (dvu_test_calls.kv ? dvu_test_calls.kv(__VA_ARGS__) : json_push_kv(__VA_ARGS__))
#define json_push_back(...) \
    (dvu_test_calls.back ? dvu_test_calls.back(__VA_ARGS__) : json_push_back(__VA_ARGS__))
#define json_push_kv_str(...) \
    (dvu_test_calls.str ? dvu_test_calls.str(__VA_ARGS__) : json_push_kv_str(__VA_ARGS__))
#define json_push_kv_int(...) \
    (dvu_test_calls.integer ? dvu_test_calls.integer(__VA_ARGS__) : json_push_kv_int(__VA_ARGS__))
#define json_push_kv_bool(...) \
    (dvu_test_calls.boolean ? dvu_test_calls.boolean(__VA_ARGS__) : json_push_kv_bool(__VA_ARGS__))
#endif

#define DVU_MAX_DEPTH 8
#define DVU_MAX_FILES 50000
#define DVU_MAX_EVENTS 1000000u
#define DVU_MAX_HOURS 48u
#define DVU_HOUR_LEN 13u /* YYYY-MM-DDTHH */
#define DVU_PATH_MAX 4096u

enum dvu_field {
    DVU_IN,
    DVU_OUT,
    DVU_CREAD,
    DVU_CWRITE,
    DVU_REASON,
    DVU_UNCACHED, /* derived: comparable across formats */
    DVU_NF
};

static const char *const dvu_field_names[DVU_NF] = {
    "input_tokens",       "output_tokens",    "cache_read_tokens",
    "cache_write_tokens", "reasoning_tokens", "uncached_input_tokens",
};

enum dvu_format { DVU_MUSE, DVU_CLAUDE, DVU_CODEX };

static const char *const dvu_format_names[] = {"muse", "claude", "codex"};

/* One parsed line: pointers into the parsed row, copied on keep. */
struct dvu_fields {
    enum dvu_format format;
    const char *id;
    const char *model;
    char hour[DVU_HOUR_LEN + 1];
    int64_t v[DVU_NF];
    int64_t ordinal;
};

struct dvu_event {
    char *id;
    char *model;
    char hour[DVU_HOUR_LEN + 1];
    enum dvu_format format;
    size_t seq; /* input order: the latest line for an id wins */
    int64_t v[DVU_NF];
    int64_t ordinal;
};

struct dvu_scan {
    struct dvu_event *ev;
    size_t n;
    size_t cap;
    size_t seq;
    int64_t files;
    int64_t lines;
    int64_t usage_lines;
    int64_t malformed;
    int64_t unreadable;
    int64_t unkeyed;
    bool truncated;
    bool alloc_failed;
    bool sum_overflow;
    bool conflict;
    const char *model;
    const char *since;
};

struct dvu_sum {
    int64_t events;
    int64_t v[DVU_NF];
    int64_t unreported[DVU_NF];
};

static void dvu_fail(struct zcl_command_reply *reply, enum zcl_command_exit exit_code,
                     const char *code, const char *message)
{
    (void)fprintf(stderr, "dev.agent.outcomes: %s: %s\n", code, message);
    zcl_command_reply_fail(reply, ZCL_COMMAND_STATUS_FAILED, exit_code, code,
                           "usage", false, false, message,
                           "dev.agent.outcomes usage_log reader");
}

/* ── parsing ─────────────────────────────────────────────────────────── */

static const struct json_value *dvu_get(const struct json_value *obj,
                                        const char *key)
{
    return obj && obj->type == JSON_OBJ ? json_get(obj, key) : NULL;
}

static const char *dvu_str(const struct json_value *obj, const char *key)
{
    const struct json_value *v = dvu_get(obj, key);
    return v && v->type == JSON_STR ? json_get_str(v) : NULL;
}

/* A reported counter is a non-negative number; anything else is -1. */
static int64_t dvu_counter(const struct json_value *obj, const char *key)
{
    const struct json_value *v = dvu_get(obj, key);
    if (v && v->type == JSON_INT)
        return json_get_int(v) >= 0 ? json_get_int(v) : -1;
    if (v && v->type == JSON_REAL) {
        double d = json_get_real(v);
        return d >= 0.0 && d < 9.0e18 ? (int64_t)d : -1;
    }
    return -1;
}

static void dvu_hour_from_us(int64_t us, char out[DVU_HOUR_LEN + 1])
{
    time_t secs = (time_t)(us / 1000000);
    struct tm tm_utc;
#if defined(_WIN32)
    bool conv = gmtime_s(&tm_utc, &secs) == 0;
#else
    bool conv = gmtime_r(&secs, &tm_utc) != NULL;
#endif
    if (us <= 0 || !conv ||
        strftime(out, DVU_HOUR_LEN + 1, "%Y-%m-%dT%H", &tm_utc) !=
            DVU_HOUR_LEN)
        (void)snprintf(out, DVU_HOUR_LEN + 1, "unknown");
}

static void dvu_hour_from_iso(const char *ts, char out[DVU_HOUR_LEN + 1])
{
    if (ts && strlen(ts) >= DVU_HOUR_LEN && ts[4] == '-' && ts[10] == 'T')
        (void)snprintf(out, DVU_HOUR_LEN + 1, "%.13s", ts);
    else
        (void)snprintf(out, DVU_HOUR_LEN + 1, "unknown");
}

static bool dvu_parse_muse(const struct json_value *row, struct dvu_fields *f)
{
    const struct json_value *ev = dvu_get(dvu_get(row, "payload"), "event");
    const char *kind = dvu_str(ev, "kind");
    if (!kind || strcmp(kind, "model_completed") != 0)
        return false;
    const struct json_value *u = dvu_get(ev, "usage");
    const struct json_value *at = dvu_get(row, "recorded_at");
    f->format = DVU_MUSE;
    f->id = dvu_str(row, "id");
    f->model = dvu_str(ev, "model");
    dvu_hour_from_us(at && at->type == JSON_INT ? json_get_int(at) : 0, f->hour);
    f->v[DVU_IN] = dvu_counter(u, "input_tokens");
    f->v[DVU_OUT] = dvu_counter(u, "output_tokens");
    f->v[DVU_CREAD] = dvu_counter(u, "cache_read_tokens");
    f->v[DVU_CWRITE] = dvu_counter(u, "cache_write_tokens");
    f->v[DVU_REASON] = dvu_counter(u, "reasoning_tokens");
    f->v[DVU_UNCACHED] = f->v[DVU_IN] >= 0 && f->v[DVU_CREAD] >= 0 &&
                                 f->v[DVU_IN] >= f->v[DVU_CREAD]
                             ? f->v[DVU_IN] - f->v[DVU_CREAD]
                             : -1;
    return true;
}

static bool dvu_parse_claude(const struct json_value *row, struct dvu_fields *f)
{
    const char *type = dvu_str(row, "type");
    const struct json_value *msg = dvu_get(row, "message");
    const struct json_value *u = dvu_get(msg, "usage");
    if (!type || strcmp(type, "assistant") != 0 || !u || u->type != JSON_OBJ)
        return false;
    f->format = DVU_CLAUDE;
    f->id = dvu_str(msg, "id");
    f->model = dvu_str(msg, "model");
    dvu_hour_from_iso(dvu_str(row, "timestamp"), f->hour);
    f->v[DVU_IN] = dvu_counter(u, "input_tokens");
    f->v[DVU_OUT] = dvu_counter(u, "output_tokens");
    f->v[DVU_CREAD] = dvu_counter(u, "cache_read_input_tokens");
    f->v[DVU_CWRITE] = dvu_counter(u, "cache_creation_input_tokens");
    f->v[DVU_REASON] = dvu_counter(dvu_get(u, "output_tokens_details"),
                                   "thinking_tokens");
    f->v[DVU_UNCACHED] = f->v[DVU_IN];
    return true;
}

/* Account and rollout generation identify the cumulative counter namespace.
 * Parent-thread session_id and the current model do not identify it. */
static bool dvu_codex_identity(const struct json_value *row, char id[300])
{
    const struct json_value *p = dvu_get(row, "payload");
    const char *account = dvu_str(p, "creator_account_id");
    const char *generation = dvu_str(p, "id");
    if (!account || !generation || !account[0] || !generation[0] ||
        strlen(account) > 128 || strlen(generation) > 128)
        return false;
    int n = snprintf(id, 300, "%zu:%s%zu:%s", strlen(account), account,
                     strlen(generation), generation);
    return n > 0 && n < 300;
}

static int64_t dvu_codex_counter(const struct json_value *obj, const char *key)
{
    const struct json_value *v = dvu_get(obj, key);
    return v && v->type == JSON_INT && json_get_int(v) >= 0 ? json_get_int(v) : -1;
}

static bool dvu_parse_codex(const struct json_value *row, const char *id,
                            struct dvu_fields *f)
{
    const char *type = dvu_str(row, "type");
    const struct json_value *p = dvu_get(row, "payload");
    const char *kind = dvu_str(p, "type");
    const struct json_value *u = dvu_get(dvu_get(p, "info"), "total_token_usage");
    if (!type || strcmp(type, "event_msg") != 0 || !kind ||
        strcmp(kind, "token_count") != 0 || !u || u->type != JSON_OBJ)
        return false;
    const struct json_value *ordinal = dvu_get(row, "ordinal");
    f->ordinal = ordinal && ordinal->type == JSON_INT ? json_get_int(ordinal) : -1;
    f->format = DVU_CODEX;
    f->id = f->ordinal >= 0 ? id : NULL;
    dvu_hour_from_iso(dvu_str(row, "timestamp"), f->hour);
    f->v[DVU_IN] = dvu_codex_counter(u, "input_tokens");
    f->v[DVU_OUT] = dvu_codex_counter(u, "output_tokens");
    f->v[DVU_CREAD] = dvu_codex_counter(u, "cached_input_tokens");
    f->v[DVU_CWRITE] = dvu_codex_counter(u, "cache_write_input_tokens");
    f->v[DVU_REASON] = dvu_codex_counter(u, "reasoning_output_tokens");
    f->v[DVU_UNCACHED] = f->v[DVU_IN] >= f->v[DVU_CREAD] && f->v[DVU_CREAD] >= 0
                               ? f->v[DVU_IN] - f->v[DVU_CREAD] : -1;
    return true;
}

/* ── collection ──────────────────────────────────────────────────────── */

static bool dvu_filtered_out(const struct dvu_scan *s, const struct dvu_fields *f)
{
    if (s->model && (!f->model || strcmp(f->model, s->model) != 0))
        return true;
    return s->since && (strcmp(f->hour, "unknown") == 0 ||
                        strncmp(f->hour, s->since, DVU_HOUR_LEN) < 0);
}

static void dvu_keep(struct dvu_scan *s, const struct dvu_fields *f)
{
    if (!f->id || !f->id[0]) {
        s->unkeyed++;
        return;
    }
    if (f->format != DVU_CODEX && dvu_filtered_out(s, f))
        return;
    if (s->n >= DVU_MAX_EVENTS) {
        s->truncated = true;
        return;
    }
    if (s->n == s->cap) {
        size_t ncap = s->cap ? s->cap * 2u : 256u;
        struct dvu_event *t = zcl_realloc(s->ev, ncap * sizeof(*t),
                                          "devagent_usage_events");
        if (!t) {
            s->alloc_failed = true;
            return;
        }
        s->ev = t;
        s->cap = ncap;
    }
    struct dvu_event *e = &s->ev[s->n];
    e->id = zcl_strdup(f->id, "devagent_usage_id");
    e->model = zcl_strdup(f->model ? f->model : "", "devagent_usage_model");
    if (!e->id || !e->model) {
        free(e->id);
        free(e->model);
        s->alloc_failed = true;
        return;
    }
    memcpy(e->hour, f->hour, sizeof(e->hour));
    memcpy(e->v, f->v, sizeof(e->v));
    e->format = f->format;
    e->seq = s->seq++;
    e->ordinal = f->ordinal;
    s->n++;
}

static void dvu_codex_meta(struct dvu_scan *s, char codex_id[300],
                           const struct json_value *row)
{
    const char *type = dvu_str(row, "type");
    if (!type || strcmp(type, "session_meta") != 0)
        return;
    char id[300] = {0};
    (void)dvu_codex_identity(row, id);
    const struct json_value *p = dvu_get(row, "payload");
    const char *generation = dvu_str(p, "id");
    const char *session = dvu_str(p, "session_id");
    const struct json_value *parent = dvu_get(p, "parent_thread_id");
    if (!generation || !session || strcmp(generation, session) != 0 ||
        (parent && parent->type != JSON_NULL)) {
        s->conflict = true;
        id[0] = 0;
    }
    if (codex_id[0] && strcmp(codex_id, id))
        s->conflict = true;
    memcpy(codex_id, id, sizeof(id));
}

static void dvu_malformed(struct dvu_scan *s, char codex_id[300])
{
    s->malformed++;
    if (codex_id[0])
        s->conflict = true;
    codex_id[0] = 0;
}

static void dvu_line(struct dvu_scan *s, char codex_id[300], const char *line,
                      size_t len)
{
    s->lines++;
    if (memchr(line, 0, len) || !zutf8_validate_n(line, len)) {
        dvu_malformed(s, codex_id);
        return;
    }
    if (!codex_id[0] && !strstr(line, "\"usage\"") && !strstr(line, "\"token_count\"") &&
        !strstr(line, "\"session_meta\""))
        return;
    struct json_value row;
    json_init(&row);
    if (!json_read(&row, line, len) || row.type != JSON_OBJ) {
        dvu_malformed(s, codex_id);
        json_free(&row);
        return;
    }
    struct dvu_fields f;
    memset(&f, 0, sizeof(f));
    dvu_codex_meta(s, codex_id, &row);
    if (dvu_parse_muse(&row, &f) || dvu_parse_claude(&row, &f) ||
        dvu_parse_codex(&row, codex_id, &f)) {
        s->usage_lines++;
        dvu_keep(s, &f);
    }
    json_free(&row);
}

/* Portable whole-line reader (not every target has getline): grows one
 * buffer until the line's newline or EOF. False at EOF or on OOM. */
struct dvu_linebuf {
    char *buf;
    size_t len;
    size_t cap;
};

static bool dvu_next_line(FILE *fp, struct dvu_linebuf *lb, bool *oom)
{
    lb->len = 0;
    for (;;) {
        if (lb->cap - lb->len < 4096u) {
            if (lb->cap > SIZE_MAX / 2u) {
                *oom = true;
                return false;
            }
            size_t ncap = lb->cap ? lb->cap * 2u : 65536u;
            char *t = zcl_realloc(lb->buf, ncap, "devagent_usage_line");
            if (!t) {
                *oom = true;
                return false;
            }
            lb->buf = t;
            lb->cap = ncap;
        }
        int c = fgetc(fp);
        if (c == EOF)
            return lb->len > 0;
        lb->buf[lb->len++] = (char)c;
        lb->buf[lb->len] = 0;
        if (c == '\n')
            return true;
    }
}

static void dvu_read_file(struct dvu_scan *s, const char *path)
{
    if (s->files >= DVU_MAX_FILES) {
        s->truncated = true;
        return;
    }
    FILE *fp = fopen(path, "r");
    if (!fp) {
        s->unreadable++;
        return;
    }
    s->files++;
    struct dvu_linebuf lb = {0};
    char codex_id[300] = {0};
    while (!s->alloc_failed && dvu_next_line(fp, &lb, &s->alloc_failed))
        dvu_line(s, codex_id, lb.buf, lb.len);
    if (ferror(fp))
        s->unreadable++;
    free(lb.buf);
    (void)fclose(fp);
}

static bool dvu_is_jsonl(const char *name)
{
    size_t len = strlen(name);
    return len > 6u && strcmp(name + len - 6u, ".jsonl") == 0;
}

/* Never follow a symlink out of the walked tree; Windows has no lstat. */
static int dvu_lstat(const char *path, struct stat *st)
{
#if defined(_WIN32)
    return stat(path, st);
#else
    return lstat(path, st);
#endif
}

static void dvu_walk(struct dvu_scan *s, const char *dir, int depth)
{
    DIR *d = opendir(dir);
    if (!d) {
        s->unreadable++;
        return;
    }
    struct dirent *e;
    char path[DVU_PATH_MAX];
    while (!s->alloc_failed && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        struct stat st;
        int n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (n < 0 || (size_t)n >= sizeof(path) || dvu_lstat(path, &st) != 0) {
            s->unreadable++;
        } else if (S_ISDIR(st.st_mode)) {
            if (depth < DVU_MAX_DEPTH)
                dvu_walk(s, path, depth + 1);
            else
                s->truncated = true;
        } else if (S_ISREG(st.st_mode) && dvu_is_jsonl(e->d_name)) {
            dvu_read_file(s, path);
        }
    }
    (void)closedir(d);
}

/* ── dedup and aggregation ───────────────────────────────────────────── */

static int dvu_cmp_id(const void *a, const void *b)
{
    const struct dvu_event *x = a, *y = b;
    if (x->format != y->format)
        return (int)x->format - (int)y->format;
    int c = strcmp(x->id, y->id);
    if (c)
        return c;
    if (x->format == DVU_CODEX && x->ordinal != y->ordinal)
        return (x->ordinal > y->ordinal) - (x->ordinal < y->ordinal);
    return (x->seq > y->seq) - (x->seq < y->seq);
}

static bool dvu_same_id(const struct dvu_event *a, const struct dvu_event *b)
{
    return a->format == b->format && strcmp(a->id, b->id) == 0;
}

static bool dvu_codex_conflict(const struct dvu_event *a, const struct dvu_event *b)
{
    if (a->format != DVU_CODEX)
        return false;
    if (a->ordinal == b->ordinal && strcmp(a->hour, b->hour) != 0)
        return true;
    /* A reset, changed coverage, or conflicting checkpoint cannot be folded. */
    for (int f = 0; f < DVU_NF; f++) {
        if ((a->v[f] < 0) != (b->v[f] < 0) || b->v[f] < a->v[f] ||
            (a->ordinal == b->ordinal && a->v[f] != b->v[f]))
            return true;
    }
    return false;
}

static bool dvu_codex_filtered(const struct dvu_scan *s, const struct dvu_event *e)
{
    if (e->format != DVU_CODEX)
        return false;
    struct dvu_fields f = {.model = e->model};
    memcpy(f.hour, e->hour, sizeof(f.hour));
    return dvu_filtered_out(s, &f);
}

/* Keep the latest line for each id; returns the number of dropped lines. */
static int64_t dvu_dedup(struct dvu_scan *s)
{
    if (s->n == 0)
        return 0;
    qsort(s->ev, s->n, sizeof(*s->ev), dvu_cmp_id);
    size_t out = 0;
    int64_t dropped = 0;
    for (size_t i = 0; i < s->n; i++) {
        bool last = i + 1 == s->n || !dvu_same_id(&s->ev[i], &s->ev[i + 1]);
        if (!last && dvu_codex_conflict(&s->ev[i], &s->ev[i + 1]))
            s->conflict = true;
        if (last && !dvu_codex_filtered(s, &s->ev[i])) {
            s->ev[out++] = s->ev[i];
        } else {
            if (!last)
                dropped++;
            free(s->ev[i].id);
            free(s->ev[i].model);
        }
    }
    s->n = out;
    return dropped;
}

static int dvu_cmp_model(const void *a, const void *b)
{
    const struct dvu_event *x = a, *y = b;
    if (x->format != y->format)
        return (int)x->format - (int)y->format;
    return strcmp(x->model, y->model);
}

/* Newest hour first, then format, then model. */
static int dvu_cmp_hour(const void *a, const void *b)
{
    const struct dvu_event *x = a, *y = b;
    int c = strcmp(y->hour, x->hour);
    return c ? c : dvu_cmp_model(a, b);
}

static bool dvu_sum_add(struct dvu_sum *t, const struct dvu_event *e)
{
    t->events++;
    for (int f = 0; f < DVU_NF; f++) {
        if (e->v[f] < 0)
            t->unreported[f]++;
        else {
            if (t->v[f] > INT64_MAX - e->v[f])
                return false;
            t->v[f] += e->v[f];
        }
    }
    return true;
}

/* JSON insertion may report success after an incomplete recursive copy.
 * Check the copied tree too; no partial summary is accepted on that path. */
static bool dvu_json_equal(const struct json_value *a,
                            const struct json_value *b);

static bool dvu_json_children_equal(const struct json_value *a,
                                     const struct json_value *b)
{
    if (a->num_children != b->num_children)
        return false;
    for (size_t k = 0; k < a->num_children; k++) {
        if (a->type == JSON_OBJ) {
            if (!b->keys[k] || strcmp(a->keys[k], b->keys[k]) != 0)
                return false;
        }
        if (!dvu_json_equal(&a->children[k], &b->children[k]))
            return false;
    }
    return true;
}

static bool dvu_json_equal(const struct json_value *a,
                            const struct json_value *b)
{
    if (a->type != b->type)
        return false;
    switch (a->type) {
    case JSON_STR:
        return b->val.s && strcmp(a->val.s, b->val.s) == 0;
    case JSON_INT:
        return a->val.i == b->val.i;
    case JSON_BOOL:
        return a->val.b == b->val.b;
    case JSON_OBJ:
    case JSON_ARR:
        return dvu_json_children_equal(a, b);
    default:
        return false;
    }
}

static bool dvu_json_kv(struct json_value *obj, const char *key,
                         const struct json_value *value)
{
    size_t n = obj->num_children;
    return json_push_kv(obj, key, value) && obj->num_children > n &&
           dvu_json_equal(value, &obj->children[n]);
}

static bool dvu_json_back(struct json_value *arr,
                           const struct json_value *value)
{
    size_t n = arr->num_children;
    return json_push_back(arr, value) && arr->num_children > n &&
           dvu_json_equal(value, &arr->children[n]);
}

static bool dvu_json_str(struct json_value *obj, const char *key,
                          const char *text)
{
    if (!text || !zutf8_validate(text))
        return false;
    size_t n = obj->num_children;
    if (!json_push_kv_str(obj, key, text) || obj->num_children <= n)
        return false;
    const struct json_value *v = &obj->children[n];
    return v->type == JSON_STR && v->val.s && strcmp(v->val.s, text) == 0;
}

static bool dvu_push_sum(struct json_value *row, const struct dvu_sum *t,
                         bool with_unreported)
{
    if (!json_push_kv_int(row, "events", t->events))
        return false;
    for (int f = 0; f < DVU_NF; f++)
        if (!json_push_kv_int(row, dvu_field_names[f], t->v[f]))
            return false;
    if (!with_unreported)
        return true;
    struct json_value un;
    json_init(&un);
    json_set_object(&un);
    bool ok = true;
    for (int f = 0; ok && f < DVU_NF; f++)
        ok = json_push_kv_int(&un, dvu_field_names[f], t->unreported[f]);
    ok = ok && dvu_json_kv(row, "unreported", &un);
    json_free(&un);
    return ok;
}

static bool dvu_push_group_head(struct json_value *row, const struct dvu_event *e)
{
    return dvu_json_str(row, "format", dvu_format_names[e->format]) &&
           dvu_json_str(row, "model", e->model) &&
           dvu_json_str(row, "counter_semantics", e->format == DVU_CODEX
                           ? "session_cumulative_latest" : "event_latest");
}

static bool dvu_push_by_model(struct json_value *usage, struct dvu_scan *s)
{
    struct json_value arr;
    json_init(&arr);
    json_set_array(&arr);
    if (s->n > 1)
        qsort(s->ev, s->n, sizeof(*s->ev), dvu_cmp_model);
    bool ok = true;
    for (size_t i = 0; ok && i < s->n;) {
        struct dvu_sum t;
        memset(&t, 0, sizeof(t));
        size_t j = i;
        while (ok && j < s->n && dvu_cmp_model(&s->ev[i], &s->ev[j]) == 0)
            ok = dvu_sum_add(&t, &s->ev[j++]);
        if (!ok) {
            s->sum_overflow = true;
            break;
        }
        struct json_value row;
        json_init(&row);
        json_set_object(&row);
        ok = dvu_push_group_head(&row, &s->ev[i]) &&
             json_push_kv_bool(&row, "input_includes_cache_read",
                               s->ev[i].format != DVU_CLAUDE) &&
             dvu_push_sum(&row, &t, true) && dvu_json_back(&arr, &row);
        json_free(&row);
        i = j;
    }
    ok = ok && dvu_json_kv(usage, "by_model", &arr);
    json_free(&arr);
    return ok;
}

static bool dvu_push_by_hour(struct json_value *usage, struct dvu_scan *s)
{
    struct json_value arr;
    json_init(&arr);
    json_set_array(&arr);
    if (s->n > 1)
        qsort(s->ev, s->n, sizeof(*s->ev), dvu_cmp_hour);
    int64_t groups = 0;
    bool ok = true;
    for (size_t i = 0; ok && i < s->n;) {
        struct dvu_sum t;
        memset(&t, 0, sizeof(t));
        size_t j = i;
        while (ok && j < s->n && dvu_cmp_hour(&s->ev[i], &s->ev[j]) == 0)
            ok = dvu_sum_add(&t, &s->ev[j++]);
        if (!ok) {
            s->sum_overflow = true;
            break;
        }
        if ((size_t)groups++ < DVU_MAX_HOURS) {
            struct json_value row;
            json_init(&row);
            json_set_object(&row);
            ok = dvu_json_str(&row, "hour", s->ev[i].hour) &&
                 dvu_push_group_head(&row, &s->ev[i]) &&
                 dvu_push_sum(&row, &t, false) && dvu_json_back(&arr, &row);
            json_free(&row);
        }
        i = j;
    }
    ok = ok && json_push_kv_int(usage, "hours_total", groups) &&
         dvu_json_kv(usage, "by_hour", &arr);
    json_free(&arr);
    return ok;
}

static void dvu_scan_free(struct dvu_scan *s)
{
    for (size_t i = 0; i < s->n; i++) {
        free(s->ev[i].id);
        free(s->ev[i].model);
    }
    free(s->ev);
}

static bool dvu_push_counts(struct json_value *usage, const struct dvu_scan *s,
                            int64_t duplicates)
{
    return json_push_kv_int(usage, "files", s->files) &&
           json_push_kv_int(usage, "lines", s->lines) &&
           json_push_kv_int(usage, "usage_lines", s->usage_lines) &&
           json_push_kv_int(usage, "malformed", s->malformed) &&
           json_push_kv_int(usage, "unreadable", s->unreadable) &&
           json_push_kv_int(usage, "unkeyed", s->unkeyed) &&
           json_push_kv_int(usage, "events", (int64_t)s->n) &&
           json_push_kv_int(usage, "duplicate_lines", duplicates) &&
           json_push_kv_bool(usage, "truncated", s->truncated);
}

/* ── entry ───────────────────────────────────────────────────────────── */

/* The usage_log path; NULL when the key is absent (nothing to do) or after
 * failing the reply. */
static const char *dvu_path(const struct json_value *input,
                            struct zcl_command_reply *reply, struct stat *st)
{
    const struct json_value *v = dvu_get(input, "usage_log");
    if (!v)
        return NULL;
    const char *path = v->type == JSON_STR ? json_get_str(v) : NULL;
    if (!path || !path[0] || !zutf8_validate(path)) {
        dvu_fail(reply, ZCL_COMMAND_EXIT_INVALID, "BAD_INPUT",
                 "input key 'usage_log' must be a nonempty UTF-8 path");
        return NULL;
    }
    char msg[DVU_PATH_MAX + 96];
    if (stat(path, st) != 0) {
        (void)snprintf(msg, sizeof(msg), "usage_log '%s' cannot be read: %s",
                       path, strerror(errno));
        dvu_fail(reply, ZCL_COMMAND_EXIT_INVALID,
                 errno == ENOENT ? "USAGE_LOG_NOT_FOUND" : "USAGE_LOG_UNREADABLE",
                 msg);
        return NULL;
    }
    if (!S_ISDIR(st->st_mode) && !S_ISREG(st->st_mode)) {
        (void)snprintf(msg, sizeof(msg),
                       "usage_log '%s' is neither a file nor a directory", path);
        dvu_fail(reply, ZCL_COMMAND_EXIT_INVALID, "BAD_INPUT", msg);
        return NULL;
    }
    return path;
}

void dvu_push_usage(const struct json_value *input, const char *model,
                    const char *since, struct zcl_command_reply *reply)
{
    struct stat st;
    const char *path = reply ? dvu_path(input, reply, &st) : NULL;
    if (!path)
        return;
    struct dvu_scan s;
    memset(&s, 0, sizeof(s));
    s.model = model;
    s.since = since;
    if (S_ISDIR(st.st_mode))
        dvu_walk(&s, path, 0);
    else
        dvu_read_file(&s, path);
    if (s.alloc_failed) {
        dvu_scan_free(&s);
        dvu_fail(reply, ZCL_COMMAND_EXIT_INTERNAL, "ALLOC",
                 "out of memory while reading usage_log");
        return;
    }
    int64_t duplicates = dvu_dedup(&s);
    if (s.conflict) {
        dvu_scan_free(&s);
        json_free(&reply->data);
        json_set_object(&reply->data);
        dvu_fail(reply, ZCL_COMMAND_EXIT_INVALID, "USAGE_CONFLICT",
                 "usage_log Codex lineage, namespace or cumulative checkpoints conflict");
        return;
    }
    struct json_value usage;
    json_init(&usage);
    json_set_object(&usage);
    bool ok = dvu_json_str(&usage, "usage_log", path) &&
              dvu_push_counts(&usage, &s, duplicates) &&
              dvu_push_by_model(&usage, &s) &&
              dvu_push_by_hour(&usage, &s) &&
              dvu_json_kv(&reply->data, "usage", &usage);
    json_free(&usage);
    dvu_scan_free(&s);
    if (!ok) {
        json_free(&reply->data);
        json_set_object(&reply->data);
        dvu_fail(reply, ZCL_COMMAND_EXIT_INTERNAL,
                 s.sum_overflow ? "USAGE_OVERFLOW" : "ALLOC",
                 s.sum_overflow ? "usage_log counter total exceeds INT64_MAX"
                                : "usage_log summary construction failed (allocation or invalid UTF-8)");
    }
}
