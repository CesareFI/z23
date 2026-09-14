/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _GNU_SOURCE
#include "emulator_reap.h"
/* The adapter's caller declarations retain libc's nonnull attributes. Do not
 * attach them to the fault provider definitions: GCC would reject, and an
 * optimizer could remove, their deliberate runtime argument assertions. */
#undef dlsym
#undef dladdr
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Reap adapter fixture at %d\n", __LINE__); abort(); } } while (0)
static unsigned lookup_mode, lookups, waits, interrupts;
static pid_t expected_pid;
static int *expected_status;
static int expected_options;

static pid_t fixture_wait(pid_t child, int *status, int options)
{
    CHECK(child == expected_pid && status == expected_status && options == expected_options);
    CHECK(errno == (waits == 0 ? EDOM : EINTR));
    if (waits++ < interrupts) { errno = EINTR; return -1; }
    if (status != NULL) *status = 29 << 8;
    errno = ECHILD;
    return child;
}

void *zcl_fixture_dlsym(void *handle, const char *name)
{
    CHECK(handle == RTLD_NEXT && name != NULL && strcmp(name, "waitpid") == 0);
#ifdef ZCL_REAP_MISSING_SYMBOL
    (void)fixture_wait;
    return NULL;
#else
    zcl_host_wait function = fixture_wait;
    void *result = NULL;
    _Static_assert(sizeof(result) == sizeof(function), "Linux ELF function pointer ABI");
    memcpy(&result, &function, sizeof(result));
    return result;
#endif
}

int zcl_fixture_dladdr(const void *caller, Dl_info *info)
{
    CHECK(caller != NULL && info != NULL && (uintptr_t)caller > 0x52b40eu);
    ++lookups;
    CHECK(info->dli_fbase == NULL && info->dli_fname == NULL);
    info->dli_fbase = (void *)((uintptr_t)caller - 0x52b40eu);
    info->dli_fname = "/sdk/lib64/libandroid-emu-metrics.so";
    errno = EIO; /* Loader lookup must not replace the real wait's input errno. */
    if (lookup_mode == 2) info->dli_fname = "/sdk/lib64/other.so";
    if (lookup_mode == 3) info->dli_fbase = (void *)((uintptr_t)caller - 0x52b40fu);
    if (lookup_mode == 4) info->dli_fbase = NULL;
    if (lookup_mode == 5) info->dli_fname = NULL;
    return lookup_mode != 1;
}

static void configure(unsigned mode, pid_t child, int *status, int options, unsigned interrupted)
{
    lookup_mode = mode; lookups = 0; waits = 0; interrupts = interrupted;
    expected_pid = child; expected_status = status; expected_options = options;
    errno = EDOM;
}

static void lookup_results(void)
{
    configure(0, 19, NULL, 0, 3);
    CHECK(waitpid(19, NULL, WNOHANG) == 19);
    CHECK(lookups == 1 && waits == 4 && errno == ECHILD);
    for (unsigned mode = 1; mode <= 5; ++mode) {
        configure(mode, 19, NULL, WNOHANG, 1);
        CHECK(waitpid(19, NULL, WNOHANG) == -1);
        CHECK(lookups == 1 && waits == 1 && errno == EINTR);
    }
}

static void ordinary_waits(void)
{
    int status = 0;
    configure(0, 19, &status, WNOHANG, 0);
    CHECK(waitpid(19, &status, WNOHANG) == 19);
    CHECK(lookups == 0 && waits == 1 && status == (29 << 8) && errno == ECHILD);
    const pid_t pids[] = {-5, -1, 0};
    for (size_t i = 0; i < sizeof(pids) / sizeof(pids[0]); ++i) {
        configure(0, pids[i], NULL, WNOHANG, 1);
        CHECK(waitpid(pids[i], NULL, WNOHANG) == -1);
        CHECK(lookups == 0 && waits == 1 && errno == EINTR);
    }
    const int options[] = {0, WNOHANG | WCONTINUED, WNOHANG | WUNTRACED};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        configure(0, 19, NULL, options[i], 1);
        CHECK(waitpid(19, NULL, options[i]) == -1);
        CHECK(lookups == 0 && waits == 1 && errno == EINTR);
    }
}

int main(void)
{
    lookup_results(); ordinary_waits();
    puts("Loader adapter preserves metadata failures, errno, status and ordinary polling.");
    return 0;
}
