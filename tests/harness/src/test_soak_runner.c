/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * CLI path bounds, numeric refusals, help and last-argument precedence.
 * Path probes use a zero interval to stop before log opening or node spawn. */

#include "test/test_core.h"
#include "util/spawn.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#define SOAK_RUNNER_BIN "build/bin/soak_runner"

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
    *timed_out = false;
    if (out_size > 0) out[0] = '\0';
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
            SOAK_RUNNER_BIN, flag, "--rpcport=28777", "--interval-sec=0",
            "--duration-sec=1", "--log=unused-soak-refused.log", NULL};
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
            SOAK_RUNNER_BIN, flag, "--interval-sec=0", "--duration-sec=1",
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
            SOAK_RUNNER_BIN, flag, "--interval-sec=0", "--duration-sec=1",
            "--log=unused-soak-refused.log", NULL};
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
    TEST("soak_runner: --help bypasses deferred path validation") {
        char flag[720];
        int n = snprintf(flag, sizeof(flag), "--log=%s", soak_long_arg());
        ASSERT(n > 0 && (size_t)n < sizeof(flag));
        const char *const argv[] = { SOAK_RUNNER_BIN, flag, "--help", NULL };
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
        ASSERT(rc == 0);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "Usage:") != NULL);
        ASSERT(strstr(buf, "too long") == NULL);
        PASS();
    } _test_next:;
    return failures;
}

/* A zero interval forces a deterministic terminal refusal after the path
 * copies. Even a mutant cannot reach log opening, pidof, RPC or node spawn. */
static int test_soak_runner_path_boundaries(void)
{
    int failures = 0;
    TEST("soak_runner: exact path capacities and last argument precedence") {
        static const struct { const char *flag; size_t cap; } rows[] = {
            {"--log", 256}, {"--node-datadir", 512}, {"--connect", 128},
        };
        for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
            char value[513], flag[540], buf[4096];
            bool timed_out = false;
            memset(value, 'x', rows[i].cap - 1);
            value[rows[i].cap - 1] = '\0';
            int n = snprintf(flag, sizeof(flag), "%s=%s", rows[i].flag, value);
            ASSERT(n > 0 && (size_t)n < sizeof(flag));
            const char *const accepted[] = {
                SOAK_RUNNER_BIN, flag, "--interval-sec=0", NULL};
            ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, accepted) == 2);
            ASSERT(!timed_out);
            ASSERT(strstr(buf, "interval-sec (0) out of range") != NULL);
            ASSERT(strstr(buf, "too long") == NULL);

            value[rows[i].cap - 1] = 'x';
            value[rows[i].cap] = '\0';
            n = snprintf(flag, sizeof(flag), "%s=%s", rows[i].flag, value);
            ASSERT(n > 0 && (size_t)n < sizeof(flag));
            ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, accepted) == 2);
            ASSERT(!timed_out);
            ASSERT(strstr(buf, "too long") != NULL);
            ASSERT(strstr(buf, rows[i].flag) != NULL);

            char replacement[64];
            n = snprintf(replacement, sizeof(replacement), "%s=short", rows[i].flag);
            ASSERT(n > 0 && (size_t)n < sizeof(replacement));
            const char *const replaced[] = {
                SOAK_RUNNER_BIN, flag, replacement, "--interval-sec=0", NULL};
            ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, replaced) == 2);
            ASSERT(!timed_out);
            ASSERT(strstr(buf, "interval-sec (0) out of range") != NULL);
            ASSERT(strstr(buf, "too long") == NULL);

            const char *const final_long[] = {
                SOAK_RUNNER_BIN, replacement, flag, "--interval-sec=0", NULL};
            ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, final_long) == 2);
            ASSERT(!timed_out);
            ASSERT(strstr(buf, "too long") != NULL);
            ASSERT(strstr(buf, rows[i].flag) != NULL);
        }
        PASS();
    } _test_next:;
    return failures;
}
static int test_soak_runner_numeric_refused(void)
{
    int failures = 0;
    char dir[256] = {0};
    TEST("soak_runner: empty numeric flags refuse before log opening") {
        ASSERT(test_mkdtemp(dir, sizeof(dir), "soak_numeric") != NULL);
        char log_flag[280], path[272], buf[4096];
        bool timed_out = false;
        int n = snprintf(path, sizeof(path), "%s/run.log", dir);
        ASSERT(n > 0 && (size_t)n < sizeof(path));
        n = snprintf(log_flag, sizeof(log_flag), "--log=%s", path);
        ASSERT(n > 0 && (size_t)n < sizeof(log_flag));
        const char *const exact[] = {SOAK_RUNNER_BIN, "--duration-sec=1",
            "--duration-sec=", "--interval-sec=1", log_flag, "--help", NULL};
        int rc = soak_run_merged(buf, sizeof(buf), &timed_out, exact);
        struct stat st;
        bool absent = lstat(path, &st) != 0 && errno == ENOENT;
        ASSERT(!timed_out);
        ASSERT(rc == 2);
        ASSERT(strstr(buf, "invalid numeric argument: --duration-sec=") != NULL);
        ASSERT(absent);
        static const char *const flags[] = {
            "--duration-sec=", "--interval-sec=", "--stall-sec=",
            "--warmup-sec=", "--rss-growth-mib=",
        };
        for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
            const char *const argv[] = {SOAK_RUNNER_BIN, flags[i],
                "--interval-sec=1", log_flag, "--help", NULL};
            rc = soak_run_merged(buf, sizeof(buf), &timed_out, argv);
            ASSERT(!timed_out);
            ASSERT(rc == 2);
            ASSERT(strstr(buf, "invalid numeric argument:") != NULL);
            ASSERT(strstr(buf, flags[i]) != NULL);
            ASSERT(lstat(path, &st) != 0 && errno == ENOENT);
        }
        PASS();
    } _test_next:;
    if (dir[0]) test_cleanup_tmpdir(dir);
    return failures;
}

