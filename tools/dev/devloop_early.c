/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Early feedback stage of the resident restart proof: resolve the facts-narrowed plan, run its groups on the new candidate bytes before the full plan, and report them as feedback only. */
#include "devloop_early.h"

#include "devloop_facts.h"
#include "test_group_catalog.h"

#include "base/hex.h"
#include "crypto/sha256.h"
#include "json/json.h"
#include "platform/time_compat.h"
#include "util/safe_alloc.h"
#include "vcs/semantic_manifest.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define EARLY_SOURCE_MAX (16u * 1024u * 1024u)
#define EARLY_TIMEOUT_MS 300000
/* The groups list rides in the event only while it stays this small; the
 * event wire is bounded and its digest always names the set. */
#define EARLY_GROUPS_INLINE_MAX 512u

static atomic_int_fast64_t g_early_edit_seen_us;

void zcl_devloop_early_note_edit(int64_t seen_us)
{
    atomic_store(&g_early_edit_seen_us, seen_us > 0 ? seen_us : 0);
}

int64_t zcl_devloop_early_edit_seen_us(void)
{
    return (int64_t)atomic_load(&g_early_edit_seen_us);
}

static void early_plan_refuse(struct zcl_devloop_early_plan *out,
                              const char *reason, const char *detail)
{
    out->ready = false;
    out->skip_reason = reason;
    (void)snprintf(out->detail, sizeof(out->detail), "%s",
                   detail ? detail : "");
}

static bool early_dir_confined(const char *dir)
{
    return dir[0] && dir[0] != '/' && dir[0] != '\\' && !strstr(dir, "..") &&
           strlen(dir) < ZCL_DEVLOOP_PATH_MAX;
}

