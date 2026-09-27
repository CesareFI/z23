/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Write the declaration-identity consumer fixture tree, its variants and their depfiles for the checked-in and live consumer tests. */
#include "test/semantic_consumer_fixture.h"

#include "base/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char k_scx_header[] =
    "#ifndef CX_H\n"
    "#define CX_H\n"
    "#define CX_CAP 64\n"
    "#define CX_MODE 1\n"
    "#define CX_BASE 3\n"
    "#define CX_SCALE (CX_BASE * 2)\n"
    "#define CX_PAD 4\n"
    "typedef int cx_count;\n"
    "struct cx_big { int a; char buf[CX_CAP]; };\n"
    "struct cx_small { char pad[CX_PAD]; int n; };\n"
    "typedef struct cx_big cx_big_t;\n"
    "int cx_fill(struct cx_big *b);\n"
    "int cx_count_of(cx_count c);\n"
    "int cx_sum(int v);\n"
    "int cx_hook(int v);\n"
    "#endif\n";

static const char k_scx_a[] =
    "#include \"cx.h\"\n"
    "int cx_fill(struct cx_big *b) { b->a = 1; return (int)sizeof(b->buf); }\n"
    "static int cx_twice(int v) { return v * 2; }\n"
    "int cx_top_a(void);\n"
    "int cx_top_a(void) { return cx_twice(cx_sum(1)); }\n";

static const char k_scx_b[] =
    "#include \"cx.h\"\n"
    "#if CX_MODE > 1\n"
    "int cx_count_of(cx_count c) { return (int)c * 2; }\n"
    "#else\n"
    "int cx_count_of(cx_count c) { return (int)c; }\n"
    "#endif\n"
    "static int cx_twice(int v) { return v + v; }\n"
    "int cx_top_b(void);\n"
    "int cx_top_b(void) { return cx_twice(cx_count_of(2)); }\n";

static const char k_scx_c[] =
    "#include \"cx.h\"\n"
    "int cx_sum(int v) { struct cx_small s = {.n = v};"
    " return s.n + (int)sizeof(s) + CX_SCALE; }\n"
    "int cx_hook(int v) { return v - 1; }\n";

static const char k_scx_d[] =
    "#include \"cx.h\"\n"
    "int (*const cx_hook_ref)(int) = cx_hook;\n"
    "int cx_top_d(void);\n"
    "int cx_top_d(void) { return cx_sum(3); }\n"
    "int cx_size_d(void);\n"
    "int cx_size_d(void) { return (int)sizeof(cx_big_t); }\n";

static const char k_scx_e[] =
    "#include \"cx.h\"\n"
    "int cx_top_e(void);\n"
    "int cx_top_e(void) { return 5; }\n";

const char *const k_scx_paths[SCX_FILE_COUNT] = {SCX_HEADER, SCX_A, SCX_B,
                                                 SCX_C,      SCX_D, SCX_E};
const char *const k_scx_tus[SCX_TU_COUNT] = {SCX_A, SCX_B, SCX_C, SCX_D,
                                             SCX_E};
static const char *const k_scx_bodies[SCX_FILE_COUNT] = {
    k_scx_header, k_scx_a, k_scx_b, k_scx_c, k_scx_d, k_scx_e};

const char *const k_scx_flags[] = {"-std=c23", "-g1", "-O1",
                                   "-I" SCX_DIR "/include"};
const size_t k_scx_nflags = sizeof(k_scx_flags) / sizeof(k_scx_flags[0]);

#define SCX_NONE {false, false, false, false, false}
#define SCX_UNAFFECTED {"unaffected", "unaffected", "unaffected", \
                        "unaffected", "unaffected"}

