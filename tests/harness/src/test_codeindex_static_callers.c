/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * codeindex_static_callers — the impact closure walks symbol IDENTITIES, not
 * bare names. A static function defined in a .c file is internal to that
 * translation unit, so a same-named static in another TU is not one of its
 * callers. The name-only walk treated every `helper(` in the tree as a call to
 * the changed `helper`, and on the real tree one test-file edit selected 134
 * test groups where two were owed.
 *
 * Coverage (all over one fixture tree under ./test-tmp/):
 *   1. two TUs each define `static int helper(void)`: editing one selects only
 *      its own callers, never the other TU or that TU's callers;
 *   2. a static CALLER found mid-walk keeps its file: `static run()` in two
 *      files, only the one that calls the changed function climbs;
 *   3. never under-select: a .c that #includes the static's file and calls it,
 *      a header whose inline body calls it, and a file that defines its own
 *      `helper` under #ifdef but includes the static's file on the other arm
 *      (seen through the depfile) all stay selected;
 *   4. a `static inline` defined in a header is defined in every includer: its
 *      callers are the includers' functions, and the header case keeps the
 *      conservative name walk (it does not shrink);
 *   5. the overlay walk (changed bytes scanned fresh) keys its seeds the same
 *      way and returns the identical set. */

#include "test/test_core.h"

#include "codeindex/codeindex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SC_FIX "test-tmp/codeindex_static_callers"
#define SC_SRC "core/modules/net/src/"
#define SC_INC "core/modules/net/include/net/"
#define SC_OUT_CAP 64

