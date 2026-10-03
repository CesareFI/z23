/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * test_soak_runner — regressions for the soak runner's path-argument
 * handling (tools/soak/main.c -> build/bin/soak_runner).
 *
 * The runner copies --log, --node-datadir and --connect into fixed buffers
 * with snprintf and never checked the would-have-written length. Unguarded,
 * an over-long argument silently truncates: the soak then logs to a
 * 255-byte stranger filename (exactly NAME_MAX, so fopen SUCCEEDS), or
 * spawns the isolated node with a chopped datadir/connect string and only
 * fails after the spawn-ready wait burns its full timeout. These cases pin
 * the refusal: an over-long path argument must exit 2 naming the flag,
 * before any log open or node spawn, and ordinary arguments keep working. */

#include "test/test_core.h"
#include "util/spawn.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define SOAK_RUNNER_BIN "build/bin/soak_runner.exe"
#else
#define SOAK_RUNNER_BIN "build/bin/soak_runner"
#endif

/* 699 chars: over every fixed path buffer the runner copies into. */
static const char *soak_long_arg(void)
{
    static char arg[700];
    memset(arg, 'x', sizeof(arg) - 1);
    arg[sizeof(arg) - 1] = '\0';
    return arg;
}

/* Run the soak runner with merged stdout+stderr (the refusal goes to
 * stderr); returns the exit status, or -1 on a spawn failure. */
static int soak_run_merged(char *out, size_t out_size, bool *timed_out,
                           const char *const argv[])
{
    return zcl_spawn_capture_merged_observed(argv, out, out_size, 30000,
                                             timed_out);
}

static int test_soak_runner_datadir_refused(void)
{
    int failures = 0;
    char buf[4096] = {0};
    char flag[720] = {0};
    bool timed_out = false;
    int n = snprintf(flag, sizeof(flag), "--node-datadir=%s", soak_long_arg());
    TEST("soak_runner: over-long --node-datadir refused before any spawn") {
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = {
            SOAK_RUNNER_BIN, flag, "--rpcport=28777", "--interval-sec=1",
            "--duration-sec=1", "--log=/tmp/soak_runner_refused.log", NULL};
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "too long") != NULL);
        ASSERT(strstr(buf, "--node-datadir") != NULL);
        PASS();
    } _test_next:;
    return failures;
}

static int test_soak_runner_log_refused(void)
{
    int failures = 0;
    char buf[4096] = {0};
    char flag[720] = {0};
    bool timed_out = false;
    int n = snprintf(flag, sizeof(flag), "--log=%s", soak_long_arg());
    TEST("soak_runner: over-long --log refused instead of logging elsewhere") {
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = {
            SOAK_RUNNER_BIN, flag, "--interval-sec=1", "--duration-sec=1",
            NULL};
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "too long") != NULL);
        ASSERT(strstr(buf, "--log") != NULL);
        PASS();
    } _test_next:;
    return failures;
}

static int test_soak_runner_connect_refused(void)
{
    int failures = 0;
    char buf[4096] = {0};
    char flag[720] = {0};
    bool timed_out = false;
    int n = snprintf(flag, sizeof(flag), "--connect=%s", soak_long_arg());
    TEST("soak_runner: over-long --connect refused instead of truncating it") {
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = {
            SOAK_RUNNER_BIN, flag, "--interval-sec=1", "--duration-sec=1",
            "--log=/tmp/soak_runner_refused.log", NULL};
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "too long") != NULL);
        ASSERT(strstr(buf, "--connect") != NULL);
        PASS();
    } _test_next:;
    return failures;
}

static int test_soak_runner_help_still_works(void)
{
    int failures = 0;
    char buf[4096] = {0};
    bool timed_out = false;
    TEST("soak_runner: --help still exits 0") {
        const char *const argv[] = { SOAK_RUNNER_BIN, "--help", NULL };
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 0);
        ASSERT(!timed_out);
        PASS();
    } _test_next:;
    return failures;
}

int test_soak_runner(void)
{
    int failures = 0;
    struct stat st;
    if (stat(SOAK_RUNNER_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("soak_runner: " SOAK_RUNNER_BIN " missing (run: "
               "make soak_runner) — skipped\n");
        return 0;
    }
    failures += test_soak_runner_datadir_refused();
    failures += test_soak_runner_log_refused();
    failures += test_soak_runner_connect_refused();
    failures += test_soak_runner_help_still_works();
    return failures;
}
