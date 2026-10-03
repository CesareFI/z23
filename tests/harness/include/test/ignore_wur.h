/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Stated-reason discard for libc calls declared warn_unused_result (glibc
 * marks write/read/system/chdir/truncate/symlink and friends __wur).
 *
 * Unlike C23 [[nodiscard]], a `(void)` cast does NOT silence this
 * attribute — GCC still warns — so the result must be bound before it is
 * dropped. The `reason` argument is enforced non-empty at compile time and
 * documents why the failure is safe to drop at that one site, matching
 * the discipline of ZCL_IGNORE_RESULT in base/result.h.
 */
#ifndef ZCL_TEST_IGNORE_WUR_H
#define ZCL_TEST_IGNORE_WUR_H

#define ZCL_IGNORE_WUR(call, reason)                                     \
    do {                                                                 \
        static_assert(sizeof("" reason) > 1,                             \
                      "ZCL_IGNORE_WUR requires a non-empty reason");     \
        typeof(call) zcl_wur_rc_ = (call);                               \
        (void)zcl_wur_rc_;                                               \
    } while (0)

#endif /* ZCL_TEST_IGNORE_WUR_H */
