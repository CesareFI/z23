/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Bounded, filename-bound package-swarm recovery enumeration. */

#ifndef ZCL_VCS_PACKAGE_SWARM_RESUME_H
#define ZCL_VCS_PACKAGE_SWARM_RESUME_H

#include "package_swarm_record.h"

#include <stdbool.h>

/* Called in canonical root order only after the complete bounded directory
 * view has been observed. All pointer arguments are borrowed for the call. */
typedef bool (*vcs_swarm_resume_apply_fn)(
    const struct vcs_swarm_record *record, const char *root_hex,
    const char *record_path, void *ctx);

/* Missing downloads/ is an empty successful recovery. Hard directory I/O,
 * an over-bound view, or a callback refusal returns false. Corrupt, linked,
 * and root/name-mismatched leaves are removed and skipped. */
bool vcs_swarm_resume_scan(const char *zcode_dir,
                           vcs_swarm_resume_apply_fn apply, void *ctx);

#endif /* ZCL_VCS_PACKAGE_SWARM_RESUME_H */
