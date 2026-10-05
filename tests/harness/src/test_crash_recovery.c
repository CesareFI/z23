/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * test_crash_recovery — regressions for the crash-recovery harness's
 * path-argument handling (tools/crash_recovery_test.c ->
 * build/bin/crash_recovery_test).
 *
 * The harness copies --datadir/--connect into fixed buffers with snprintf
 * and never checked the would-have-written length. A truncated datadir
 * reached sqlite3_open_v2, popen's stderr redirect and the spawned node's
 * -datadir; a truncated connect reached -connect. These cases pin the
 * refusal: an over-long path argument exits 2 naming the flag before any
 * spawn or open, and ordinary arguments keep working. */

#include "test/test_core.h"
#include "util/spawn.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define CRASH_RECOVERY_BIN "build/bin/crash_recovery_test.exe"
#else
#define CRASH_RECOVERY_BIN "build/bin/crash_recovery_test"
#endif

/* 699 chars: over every fixed path buffer the harness copies into. */
static const char *cr_long_arg(void)
{
    static char arg[700];
    memset(arg, 'x', sizeof(arg) - 1);
    arg[sizeof(arg) - 1] = '\0';
    return arg;
}

/* Run the harness with merged stdout+stderr (the refusal goes to
 * stderr); returns the exit status, or -1 on a spawn failure. */
static int cr_run_merged(char *out, size_t out_size, bool *timed_out,
                         const char *const argv[])
{
    return zcl_spawn_capture_merged_observed(argv, out, out_size, 30000,
                                             timed_out);
}

static int test_crash_recovery_datadir_refused(void)
{
    int failures = 0;
    char buf[4096] = {0};
    char flag[720] = {0};
    bool timed_out = false;
    int n = snprintf(flag, sizeof(flag), "--datadir=%s", cr_long_arg());
    TEST("crash_recovery: over-long --datadir refused before any spawn") {
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = {
            CRASH_RECOVERY_BIN, flag, "--regtest", "--iterations=1",
            "--rpc-port=29777", NULL};
        int rc = cr_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "too long") != NULL);
        ASSERT(strstr(buf, "--datadir") != NULL);
        PASS();
    } _test_next:;
    return failures;
}

static int test_crash_recovery_connect_refused(void)
{
    int failures = 0;
    char buf[4096] = {0};
    char flag[720] = {0};
    bool timed_out = false;
    int n = snprintf(flag, sizeof(flag), "--connect=%s", cr_long_arg());
    TEST("crash_recovery: over-long --connect refused before any spawn") {
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = {
            CRASH_RECOVERY_BIN, flag, "--regtest", "--iterations=1",
            "--rpc-port=29778", NULL};
        int rc = cr_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "too long") != NULL);
        ASSERT(strstr(buf, "--connect") != NULL);
        PASS();
    } _test_next:;
    return failures;
}

static int test_crash_recovery_help_still_works(void)
{
    int failures = 0;
    char buf[4096] = {0};
    bool timed_out = false;
    TEST("crash_recovery: --help still exits 0") {
        const char *const argv[] = { CRASH_RECOVERY_BIN, "--help", NULL };
        int rc = cr_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 0);
        ASSERT(!timed_out);
        PASS();
    } _test_next:;
    return failures;
}

int test_crash_recovery(void)
{
    int failures = 0;
    struct stat st;
    if (stat(CRASH_RECOVERY_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("crash_recovery: " CRASH_RECOVERY_BIN " missing (run: "
               "make crash_recovery_test) — skipped\n");
        return 0;
    }
    failures += test_crash_recovery_datadir_refused();
    failures += test_crash_recovery_connect_refused();
    failures += test_crash_recovery_help_still_works();
    return failures;
}
