/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "emulator_reap.h"
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

bool zcl_emulator_reap_site(uintptr_t caller, uintptr_t base,
    const char *module, size_t module_length)
{
    if (module == NULL || base == 0 || caller < base || caller - base != 0x52b40eu)
        return false;
    if (module_length >= 4096) return false;
    const char expected[] = "/libandroid-emu-metrics.so";
    const size_t suffix = sizeof(expected) - 1;
    return module_length >= suffix && memchr(module, 0, module_length) == NULL
        && memcmp(module + module_length - suffix, expected, suffix) == 0;
}

pid_t zcl_emulator_reap_wait(zcl_host_wait next, bool accepted_site,
    pid_t child, int *status, int options)
{
    if (next == NULL) { errno = ENOSYS; return -1; }
    if (!accepted_site || child <= 0 || status != NULL || options != WNOHANG)
        return next(child, status, options);
    /* The pinned HostSystem timeout owns this child and has just sent SIGKILL.
     * It discards status and never revisits this PID. Complete its reap before
     * returning; preserve every ordinary poll/wait and its original errno. */
    pid_t result;
    do { result = next(child, NULL, 0); } while (result < 0 && errno == EINTR);
    return result;
}