static bool sc_write(const char *rel, const char *content)
{
    char full[512];
    int n = snprintf(full, sizeof(full), "%s/%s", SC_FIX, rel);
    if (n <= 0 || (size_t)n >= sizeof(full))
        return false;
    for (char *p = full + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(full, 0755);
            *p = '/';
        }
    }
    FILE *f = fopen(full, "wb");
    if (!f)
        return false;
    size_t len = strlen(content);
    bool ok = fwrite(content, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static bool sc_write_fixture(void)
{
    bool ok = true;
    /* 1: two TUs, one static name. */
    ok = ok && sc_write(SC_SRC "sc_x.c",
        "static int helper(void)\n{\n    return 1;\n}\n"
        "int sc_x_api(void)\n{\n    return helper();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_y.c",
        "static int helper(void)\n{\n    return 2;\n}\n"
        "int sc_y_api(void)\n{\n    return helper();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_x_user.c",
        "int sc_x_user(void)\n{\n    return sc_x_api();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_y_user.c",
        "int sc_y_user(void)\n{\n    return sc_y_api();\n}\n");
    ok = ok && sc_write("build/obj/sc_y.d",
        "build/obj/sc_y.o: " SC_SRC "sc_y.c\n");
    /* Calls some OTHER helper; its depfile proves sc_x.c is not in its TU. */
    ok = ok && sc_write(SC_SRC "sc_ext.c",
        "int sc_ext_fn(void)\n{\n    return helper();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_ext_user.c",
        "int sc_ext_user(void)\n{\n    return sc_ext_fn();\n}\n");
    ok = ok && sc_write("build/obj/sc_ext.d",
        "build/obj/sc_ext.o: " SC_SRC "sc_ext.c\n");

    /* 3: the ways a file other than sc_x.c CAN reach sc_x.c's helper. */
    ok = ok && sc_write(SC_SRC "sc_inc.c",
        "#include \"sc_x.c\"\n"
        "int sc_inc_fn(void)\n{\n    return helper() + 1;\n}\n");
    /* The same include with no depfile: nothing proves it away. */
    ok = ok && sc_write(SC_SRC "sc_inc2.c",
        "#include \"sc_x.c\"\n"
        "int sc_inc2_fn(void)\n{\n    return helper() + 2;\n}\n");
    ok = ok && sc_write(SC_SRC "sc_inc_user.c",
        "int sc_inc_user(void)\n{\n    return sc_inc_fn();\n}\n");
    ok = ok && sc_write(SC_INC "sc_x_inl.h",
        "static inline int sc_x_wrap(void)\n{\n    return helper();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_alt.c",
        "#ifdef SC_ALT\n"
        "static int helper(void)\n{\n    return 3;\n}\n"
        "#else\n#include \"sc_x.c\"\n#endif\n"
        "int sc_alt_fn(void)\n{\n    return helper();\n}\n");
    ok = ok && sc_write("build/obj/sc_alt.d",
        "build/obj/sc_alt.o: " SC_SRC "sc_alt.c " SC_SRC "sc_x.c\n");
    ok = ok && sc_write("build/obj/sc_inc.d",
        "build/obj/sc_inc.o: " SC_SRC "sc_inc.c " SC_SRC "sc_x.c\n");

    /* 2: a static caller discovered mid-walk. */
    ok = ok && sc_write(SC_SRC "sc_lib.c",
        "int sc_lib_fn(void)\n{\n    return 7;\n}\n");
    ok = ok && sc_write(SC_SRC "sc_p.c",
        "static int run(void)\n{\n    return sc_lib_fn();\n}\n"
        "int sc_p_api(void)\n{\n    return run();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_q.c",
        "static int run(void)\n{\n    return 0;\n}\n"
        "int sc_q_api(void)\n{\n    return run();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_p_user.c",
        "int sc_p_user(void)\n{\n    return sc_p_api();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_q_user.c",
        "int sc_q_user(void)\n{\n    return sc_q_api();\n}\n");

    /* 4: a static inline defined in a header. */
    ok = ok && sc_write(SC_INC "sc_inl.h",
        "#ifndef NET_SC_INL_H\n#define NET_SC_INL_H\n"
        "static inline int sc_hx(void)\n{\n    return 5;\n}\n#endif\n");
    ok = ok && sc_write(SC_SRC "sc_ha.c",
        "#include \"net/sc_inl.h\"\n"
        "int sc_ha_fn(void)\n{\n    return sc_hx();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_hb.c",
        "#include \"net/sc_inl.h\"\n"
        "int sc_hb_fn(void)\n{\n    return sc_hx() + 1;\n}\n");
    ok = ok && sc_write(SC_SRC "sc_hc.c",
        "static int sc_hx(void)\n{\n    return 9;\n}\n"
        "int sc_hc_fn(void)\n{\n    return sc_hx();\n}\n");
    ok = ok && sc_write(SC_SRC "sc_hd.c",
        "int sc_hd_fn(void)\n{\n    return sc_ha_fn();\n}\n");
    ok = ok && sc_write("build/obj/sc_ha.d",
        "build/obj/sc_ha.o: " SC_SRC "sc_ha.c " SC_INC "sc_inl.h\n");
    ok = ok && sc_write("build/obj/sc_hb.d",
        "build/obj/sc_hb.o: " SC_SRC "sc_hb.c " SC_INC "sc_inl.h\n");
    return ok;
}

/* Run the store walk (or, with `overlay`, the fresh-bytes walk) for one
 * changed file and compare the result with `want`, a sorted NULL-terminated
 * list. Prints the first difference so a failure names the file. */
static bool sc_closure_is(struct codeindex *ci, const char *changed_path,
                          bool overlay, const char *const *want)
{
    char changed[1][256];
    snprintf(changed[0], sizeof(changed[0]), "%s", changed_path);
    char out[SC_OUT_CAP][256];
    bool truncated = true;
    int n = overlay
        ? codeindex_impact_closure_overlay(ci, SC_FIX, changed, 1, 0, out,
                                           SC_OUT_CAP, &truncated)
        : codeindex_impact_closure(ci, changed, 1, 0, out, SC_OUT_CAP,
                                   &truncated);
    int want_n = 0;
    while (want[want_n])
        want_n++;
    bool same = n == want_n && !truncated;
    for (int i = 0; same && i < n; i++)
        same = strcmp(out[i], want[i]) == 0;
    if (!same) {
        printf("\n    closure(%s%s) n=%d truncated=%d:", changed_path,
               overlay ? ", overlay" : "", n, truncated ? 1 : 0);
        for (int i = 0; i < n && i < SC_OUT_CAP; i++)
            printf(" %s", out[i]);
        printf("\n    want:");
        for (int i = 0; i < want_n; i++)
            printf(" %s", want[i]);
        printf("\n    ");
    }
    return same;
}

static bool sc_closure_has(struct codeindex *ci, const char *changed_path,
                           const char *needle, bool *has)
{
    char changed[1][256];
    snprintf(changed[0], sizeof(changed[0]), "%s", changed_path);
    char out[SC_OUT_CAP][256];
    bool truncated = true;
    int n = codeindex_impact_closure(ci, changed, 1, 0, out, SC_OUT_CAP,
                                     &truncated);
    *has = false;
    for (int i = 0; i < n; i++)
        if (strcmp(out[i], needle) == 0)
            *has = true;
    return n >= 0 && !truncated;
}

static const char *const g_sc_x_want[] = {
    SC_INC "sc_x_inl.h",
    SC_SRC "sc_alt.c",
    SC_SRC "sc_inc.c",
    SC_SRC "sc_inc2.c",
    SC_SRC "sc_inc_user.c",
    SC_SRC "sc_x.c",
    SC_SRC "sc_x_user.c",
    NULL,
};

static int test_sc_two_tus_one_static_name(struct codeindex *ci)
{
    int failures = 0;
    TEST("codeindex_static_callers: editing one TU's static helper selects "
         "only that TU's callers") {
        ASSERT(sc_closure_is(ci, SC_SRC "sc_x.c", false, g_sc_x_want));
        bool has = true;
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_y.c", &has));
        ASSERT(!has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_y_user.c",
                              &has));
        ASSERT(!has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_ext.c", &has));
        ASSERT(!has);
        /* The other direction: sc_x.c defines its own helper and every
         * depfile-recorded TU lacks sc_y.c. The header inline body and the
         * depfile-less includer sc_inc2.c cannot be proven away, so they stay:
         * the rule drops only what it can prove. */
        static const char *const y_want[] = {
            SC_INC "sc_x_inl.h", SC_SRC "sc_inc2.c", SC_SRC "sc_y.c",
            SC_SRC "sc_y_user.c", NULL,
        };
        ASSERT(sc_closure_is(ci, SC_SRC "sc_y.c", false, y_want));
        PASS();
    } _test_next:;
    return failures;
}

static int test_sc_static_caller_keeps_its_file(struct codeindex *ci)
{
    int failures = 0;
    TEST("codeindex_static_callers: a static caller reached mid-walk climbs "
         "only in its own file") {
        static const char *const want[] = {
            SC_SRC "sc_lib.c", SC_SRC "sc_p.c", SC_SRC "sc_p_user.c", NULL,
        };
        ASSERT(sc_closure_is(ci, SC_SRC "sc_lib.c", false, want));
        PASS();
    } _test_next:;
    return failures;
}

static int test_sc_includers_never_dropped(struct codeindex *ci)
{
    int failures = 0;
    TEST("codeindex_static_callers: an includer, a header inline body and an "
         "#ifdef twin with a depfile edge stay selected") {
        bool has = false;
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_inc.c", &has));
        ASSERT(has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_inc_user.c",
                              &has));
        ASSERT(has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_inc2.c", &has));
        ASSERT(has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_INC "sc_x_inl.h",
                              &has));
        ASSERT(has);
        ASSERT(sc_closure_has(ci, SC_SRC "sc_x.c", SC_SRC "sc_alt.c", &has));
        ASSERT(has);
        PASS();
    } _test_next:;
    return failures;
}