static bool early_is_directory(const char *root, const char *dir)
{
    char path[ZCL_DEVLOOP_PATH_MAX * 2 + 2];
    struct stat st;
    if (snprintf(path, sizeof(path), "%s/%s", root, dir) >= (int)sizeof(path))
        return false;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void early_tu_unload(struct zcl_devloop_facts_tu *tu)
{
    free((void *)tu->before);
    free((void *)tu->after);
    free((void *)tu->before_src);
    free((void *)tu->after_src);
    memset(tu, 0, sizeof(*tu));
}

/* The same evidence dev.change.plan "facts" reads for one changed .c: its
 * before/after manifests, its before source, and the working-tree source. */
static bool early_tu_load(const char *root, const char *dir, const char *file,
                          struct zcl_devloop_facts_tu *tu)
{
    uint8_t *b = NULL, *a = NULL, *bs = NULL, *as = NULL;
    bool ok = zcl_devloop_facts_read(root, dir, file, ".before.zsm",
                                     VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &b,
                                     &tu->before_len) &&
              zcl_devloop_facts_read(root, dir, file, ".after.zsm",
                                     VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &a,
                                     &tu->after_len) &&
              zcl_devloop_facts_read(root, dir, file, ".before",
                                     EARLY_SOURCE_MAX, &bs,
                                     &tu->before_src_len) &&
              zcl_devloop_facts_read(root, NULL, file, "", EARLY_SOURCE_MAX,
                                     &as, &tu->after_src_len);
    tu->source = file;
    tu->before = b;
    tu->after = a;
    tu->before_src = bs;
    tu->after_src = as;
    if (!ok)
        early_tu_unload(tu);
    return ok;
}

/* after-stale and lookup-unbound are the consumer's words for evidence the
 * tree no longer matches. */
static const char *early_verdict_reason(const char *reason)
{
    if (reason && (strcmp(reason, "after-stale") == 0 ||
                   strcmp(reason, "lookup-unbound") == 0))
        return "facts_stale";
    return "facts_not_narrowed";
}

static void early_plan_consume(const char *root, const char *const *sources,
                               size_t n, struct zcl_devloop_facts_tu *tus,
                               struct zcl_devloop_early_plan *out)
{
    struct zcl_devloop_facts_verdict *v = zcl_malloc(sizeof(*v), "early.v");
    struct zcl_devloop_facts_report report = {0};
    if (!v) {
        early_plan_refuse(out, "out_of_memory", "facts verdict");
        return;
    }
    bool ok = zcl_devloop_plan_files(sources, n, &out->plan) &&
              zcl_devloop_facts_consume(root, sources, n, out->facts_dir, tus,
                                        &out->plan, v, &report);
    if (!ok)
        early_plan_refuse(out, "facts_consume_failed",
                          "the facts consumer refused the changed set");
    else if (!v->narrowed)
        early_plan_refuse(out, early_verdict_reason(v->reason),
                          v->reason && v->reason[0] ? v->reason : v->detail);
    else {
        out->ready = true;
        out->skip_reason = "";
        (void)snprintf(out->detail, sizeof(out->detail),
                       "%zu seeds reached %zu files", v->seeds_total,
                       v->reached_files);
    }
    zcl_devloop_facts_report_free(&report);
    free(v);
}

static void early_plan_load_and_consume(const char *root,
                                        const char *const *sources, size_t n,
                                        struct zcl_devloop_early_plan *out)
{
    struct zcl_devloop_facts_tu *tus = zcl_calloc(n, sizeof(*tus),
                                                  "early.tus");
    if (!tus) {
        early_plan_refuse(out, "out_of_memory", "facts evidence");
        return;
    }
    size_t loaded = 0;
    while (loaded < n &&
           early_tu_load(root, out->facts_dir, sources[loaded], &tus[loaded]))
        loaded++;
    if (loaded < n)
        early_plan_refuse(out, "facts_evidence_missing", sources[loaded]);
    else
        early_plan_consume(root, sources, n, tus, out);
    for (size_t i = 0; i < loaded; i++)
        early_tu_unload(&tus[i]);
    free(tus);
}

bool zcl_devloop_early_plan_facts(const char *root,
                                  const char *const *sources, size_t n,
                                  const char *facts_dir, int64_t origin_us,
                                  struct zcl_devloop_early_plan *out)
{
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    out->origin_us = origin_us;
    early_plan_refuse(out, "facts_dir_unset", "");
    if (!root || !sources || n == 0 || n > ZCL_DEVLOOP_MAX_FILES)
        return false;
    int64_t started = platform_time_monotonic_us();
    if (!facts_dir || !facts_dir[0])
        return true;
    (void)snprintf(out->facts_dir, sizeof(out->facts_dir), "%s", facts_dir);
    if (!early_dir_confined(facts_dir))
        early_plan_refuse(out, "facts_dir_invalid", facts_dir);
    else if (!early_is_directory(root, facts_dir))
        early_plan_refuse(out, "facts_dir_missing", facts_dir);
    else
        early_plan_load_and_consume(root, sources, n, out);
    out->plan_us = platform_time_monotonic_us() - started;
    return true;
}

struct zcl_devloop_early_plan *zcl_devloop_early_plan_resident(
    const char *root, const char *const *sources, size_t n)
{
    struct zcl_devloop_early_plan *plan = zcl_malloc(sizeof(*plan),
                                                     "early.plan");
    if (plan)
        (void)zcl_devloop_early_plan_facts(
            root, sources, n, getenv(ZCL_DEVLOOP_EARLY_FACTS_ENV),
            zcl_devloop_early_edit_seen_us(), plan);
    return plan;
}

void zcl_devloop_early_skip(struct zcl_devloop_early_receipt *r,
                            const char *reason, const char *detail)
{
    r->status = "skipped";
    r->reason = reason;
    (void)snprintf(r->detail, sizeof(r->detail), "%s", detail ? detail : "");
}

static void early_sha256(const char *text, char out[65])
{
    struct sha256_ctx sha;
    unsigned char digest[SHA256_OUTPUT_SIZE];
    sha256_init(&sha);
    sha256_write(&sha, (const unsigned char *)text, strlen(text));
    sha256_finalize(&sha, digest);
    zcl_hex_encode(digest, sizeof(digest), out);
}

/* The plan's non-integration groups, comma-joined in canonical order. A
 * universal, unbounded or empty selection is not early feedback. */
static const char *early_join(const char exact[][ZCL_TEST_GROUP_FULL_MAX],
                              size_t total,
                              struct zcl_devloop_early_receipt *r)
{
    for (size_t i = 0; i < total; i++) {
        size_t used = strlen(r->groups);
        int wrote = snprintf(r->groups + used, sizeof(r->groups) - used,
                             "%s%s", used ? "," : "", exact[i]);
        if (wrote < 0 || (size_t)wrote >= sizeof(r->groups) - used)
            return "facts_plan_exceeds_bound";
    }
    r->group_count = (uint32_t)total;
    early_sha256(r->groups, r->groups_sha256);
    return "";
}

static const char *early_select(const struct zcl_devloop_plan *plan,
                                struct zcl_devloop_early_receipt *r)
{
    const char *ids[ZCL_DEVLOOP_MAX_PLAN_GROUPS * 2];
    size_t id_count = 0;
    if (plan->closure_universal)
        return "facts_plan_universal";
    for (size_t i = 0; i < plan->path_groups_len; i++)
        ids[id_count++] = plan->path_groups[i];
    for (size_t i = 0; i < plan->closure_groups_len; i++)
        ids[id_count++] = plan->closure_groups[i];
    char (*exact)[ZCL_TEST_GROUP_FULL_MAX] = zcl_calloc(
        ZCL_DEVLOOP_EARLY_GROUP_MAX, sizeof(*exact), "early.exact");
    if (!exact)
        return "out_of_memory";
    bool truncated = false;
    size_t total = id_count ? zcl_test_group_expand_plan_immediate(
        ids, id_count, exact, ZCL_DEVLOOP_EARLY_GROUP_MAX, &truncated) : 0;
    const char *refused = total == SIZE_MAX ? "facts_plan_unresolved"
                        : truncated ? "facts_plan_exceeds_bound"
                        : total == 0 ? "facts_plan_no_groups"
                        : early_join((const char (*)[ZCL_TEST_GROUP_FULL_MAX])
                                         exact, total, r);
    free(exact);
    return refused;
}

static bool early_summary(const char *output, const char *key, uint32_t *out)
{
    const char *line = output ? strstr(output, "SUITE VERDICT ") : NULL;
    const char *at = line ? strstr(line, key) : NULL;
    if (!at)
        return false;
    at += strlen(key);
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(at, &end, 10);
    if (errno || end == at || value > UINT32_MAX)
        return false;
    *out = (uint32_t)value;
    return true;
}

static const char *early_red_reason(const struct zcl_devloop_process_result *p,
                                    bool ran, bool summary,
                                    const struct zcl_devloop_early_receipt *r)
{
    if (!ran)
        return "runner_not_executed";
    if (p->timed_out)
        return "runner_timed_out";
    if (p->term_signal)
        return "runner_signalled";
    if (!summary)
        return "runner_summary_missing";
    return r->groups_failed ? "groups_failed" : "runner_incomplete";
}

/* Green only for a clean exit whose summary ran or matched every group
 * with no failure and no self-skip. */
static void early_judge(const struct zcl_devloop_process_result *p, bool ran,
                        struct zcl_devloop_early_receipt *r)
{
    bool summary = ran &&
        early_summary(p->output, "groups_ran=", &r->groups_ran) &&
        early_summary(p->output, "groups_cached=", &r->groups_cached) &&
        early_summary(p->output, "groups_failed=", &r->groups_failed) &&
        early_summary(p->output, "self_skips=", &r->self_skips);
    r->exit_code = p->exit_code;
    if (p->cancelled) {
        r->status = "cancelled";
        r->reason = "cancelled";
        return;
    }
    bool green = summary && !p->timed_out && !p->term_signal &&
        p->exit_code == 0 && r->groups_failed == 0 && r->self_skips == 0 &&
        r->groups_ran + r->groups_cached == r->group_count;
    r->status = green ? "green" : "red";
    r->reason = green ? "" : early_red_reason(p, ran, summary, r);
}

/* Reported the moment it is known, before the full plan starts. */
static void early_report(const struct zcl_devloop_early_receipt *r)
{
    fprintf(stderr,
            "[devloop] early feedback (not proof) %s: %u facts groups on "
            "candidate %.12s, %u failed, first new-bytes execution %lld ms "
            "after the edit, %lld ms wall%s%s; the full plan still runs\n",
            r->status, r->group_count, r->artifact_sha256, r->groups_failed,
            (long long)(r->first_exec_us / 1000),
            (long long)(r->wall_us / 1000), r->reason[0] ? ": " : "",
            r->reason);
}

static void early_execute(const char *root, const char *artifact,
                          struct zcl_devloop_early_receipt *r)
{
    char exact[sizeof(r->groups) + 16];
    (void)snprintf(exact, sizeof(exact), "--exact=%s", r->groups);
    const char *argv[] = { artifact, exact, "--no-cache", NULL };
    struct zcl_devloop_process_result *p = zcl_calloc(1, sizeof(*p),
                                                      "early.process");
    if (!p) {
        zcl_devloop_early_skip(r, "out_of_memory", "early runner");
        return;
    }
    int64_t started = platform_time_monotonic_us();
    r->first_exec_us = started - r->origin_us;
    bool ran = zcl_devloop_process_run_test(root, argv, EARLY_TIMEOUT_MS, p);
    r->wall_us = platform_time_monotonic_us() - started;
    early_judge(p, ran, r);
    early_report(r);
    free(p);
}

void zcl_devloop_early_run(const char *root, const char *artifact,
                           const char *artifact_sha256,
                           const struct zcl_devloop_early_plan *plan,
                           const char *full_groups, int64_t fallback_origin_us,
                           struct zcl_devloop_early_receipt *r)
{
    memset(r, 0, sizeof(*r));
    r->origin_is_edit = plan && plan->origin_us > 0 &&
                        plan->origin_us <= fallback_origin_us;
    r->origin_us = r->origin_is_edit ? plan->origin_us : fallback_origin_us;
    if (!plan) {
        zcl_devloop_early_skip(r, "facts_dir_unset", "");
        return;
    }
    (void)snprintf(r->facts_dir, sizeof(r->facts_dir), "%s", plan->facts_dir);
    (void)snprintf(r->artifact_sha256, sizeof(r->artifact_sha256), "%s",
                   artifact_sha256 ? artifact_sha256 : "");
    r->facts_plan_us = plan->plan_us;
    if (!plan->ready) {
        zcl_devloop_early_skip(r, plan->skip_reason, plan->detail);
        return;
    }
    const char *refused = early_select(&plan->plan, r);
    if (!refused[0] && full_groups && strcmp(r->groups, full_groups) == 0)
        refused = "facts_plan_not_narrower";
    if (refused[0] || !root || !artifact || !artifact[0]) {
        zcl_devloop_early_skip(r, refused[0] ? refused : "candidate_missing",
                               plan->detail);
        return;
    }
    early_execute(root, artifact, r);
}

static void early_json_selection(const struct zcl_devloop_early_receipt *r,
                                 struct json_value *doc)
{
    struct json_value o;
    json_init(&o);
    json_set_object(&o);
    (void)json_push_kv_str(&o, "source", "facts");
    (void)json_push_kv_str(&o, "facts_dir", r->facts_dir);
    (void)json_push_kv_int(&o, "group_count", r->group_count);
    (void)json_push_kv_str(&o, "groups_sha256", r->groups_sha256);
    bool inline_groups = strlen(r->groups) <= EARLY_GROUPS_INLINE_MAX;
    (void)json_push_kv_bool(&o, "groups_listed", inline_groups);
    (void)json_push_kv_str(&o, "groups", inline_groups ? r->groups : "");
    (void)json_push_kv_str(&o, "candidate_sha256", r->artifact_sha256);
    (void)json_push_kv(doc, "selection", &o);
    json_free(&o);
}

static void early_json_result(const struct zcl_devloop_early_receipt *r,
                              struct json_value *doc)
{
    struct json_value o;
    json_init(&o);
    json_set_object(&o);
    (void)json_push_kv_int(&o, "groups_ran", r->groups_ran);
    (void)json_push_kv_int(&o, "groups_cached", r->groups_cached);
    (void)json_push_kv_int(&o, "groups_failed", r->groups_failed);
    (void)json_push_kv_int(&o, "self_skips", r->self_skips);
    (void)json_push_kv_int(&o, "exit_code", r->exit_code);
    (void)json_push_kv(doc, "result", &o);
    json_free(&o);
}

static void early_json_timing(const struct zcl_devloop_early_receipt *r,
                              struct json_value *doc)
{
    struct json_value o;
    json_init(&o);
    json_set_object(&o);
    (void)json_push_kv_str(&o, "origin",
                           r->origin_is_edit ? "edit_seen" : "proof_start");
    (void)json_push_kv_int(&o, "facts_plan_us", r->facts_plan_us);
    (void)json_push_kv_int(&o, "time_to_first_exec_us", r->first_exec_us);
    (void)json_push_kv_int(&o, "early_wall_us", r->wall_us);
    (void)json_push_kv_int(&o, "full_plan_time_to_first_exec_us",
                           r->full_first_exec_us);
    (void)json_push_kv_int(&o, "full_plan_wall_us", r->full_wall_us);
    (void)json_push_kv(doc, "timing", &o);
    json_free(&o);
}

void zcl_devloop_early_json(const struct zcl_devloop_early_receipt *r,
                            struct json_value *doc)
{
    if (!r || !doc || !r->status)
        return;
    struct json_value o;
    json_init(&o);
    json_set_object(&o);
    (void)json_push_kv_str(&o, "schema", ZCL_DEVLOOP_EARLY_SCHEMA);
    (void)json_push_kv_str(&o, "stage", "early");
    (void)json_push_kv_str(&o, "role", "feedback");
    (void)json_push_kv_bool(&o, "feedback_only", true);
    (void)json_push_kv_bool(&o, "proof_admissible", false);
    (void)json_push_kv_bool(&o, "receipt_authority", false);
    (void)json_push_kv_str(&o, "status", r->status);
    (void)json_push_kv_str(&o, "reason", r->reason ? r->reason : "");
    (void)json_push_kv_str(&o, "detail", r->detail);
    early_json_selection(r, &o);
    early_json_result(r, &o);
    early_json_timing(r, &o);
    (void)json_push_kv(doc, "early_feedback", &o);
    json_free(&o);
}
