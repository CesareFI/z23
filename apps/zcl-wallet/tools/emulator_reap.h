/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_EMULATOR_REAP_H
#define ZCL_EMULATOR_REAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef pid_t (*zcl_host_wait)(pid_t, int *, int);
/* Host test equipment only. The caller owns this exact child exclusively.
 * An accepted site is the pinned SDK's post-SIGKILL discard-status wait.
 * No allocation, signal handler, retained PID or opportunistic child reaper.
 * A killed child can remain in uninterruptible kernel cleanup: this operation
 * has no wall-clock completion guarantee; bound the enclosing fixture run. */
bool zcl_emulator_reap_site(uintptr_t caller, uintptr_t base,
    const char *module, size_t module_length);
pid_t zcl_emulator_reap_wait(zcl_host_wait next, bool accepted_site,
    pid_t child, int *status, int options);
#endif