static int test_sc_header_static_inline_does_not_shrink(struct codeindex *ci)
{
    int failures = 0;
    TEST("codeindex_static_callers: a header static inline keeps every "
         "includer caller and the name walk") {
        /* sc_ha.c and sc_hb.c include the header and call it; sc_hd.c calls
         * sc_ha.c. sc_hc.c defines its own static sc_hx: the header case
         * deliberately stays on the name walk, so it is selected too. */
        static const char *const want[] = {
            SC_INC "sc_inl.h",
            SC_SRC "sc_ha.c",
            SC_SRC "sc_hb.c",
            SC_SRC "sc_hc.c",
            SC_SRC "sc_hd.c",
            NULL,
        };
        ASSERT(sc_closure_is(ci, SC_INC "sc_inl.h", false, want));
        PASS();
    } _test_next:;
    return failures;
}

static int test_sc_overlay_keys_seeds_the_same_way(struct codeindex *ci)
{
    int failures = 0;
    TEST("codeindex_static_callers: the overlay walk returns the identical "
         "file-scoped set") {
        ASSERT(sc_closure_is(ci, SC_SRC "sc_x.c", true, g_sc_x_want));
        PASS();
    } _test_next:;
    return failures;
}

int test_codeindex_static_callers(void)
{
    int failures = 0;
    struct codeindex *ci = NULL;
    TEST("codeindex_static_callers: fixture tree indexes") {
        (void)system("rm -rf " SC_FIX);
        ASSERT(sc_write_fixture());
        ci = codeindex_open(SC_FIX);
        ASSERT(ci != NULL);
        PASS();
    } _test_next:;
    if (ci) {
        failures += test_sc_two_tus_one_static_name(ci);
        failures += test_sc_static_caller_keeps_its_file(ci);
        failures += test_sc_includers_never_dropped(ci);
        failures += test_sc_header_static_inline_does_not_shrink(ci);
        failures += test_sc_overlay_keys_seeds_the_same_way(ci);
        codeindex_close(ci);
    }
    (void)system("rm -rf " SC_FIX);
    return failures;
}