static int test_soak_runner_numeric_precedence(void)
{
    int failures = 0;
    char dir[256] = {0};
    TEST("soak_runner: valid duration duplicates and ordered help routing") {
        ASSERT(test_mkdtemp(dir, sizeof(dir), "soak_precedence") != NULL);
        char log_flag[280], buf[4096];
        bool timed_out = false;
        int n = snprintf(log_flag, sizeof(log_flag), "--log=%s", dir);
        ASSERT(n > 0 && (size_t)n < sizeof(log_flag));
        const char *const longer[] = {SOAK_RUNNER_BIN, "--duration-sec=1",
            "--duration-sec=3", "--interval-sec=2", log_flag, NULL};
        ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, longer) == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "cannot open log") != NULL);
        const char *const shorter[] = {SOAK_RUNNER_BIN, "--duration-sec=3",
            "--duration-sec=1", "--interval-sec=2", log_flag, NULL};
        ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, shorter) == 2);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "interval-sec (2) out of range") != NULL);
        const char *const help[] = {SOAK_RUNNER_BIN, "--help",
            "--duration-sec=", log_flag, NULL};
        ASSERT(soak_run_merged(buf, sizeof(buf), &timed_out, help) == 0);
        ASSERT(!timed_out);
        ASSERT(strstr(buf, "Usage:") != NULL);
        PASS();
    } _test_next:;
    if (dir[0]) test_cleanup_tmpdir(dir);
    return failures;
}
#endif

int test_soak_runner(void)
{
#if defined(_WIN32)
    printf("soak_runner: POSIX-only runner unavailable on Windows\n");
    return 0;
#else
    int failures = 0;
    struct stat st;
    if (stat(SOAK_RUNNER_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("soak_runner: " SOAK_RUNNER_BIN " missing (run: "
               "make soak_runner)\n");
        return 1;
    }
    failures += test_soak_runner_datadir_refused();
    failures += test_soak_runner_log_refused();
    failures += test_soak_runner_connect_refused();
    failures += test_soak_runner_help_still_works();
    failures += test_soak_runner_path_boundaries();
    failures += test_soak_runner_numeric_refused();
    failures += test_soak_runner_numeric_precedence();
    return failures;
#endif
}