const struct scx_edit k_scx_edits[SCX_VARIANT_COUNT] = {
    [SCX_BASE] = {.name = "base"},
    [SCX_LAYOUT] = {.name = "layout", .file = SCX_HEADER,
                    .from = "char buf[CX_CAP]; };",
                    .to = "char buf[CX_CAP]; int extra; };",
                    .changed = {SCX_HEADER},
                    .affected = {true, false, false, true, false},
                    .reason = {"interface", "unaffected", "unaffected",
                               "interface", "unaffected"},
                    .obligations = ""},
    [SCX_MACRO] = {.name = "macro", .file = SCX_HEADER,
                   .from = "#define CX_CAP 64", .to = "#define CX_CAP 65",
                   .changed = {SCX_HEADER},
                   .affected = {true, false, false, true, false},
                   .reason = {"interface", "unaffected", "unaffected",
                              "interface", "unaffected"},
                   .obligations = ""},
    [SCX_COND] = {.name = "cond", .file = SCX_HEADER,
                  .from = "#define CX_MODE 1", .to = "#define CX_MODE 2",
                  .changed = {SCX_HEADER},
                  .affected = {false, true, false, false, false},
                  .reason = {"unaffected", "macro-conditional", "unaffected",
                             "unaffected", "unaffected"},
                  .obligations = ""},
    [SCX_NESTED] = {.name = "nested", .file = SCX_HEADER,
                    .from = "#define CX_BASE 3", .to = "#define CX_BASE 4",
                    .changed = {SCX_HEADER},
                    .affected = {false, false, true, false, false},
                    .reason = {"unaffected", "unaffected", "interface",
                               "unaffected", "unaffected"},
                    .obligations = ""},
    [SCX_TYPEDEF] = {.name = "typedef", .file = SCX_HEADER,
                     .from = "typedef int cx_count;",
                     .to = "typedef long cx_count;",
                     .changed = {SCX_HEADER},
                     .affected = {false, true, false, false, false},
                     .reason = {"unaffected", "interface", "unaffected",
                                "unaffected", "unaffected"},
                     .obligations = ""},
    [SCX_TAIL] = {.name = "tail", .file = SCX_HEADER,
                  .from = "int cx_hook(int v);\n#endif",
                  .to = "int cx_hook(int v);\n/* tail */\n#endif",
                  .changed = {SCX_HEADER}, .affected = SCX_NONE,
                  .reason = SCX_UNAFFECTED, .obligations = ""},
    [SCX_TOP] = {.name = "top", .file = SCX_HEADER,
                 .from = "#define CX_H\n", .to = "#define CX_H\n/* top */\n",
                 .changed = {SCX_HEADER},
                 .affected = {true, true, true, true, false},
                 .reason = {"position", "position", "position", "position",
                            "unaffected"},
                 .obligations = ""},
    [SCX_SIGNATURE] = {.name = "signature", .file = SCX_HEADER,
                       .from = "int cx_sum(int v);",
                       .to = "int cx_sum(long v);", .file2 = SCX_C,
                       .from2 = "int cx_sum(int v) {",
                       .to2 = "int cx_sum(long v) {",
                       .changed = {SCX_HEADER, SCX_C},
                       .affected = {true, false, true, true, false},
                       .reason = {"interface", "unaffected", "source-changed",
                                  "interface", "unaffected"},
                       .obligations = ""},
    [SCX_STATIC] = {.name = "static", .file = SCX_A,
                    .from = "return v * 2; }", .to = "return v * 3; }",
                    .changed = {SCX_A},
                    .affected = {true, false, false, false, false},
                    .reason = {"source-changed", NULL, NULL, NULL, NULL},
                    .obligations = "",
                    /* cx_top_a calls cx_twice: -O1 may inline it */
                    .seeds = {"cx_twice", "cx_top_a"}},
    [SCX_ADDRESS] = {.name = "address", .file = SCX_C,
                     .from = "return v - 1; }", .to = "return v - 2; }",
                     .changed = {SCX_C},
                     .affected = {false, false, true, false, false},
                     .reason = {NULL, NULL, "source-changed", NULL, NULL},
                     .obligations = "address-taken"},
    [SCX_SHADOWED] = {.name = "shadowed", .add_path = SCX_SHADOW,
                      .changed = {SCX_SHADOW},
                      .affected = {true, true, true, true, true},
                      .reason = {"include-resolution-change",
                                 "include-resolution-change",
                                 "include-resolution-change",
                                 "include-resolution-change",
                                 "include-resolution-change"},
                      .obligations = "include-graph-truncated",
                      /* the module's include/cx.h still exists and no
                       * depfile lists it: the graph refuses to narrow */
                      .incomplete = "include-graph-truncated"},
    [SCX_DRIFT] = {.name = "drift", .file = SCX_HEADER,
                   .from = "int cx_hook(int v);\n#endif",
                   .to = "int cx_hook(int v);\n/* drift */\n#endif",
                   .extra_flag = "-DCX_DRIFT=1", .changed = {SCX_HEADER},
                   .affected = {true, true, true, true, true},
                   .reason = {"identity-drift", "identity-drift",
                              "identity-drift", "identity-drift",
                              "identity-drift"},
                   .obligations = ""},
    [SCX_LOCAL] = {.name = "local", .file = SCX_HEADER,
                   .from = "#define CX_PAD 4", .to = "#define CX_PAD 8",
                   .changed = {SCX_HEADER},
                   .affected = {false, false, true, false, false},
                   .reason = {"unaffected", "unaffected", "interface",
                              "unaffected", "unaffected"},
                   .obligations = "", .seeds = {"cx_sum"}},
    [SCX_BUILD] = {.name = "makefile", .changed = {SCX_MAKEFILE},
                   .affected = {true, true, true, true, true},
                   .reason = {"build-input-changed", "build-input-changed",
                              "build-input-changed", "build-input-changed",
                              "build-input-changed"},
                   .obligations = "build-input-changed",
                   .incomplete = "build-input-changed", .universal = true},
    [SCX_TOOL] = {.name = "tool", .extra_flag = "-DCX_DRIFT=1",
                  .changed = {SCX_MAKEFILE},
                  .affected = {true, true, true, true, true},
                  .reason = {"identity-drift", "identity-drift",
                             "identity-drift", "identity-drift",
                             "identity-drift"},
                  .obligations = "identity-drift",
                  .incomplete = "identity-drift", .universal = true},
    [SCX_BODY] = {.name = "body", .file = SCX_C,
                  .from = "(int)sizeof(s) + CX_SCALE; }",
                  .to = "(int)sizeof(s) + CX_SCALE + 1; }",
                  .changed = {SCX_C},
                  .affected = {false, false, true, false, false},
                  .reason = {NULL, NULL, "source-changed", NULL, NULL},
                  .obligations = "", .seeds = {"cx_sum"}},
};

