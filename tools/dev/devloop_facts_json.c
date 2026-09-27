/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: dev.change.plan facts input: load before/after manifest evidence from a facts directory and render the narrowed plan with its verdict. */
#include "devloop_facts.h"

#include "util/safe_alloc.h"
#include "vcs/semantic_manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Evidence files are bounded: a manifest by the format's own ceiling, a
 * source by the same 16 MiB the facts section cap uses. */
#define FX_SOURCE_MAX (16u * 1024u * 1024u)

static bool fx_read(const char *root, const char *dir, const char *file,
                    const char *suffix, size_t max, uint8_t **out,
                    size_t *len)
{
    char path[ZCL_DEVLOOP_PATH_MAX * 2 + 64];
    FILE *fp;
    long size;
    bool ok = false;
    *out = NULL;
    *len = 0;
    if (snprintf(path, sizeof(path), "%s/%s%s%s%s", root, dir ? dir : "",
                 dir ? "/" : "", file, suffix) >= (int)sizeof(path))
        return false;
    fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    if (fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) >= 0 &&
        (size_t)size <= max && fseek(fp, 0, SEEK_SET) == 0) {
        *out = zcl_malloc((size_t)size + 1, "facts.evidence");
        ok = *out != NULL &&
             fread(*out, 1, (size_t)size, fp) == (size_t)size;
        *len = (size_t)size;
    }
    (void)fclose(fp);
    if (!ok) {
        free(*out);
        *out = NULL;
    }
    return ok;
}

/* A file whose evidence is incomplete gets no entry; the decision then
 * names it ("no-manifest"). */
static void fx_load(const char *root, const char *dir, const char *file,
                    struct zcl_devloop_facts_tu *tu)
{
    uint8_t *b = NULL, *a = NULL, *bs = NULL, *as = NULL;
    bool ok = fx_read(root, dir, file, ".before.zsm",
                      VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &b,
                      &tu->before_len) &&
              fx_read(root, dir, file, ".after.zsm",
                      VCS_SEMANTIC_MANIFEST_V1_MAX_BYTES, &a,
                      &tu->after_len) &&
              fx_read(root, dir, file, ".before", FX_SOURCE_MAX, &bs,
                      &tu->before_src_len) &&
              fx_read(root, NULL, file, "", FX_SOURCE_MAX, &as,
                      &tu->after_src_len);
    if (!ok) {
        free(b);
        free(a);
        free(bs);
        free(as);
        memset(tu, 0, sizeof(*tu));
        return;
    }
    tu->source = file;
    tu->before = b;
    tu->after = a;
    tu->before_src = bs;
    tu->after_src = as;
}

static void fx_unload(struct zcl_devloop_facts_tu *tu)
{
    free((void *)tu->before);
    free((void *)tu->after);
    free((void *)tu->before_src);
    free((void *)tu->after_src);
}

static bool fx_put(char *out, size_t cap, size_t *at, const char *s)
{
    size_t n = strlen(s);
    if (*at + n >= cap)
        return false;
    memcpy(out + *at, s, n);
    *at += n;
    out[*at] = '\0';
    return true;
}

static bool fx_put_str(char *out, size_t cap, size_t *at, const char *s)
{
    char esc[8];
    bool ok = fx_put(out, cap, at, "\"");
    for (const unsigned char *p = (const unsigned char *)s; ok && *p; p++) {
        if (*p == '"' || *p == '\\')
            (void)snprintf(esc, sizeof(esc), "\\%c", *p);
        else if (*p < 0x20)
            (void)snprintf(esc, sizeof(esc), "\\u%04x", *p);
        else
            (void)snprintf(esc, sizeof(esc), "%c", *p);
        ok = fx_put(out, cap, at, esc);
    }
    return ok && fx_put(out, cap, at, "\"");
}

/* ,"facts":{"narrowed":B,"reason":S,"detail":S,"seeds":[S...],
 *  "reached_files":N} */
static bool fx_verdict_json(const struct zcl_devloop_facts_verdict *v,
                            char *out, size_t cap, size_t *at)
{
    char num[32];
    bool ok = fx_put(out, cap, at, ",\"facts\":{\"narrowed\":") &&
              fx_put(out, cap, at, v->narrowed ? "true" : "false") &&
              fx_put(out, cap, at, ",\"reason\":") &&
              fx_put_str(out, cap, at, v->reason) &&
              fx_put(out, cap, at, ",\"detail\":") &&
              fx_put_str(out, cap, at, v->detail) &&
              fx_put(out, cap, at, ",\"seeds\":[");
    for (size_t k = 0; ok && k < v->seeds_len; k++)
        ok = (k == 0 || fx_put(out, cap, at, ",")) &&
             fx_put_str(out, cap, at, v->seeds[k]);
    (void)snprintf(num, sizeof(num), "%zu", v->reached_files);
    return ok && fx_put(out, cap, at, "],\"reached_files\":") &&
           fx_put(out, cap, at, num) && fx_put(out, cap, at, "}}");
}

/* Plan with the facts evidence and render the plan document with the
 * verdict spliced in before its closing brace; 0 on any failure. */
static size_t fx_render(const char *root, const char *const *files,
                        size_t file_count, const char *facts_dir,
                        struct zcl_devloop_plan *plan,
                        struct zcl_devloop_facts_tu *tus,
                        struct zcl_devloop_facts_verdict *v, char *out,
                        size_t out_sz)
{
    size_t n;
    if (!zcl_devloop_plan_files(files, file_count, plan))
        return 0;
    for (size_t k = 0; k < file_count; k++)
        fx_load(root, facts_dir, files[k], &tus[k]);
    if (!zcl_devloop_plan_add_closure_facts(root, files, file_count, tus,
                                            file_count, plan, v))
        return 0;
    n = zcl_devloop_plan_json_render(plan, files, file_count, out, out_sz);
    if (n == 0 || out[n - 1] != '}')
        return 0;
    n--;
    out[n] = '\0';
    return fx_verdict_json(v, out, out_sz, &n) ? n : 0;
}

size_t zcl_devloop_plan_json_facts(const char *repo_root,
                                   const char *const *files, size_t file_count,
                                   const char *facts_dir, char *out,
                                   size_t out_sz)
{
    const char *root = repo_root && repo_root[0] ? repo_root : ".";
    struct zcl_devloop_plan *plan = zcl_malloc(sizeof(*plan), "facts.plan");
    struct zcl_devloop_facts_tu *tus =
        zcl_calloc(file_count ? file_count : 1, sizeof(*tus), "facts.tus");
    struct zcl_devloop_facts_verdict *v = zcl_malloc(sizeof(*v), "facts.v");
    bool ok = plan && tus && v && out && out_sz > 0 && facts_dir;
    size_t n = ok ? fx_render(root, files, file_count, facts_dir, plan, tus, v,
                              out, out_sz)
                  : 0;
    for (size_t k = 0; tus && k < file_count; k++)
        fx_unload(&tus[k]);
    free(tus);
    free(plan);
    free(v);
    return n;
}
