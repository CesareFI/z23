/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Allocation-failure coverage for the libclang-free clang-manifest sensor
 * support TUs (tools/sensors/clang_manifest_core.c, clang_manifest_warm.c).
 *
 * The sensor is a standalone tool, so its TUs are linked into no test binary;
 * this group unity-builds the two clang-free TUs it covers, which also puts
 * their statics in reach. zcl_alloc_fault_fail_nth (base/safe_alloc.h) makes
 * the Nth checked allocation with an exact label return NULL once.
 *
 * cm_sdir_add (clang_manifest_warm.c) keeps a shadow directory's entry names
 * in a table that cm_sdir_read later qsorts with strcmp; a failed entry
 * strdup must therefore never be stored, and must not advance the count. The
 * sibling read-side guard in cm_cmd_root (a failed read with an allocated
 * buffer reports "unreadable", never an uninitialized reason) is pinned
 * against the real sensor binary by the semantic_sensor group's short-read
 * probe case; the cm_typedef_pass NULL-name guard is the same one-line class
 * of fix in the libclang-bound TU, where no committed case can reach the
 * allocation without linking libclang into the monolith. */

#include "test/test_core.h"

#include "base/safe_alloc.h"

#include "../../../tools/sensors/clang_manifest_core.c"
/* cm_object_cc_text and cm_norm_arg live in these two clang-free TUs. */
#include "../../../tools/sensors/clang_manifest_cc.c"
#include "../../../tools/sensors/clang_manifest_paths.c"
#include "../../../tools/sensors/clang_manifest_warm.c"

static int cmo_t_sdir_add_oom(void)
{
    int failures = 0;
    struct cm_sdir d = {0};
    TEST_CASE("clang_manifest_oom: a failed shadow-dir strdup is never "
              "stored") {
        /* Baseline: without injection the add succeeds and owns its copy. */
        ASSERT(cm_sdir_add(&d, "alpha"));
        ASSERT_EQ(d.nnames, 1);
        ASSERT(strcmp(d.names[0], "alpha") == 0);
        free(d.names[0]);
        d.names[0] = NULL;
        d.nnames = 0;

        /* The first allocation labelled clang_manifest.str is the entry
         * strdup; failing it must leave the table exactly as it was, because
         * cm_sdir_read qsorts these names with strcmp. The growth realloc
         * carries its own clang_manifest.grow label and still succeeds. */
        zcl_alloc_fault_fail_nth("clang_manifest.str", 1);
        ASSERT(!cm_sdir_add(&d, "beta"));
        zcl_alloc_fault_clear();
        ASSERT_EQ(d.nnames, 0);

        /* The table is still fully usable after the refused add. */
        ASSERT(cm_sdir_add(&d, "gamma"));
        ASSERT_EQ(d.nnames, 1);
        ASSERT(strcmp(d.names[0], "gamma") == 0);
    } TEST_END
    free(d.names[0]);
    free(d.names);
    return failures;
}

int test_clang_manifest_oom(void)
{
    int failures = 0;
    failures += cmo_t_sdir_add_oom();
    return failures;
}