static char *scx_replace(const char *body, const char *from, const char *to,
                         size_t *len)
{
    const char *at = strstr(body, from);
    size_t head, n;
    char *out;
    if (at == NULL || strstr(at + 1, from) != NULL)
        return NULL; /* the edit must name exactly one place */
    head = (size_t)(at - body);
    n = strlen(body) - strlen(from) + strlen(to);
    out = zcl_malloc(n + 1, "scx.text");
    if (out == NULL)
        return NULL;
    memcpy(out, body, head);
    memcpy(out + head, to, strlen(to));
    memcpy(out + head + strlen(to), at + strlen(from),
           strlen(at + strlen(from)) + 1);
    *len = n;
    return out;
}

static const char *scx_base_body(const char *path)
{
    if (strcmp(path, SCX_SHADOW) == 0)
        return k_scx_header;
    for (size_t k = 0; k < SCX_FILE_COUNT; k++)
        if (strcmp(k_scx_paths[k], path) == 0)
            return k_scx_bodies[k];
    return NULL;
}

char *scx_text(enum scx_variant v, const char *path, size_t *len)
{
    const struct scx_edit *e = &k_scx_edits[v];
    const char *body = scx_base_body(path);
    if (body == NULL ||
        (strcmp(path, SCX_SHADOW) == 0 &&
         (e->add_path == NULL || strcmp(e->add_path, path) != 0)))
        return NULL;
    if (e->file != NULL && strcmp(e->file, path) == 0)
        return scx_replace(body, e->from, e->to, len);
    if (e->file2 != NULL && strcmp(e->file2, path) == 0)
        return scx_replace(body, e->from2, e->to2, len);
    *len = strlen(body);
    return zcl_strdup(body, "scx.text");
}

static bool scx_mkdirs(char *full)
{
    for (char *p = full + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (!scx_mkdir(full)) {
            *p = '/';
            return false;
        }
        *p = '/';
    }
    return true;
}

static bool scx_put(const char *root, const char *rel, const char *body,
                    size_t n)
{
    char full[4096];
    FILE *fp;
    bool ok;
    if (snprintf(full, sizeof(full), "%s/%s", root, rel) >= (int)sizeof(full))
        return false;
    fp = scx_mkdirs(full) ? fopen(full, "wb") : NULL;
    ok = fp != NULL && fwrite(body, 1, n, fp) == n;
    if (fp != NULL && fclose(fp) != 0)
        ok = false;
    return ok;
}

static bool scx_write_one(const char *root, enum scx_variant v,
                          const char *path)
{
    size_t n = 0;
    char *body = scx_text(v, path, &n);
    bool ok = body != NULL && scx_put(root, path, body, n);
    free(body);
    return ok;
}

bool scx_write_tree(const char *root, enum scx_variant v)
{
    const char *add = k_scx_edits[v].add_path;
    char full[4096];
    for (size_t k = 0; k < SCX_FILE_COUNT; k++)
        if (!scx_write_one(root, v, k_scx_paths[k]))
            return false;
    if (add != NULL)
        return scx_write_one(root, v, add);
    /* A variant without the shadow must not inherit one. */
    (void)snprintf(full, sizeof(full), "%s/%s", root, SCX_SHADOW);
    return unlink(full) == 0 || access(full, F_OK) != 0;
}

bool scx_write_depfiles(const char *root, enum scx_variant v)
{
    const char *hdr = k_scx_edits[v].add_path ? SCX_SHADOW : SCX_HEADER;
    for (size_t k = 0; k < SCX_TU_COUNT; k++) {
        char rel[256], body[512];
        const char *base = strrchr(k_scx_tus[k], '/') + 1;
        int n = snprintf(body, sizeof(body), "build/scx/%.*s.o: %s \\\n %s\n",
                         (int)(strlen(base) - 2), base, k_scx_tus[k], hdr);
        (void)snprintf(rel, sizeof(rel), "build/scx/%.*s.d",
                       (int)(strlen(base) - 2), base);
        if (n <= 0 || (size_t)n >= sizeof(body) ||
            !scx_put(root, rel, body, (size_t)n))
            return false;
    }
    return true;
}

bool scx_mkdir(const char *path)
{
    return mkdir(path, 0755) == 0 || access(path, F_OK) == 0;
}
