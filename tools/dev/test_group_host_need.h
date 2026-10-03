/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Resolve the declared host input an exact test group needs. */

#ifndef ZCL_TEST_GROUP_HOST_NEED_H
#define ZCL_TEST_GROUP_HOST_NEED_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What a group's host must already carry. NONE is the normal case: the group
 * needs nothing beyond the tree every runner already has. */
enum zcl_test_group_host_need_kind {
    ZCL_HOST_NEED_NONE = 0,
    /* `value` is a path relative to the tree the runner execs in. */
    ZCL_HOST_NEED_FILE,
    /* `value` is an environment variable name. */
    ZCL_HOST_NEED_ENV,
    /* Exact root-owned Clang/GCC pair must parse the C23 fuzz syntax. */
    ZCL_HOST_NEED_C23_TOOLCHAIN,
    /* `value` is a path relative to the tree the runner execs in, and
     * `target` the Make target that builds it there from that tree's own
     * sources. A proof whose selection carries the group builds `target` in
     * its generation before the test dimension and fails, by name, when it
     * cannot; the group is never left to self-skip. */
    ZCL_HOST_NEED_BUILD,
};

struct zcl_test_group_host_need {
    enum zcl_test_group_host_need_kind kind;
    const char *group; /* canonical full catalog id, or NULL for NONE */
    const char *value; /* path or environment name, or NULL for NONE */
    const char *target; /* BUILD only: the Make target; NULL otherwise */
};

/* Platform-qualified BUILD rows bound distinct targets for both runners.
 * Deduplication can reduce this count; no valid declaration can exceed it. */
enum {
    ZCL_TEST_GROUP_BUILD_NEED_ROWS = 0
#define ZCL_TEST_GROUP_NEED(id_, kind_, value_)
#define ZCL_TEST_GROUP_BUILD_NEED(id_, value_, target_) + 1
#include "test_group_host_needs.def"
#undef ZCL_TEST_GROUP_BUILD_NEED
#undef ZCL_TEST_GROUP_NEED
};

/* Every declared row names a registered catalog group with a known kind and
 * a non-empty value. A group declares at most one host gate and may also
 * declare distinct BUILD targets. A violation is named on stderr and
 * returns false; no caller may proceed on a false. */
bool zcl_test_group_host_needs_valid(void);

/* Resolve `group`'s gating need: its FILE/ENV/TOOLCHAIN row, or first BUILD row
 * (which never gates), or NONE. Returns false — and names why — when the
 * table is invalid or `group` is not a registered catalog id; that is a
 * refusal, not an answer. Returns true with out->kind == ZCL_HOST_NEED_NONE
 * for a registered group that declares no need. */
bool zcl_test_group_host_need(const char *group,
                              struct zcl_test_group_host_need *out);

/* Append to needs[0..*n) every BUILD row `group` declares whose target is
 * not already listed, in table order, advancing *n. Collecting over a whole
 * selection therefore names each Make target once. The one resolver both the
 * proof's test-needs step and the local runner's --list-build-needs read, so
 * a local run and a landing proof build the same tools for the same groups.
 * Refuses an unregistered group, an invalid table, and a full array -- a
 * dropped need would leave its group to fail on a missing tool. */
bool zcl_test_group_build_needs_add(const char *group,
                                    struct zcl_test_group_host_need *needs,
                                    size_t cap, size_t *n);

/* Is the need satisfied by the tree at `root` and this process's environment?
 * FILE and BUILD resolve `<root>/<value>`, so they answer for the tree a
 * runner will exec in and never for the caller's own checkout. NONE is always
 * met; an unknown kind or a missing argument is refused as unmet. */
bool zcl_test_group_host_need_met(const char *root,
                                  const struct zcl_test_group_host_need *need);

/* May a selector carry the group in the tree at `root`? NONE and BUILD
 * always -- a BUILD need is the proof's own to satisfy, and the proof fails
 * when it cannot -- FILE and ENV only when met. The universal selector and
 * its shadow reference ask this one question, so they cannot disagree. */
bool zcl_test_group_host_need_selectable(
    const char *root, const struct zcl_test_group_host_need *need);

/* Stable lowercase token for a kind, for logs. NULL on an unknown kind. */
const char *
zcl_test_group_host_need_kind_name(enum zcl_test_group_host_need_kind kind);

#ifdef __cplusplus
}
#endif

#endif /* ZCL_TEST_GROUP_HOST_NEED_H */
