/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "emulator_reap.h"
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Reap fixture at %d\n", __LINE__); abort(); } } while (0)
static unsigned calls, interruptions;
static pid_t expected_pid;
static int *expected_status;
static int expected_options, final_error;

static pid_t provider(pid_t pid, int *status, int options)
{
    CHECK(pid == expected_pid && status == expected_status && options == expected_options);
    CHECK(calls < 8);
    ++calls;
    if (calls <= interruptions) { errno = EINTR; return -1; }
    if (final_error != 0) { errno = final_error; return -1; }
    if (status != NULL) *status = 37 << 8;
    errno = ERANGE;
    return pid;
}

static void configure(pid_t pid, int *status, int options, unsigned interrupted, int error)
{
    expected_pid = pid; expected_status = status; expected_options = options;
    calls = 0; interruptions = interrupted; final_error = error;
}

static bool site(uintptr_t caller, uintptr_t base, const char *module)
{
    return zcl_emulator_reap_site(caller, base, module, module == NULL ? 0 : strnlen(module, 4096));
}

static void sites(void)
{
    const uintptr_t base = 0x1000000u, offset = 0x52b40eu;
    CHECK(site(base + offset, base, "/sdk/lib64/libandroid-emu-metrics.so"));
    CHECK(!site(base + offset - 1, base, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!site(base + offset + 1, base, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!site(base - 1, base, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!site(offset, 0, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!site(base + offset, base, NULL));
    CHECK(!site(base + offset, base, "libandroid-emu-metrics.so"));
    CHECK(!site(base + offset, base, "/sdk/libandroid-emu-metrics.so.extra"));
    CHECK(!site(UINTPTR_MAX, base, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!site(offset - 101u, UINTPTR_MAX - 100u, "/sdk/libandroid-emu-metrics.so"));
    CHECK(!zcl_emulator_reap_site(base + offset, base, NULL, sizeof("/sdk/libandroid-emu-metrics.so") - 1));
}

static void site_paths(void)
{
    static char path[4097];
    memset(path, 'x', sizeof(path));
    CHECK(!site(0x62b40eu, 0x100000u, path));
    const char suffix[] = "/libandroid-emu-metrics.so";
    memcpy(path + 4096 - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1);
    CHECK(!zcl_emulator_reap_site(0x62b40eu, 0x100000u, path, 4096));
    memcpy(path + 4096 - sizeof(suffix), suffix, sizeof(suffix));
    CHECK(site(0x62b40eu, 0x100000u, path));
    CHECK(!site(0x62b40eu, 0x100000u, ""));
    CHECK(!zcl_emulator_reap_site(0x62b40eu, 0x100000u, path, SIZE_MAX));
    CHECK(!zcl_emulator_reap_site(0x62b40eu, 0x100000u, path, 4096));
    path[1] = 0;
    CHECK(!zcl_emulator_reap_site(0x62b40eu, 0x100000u, path, 4095));
}

static void selected_wait(void)
{
    configure(71, NULL, 0, 3, 0);
    CHECK(zcl_emulator_reap_wait(provider, true, 71, NULL, WNOHANG) == 71);
    CHECK(calls == 4 && errno == ERANGE);
    const int errors[] = {ECHILD, ESRCH, EINVAL};
    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        configure(71, NULL, 0, 2, errors[i]);
        CHECK(zcl_emulator_reap_wait(provider, true, 71, NULL, WNOHANG) == -1);
        CHECK(calls == 3 && errno == errors[i]);
    }
    CHECK(zcl_emulator_reap_wait(NULL, true, 71, NULL, WNOHANG) == -1 && errno == ENOSYS);
}

static void untouched_waits(void)
{
    const pid_t pids[] = {-5, -1, 0, 71};
    for (size_t i = 0; i < sizeof(pids) / sizeof(pids[0]); ++i) {
        configure(pids[i], NULL, WNOHANG, 0, 0);
        CHECK(zcl_emulator_reap_wait(provider, false, pids[i], NULL, WNOHANG) == pids[i]);
        CHECK(calls == 1 && errno == ERANGE);
        if (pids[i] > 0) continue;
        configure(pids[i], NULL, WNOHANG, 1, 0);
        CHECK(zcl_emulator_reap_wait(provider, true, pids[i], NULL, WNOHANG) == -1);
        CHECK(calls == 1 && errno == EINTR);
    }
}

static void untouched_status_options(void)
{
    int status = 0;
    configure(71, &status, WNOHANG, 0, 0);
    CHECK(zcl_emulator_reap_wait(provider, true, 71, &status, WNOHANG) == 71);
    CHECK(calls == 1 && status == (37 << 8));
    const int options[] = {0, WNOHANG | WUNTRACED, WNOHANG | WCONTINUED};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        configure(71, NULL, options[i], 1, 0);
        CHECK(zcl_emulator_reap_wait(provider, true, 71, NULL, options[i]) == -1);
        CHECK(calls == 1 && errno == EINTR);
    }
}

static pid_t final_reap(pid_t child, int *status)
{
    pid_t result;
    do { result = waitpid(child, status, 0); } while (result < 0 && errno == EINTR);
    return result;
}

static void real_child(void)
{
    const pid_t parent = getpid(), child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != parent) _exit(2);
        for (;;) pause();
    }
    int status = 0;
    const pid_t poll = zcl_emulator_reap_wait(waitpid, false, child, &status, WNOHANG);
    const int killed = kill(child, SIGKILL);
    const pid_t reaped = zcl_emulator_reap_wait(waitpid, true, child, NULL, WNOHANG);
    /* Cleanup even a broken nonblocking implementation before assertions. */
    if (reaped != child) CHECK(final_reap(child, &status) == child);
    CHECK(poll == 0 && killed == 0 && reaped == child);
    CHECK(waitpid(child, &status, WNOHANG) == -1 && errno == ECHILD);
}

static void *parallel_children(void *unused)
{
    CHECK(unused == NULL);
    for (unsigned i = 0; i < 32; ++i) real_child();
    return NULL;
}

static void parallel_waits(void)
{
    pthread_t workers[4];
    for (size_t i = 0; i < 4; ++i) CHECK(pthread_create(&workers[i], NULL, parallel_children, NULL) == 0);
    for (size_t i = 0; i < 4; ++i) CHECK(pthread_join(workers[i], NULL) == 0);
}

int main(void)
{
    sites(); site_paths(); selected_wait(); untouched_waits(); untouched_status_options();
    for (unsigned i = 0; i < 128; ++i) real_child();
    parallel_waits();
    puts("Reaping preserves ordinary waits, checks the exact SDK site, retries EINTR, and reaps 256 owned killed children.");
    return 0;
}
