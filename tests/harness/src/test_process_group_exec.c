/* Copyright 2026 Rhett Creighton - Apache License 2.0.
 *
 * Regression: a local fixture launched through process-group-exec with
 * --die-with-parent must not outlive the shell that launched it. The
 * orphan this pins was real: a journey driver died mid-run, its regtest
 * daemon kept a shared test-safe port, and every later journey on the
 * host failed at bring-up until a human found the holder by hand. The
 * kernel-side parent-death signal closes that class for local fixtures
 * without disturbing the remote journey legs, which rely on plain
 * setsid survival between the driver's separate ssh calls. Both
 * properties are asserted: armed means reaped, unarmed means survives.
 *
 * Non-Linux POSIX hosts have no PR_SET_PDEATHSIG; there the launcher
 * must say so on stderr (an explicit limitation, never a silent
 * pretend-fix) and the armed case degrades to survival. Windows runs
 * the stronger Job Object acceptance binary instead of this group.
 */

#include "test/test_core.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if !defined(_WIN32)

static const char *pge_launcher(void)
{
    static const char *candidates[] = {
        "build/bin/process-group-exec", "./build/bin/process-group-exec",
        NULL};
    for (size_t i = 0; candidates[i]; i++)
        if (access(candidates[i], X_OK) == 0)
            return candidates[i];
    return NULL;
}

/* Run one launcher invocation under a disposable shell parent that
 * records the launcher pid. Returns the shell pid; the launcher pid is
 * written to pidfile by the shell. */
static pid_t pge_spawn(const char *launcher, const char *flag,
                       const char *pidfile)
{
    char script[1024];
    int n = snprintf(script, sizeof(script),
                     "'%s' %s sleep 60 & echo $! > '%s'; wait", launcher,
                     flag ? flag : "", pidfile);
    if (n <= 0 || (size_t)n >= sizeof(script))
        return 0;
    pid_t shell = fork();
    if (shell < 0)
        return 0;
    if (shell == 0) {
        execl("/bin/sh", "sh", "-c", script, (char *)NULL);
        _exit(127);
    }
    return shell;
}

static int pge_read_pid(const char *pidfile, pid_t *out)
{
    FILE *f = fopen(pidfile, "r");
    if (!f)
        return 0;
    long v = 0;
    int ok = fscanf(f, "%ld", &v) == 1 && v > 0;
    fclose(f);
    if (ok)
        *out = (pid_t)v;
    return ok;
}

static int pge_alive(pid_t p)
{
    return p > 0 && (kill(p, 0) == 0 || errno != ESRCH);
}

static int pge_wait_gone(pid_t p, int seconds)
{
    for (int i = 0; i < seconds * 10; i++) {
        if (!pge_alive(p))
            return 1;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
        nanosleep(&ts, NULL); /* real-clock: observing the kernel deliver a
                               * parent-death SIGTERM to a real child; no
                               * fake-clock seam spans processes */
    }
    return !pge_alive(p);
}

static int pge_wait_file(const char *pidfile, pid_t *out)
{
    for (int i = 0; i < 50; i++) {
        if (pge_read_pid(pidfile, out))
            return 1;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
        /* Real /bin/sh fork writing the pidfile through the kernel's
         * filesystem — no injected clock spans processes. */
        nanosleep(&ts, NULL); /* real-clock: real fork+filesystem visibility */
    }
    return 0;
}

static int test_die_with_parent_reaps_fixture(void)
{
    int failures = 0;
#if defined(__linux__)
    TEST("process-group-exec: --die-with-parent reaps the fixture when "
         "the launching shell dies") {
        const char *launcher = pge_launcher();
        ASSERT(launcher != NULL);
        char dir[] = "test-tmp/pge_dwp_XXXXXX";
        ASSERT(mkdtemp(dir) != NULL);
        char pidfile[300];
        ASSERT(snprintf(pidfile, sizeof(pidfile), "%s/pid", dir) > 0);
        pid_t shell = pge_spawn(launcher, "--die-with-parent", pidfile);
        ASSERT(shell > 0);
        pid_t fixture = 0;
        ASSERT(pge_wait_file(pidfile, &fixture));
        ASSERT(pge_alive(fixture));
        /* Kill the driver exactly as a crashed harness dies. */
        ASSERT(kill(shell, SIGKILL) == 0);
        int status = 0;
        ASSERT(waitpid(shell, &status, 0) == shell);
        ASSERT(pge_wait_gone(fixture, 5));
        unlink(pidfile);
        rmdir(dir);
        PASS();
    }
    _test_next:;
#else
    TEST("process-group-exec: --die-with-parent reports the platform "
         "limitation instead of pretending") {
        const char *launcher = pge_launcher();
        ASSERT(launcher != NULL);
        char line[512];
        int n = snprintf(line, sizeof(line),
                         "'%s' --die-with-parent true 2>&1", launcher);
        ASSERT(n > 0 && (size_t)n < sizeof(line));
        FILE *p = popen(line, "r");
        ASSERT(p != NULL);
        bool named = false;
        while (fgets(line, sizeof(line), p))
            if (strstr(line, "unavailable on this platform"))
                named = true;
        int rc = pclose(p);
        ASSERT(named);
        ASSERT(WIFEXITED(rc) && WEXITSTATUS(rc) == 0);
        PASS();
    }
    _test_next:;
#endif
    return failures;
}

static int test_plain_setsid_survives(void)
{
    int failures = 0;
    TEST("process-group-exec: without the flag the fixture survives its "
         "launching shell") {
        const char *launcher = pge_launcher();
        ASSERT(launcher != NULL);
        char dir[] = "test-tmp/pge_plain_XXXXXX";
        ASSERT(mkdtemp(dir) != NULL);
        char pidfile[300];
        ASSERT(snprintf(pidfile, sizeof(pidfile), "%s/pid", dir) > 0);
        pid_t shell = pge_spawn(launcher, NULL, pidfile);
        ASSERT(shell > 0);
        pid_t fixture = 0;
        ASSERT(pge_wait_file(pidfile, &fixture));
        ASSERT(pge_alive(fixture));
        ASSERT(kill(shell, SIGKILL) == 0);
        int status = 0;
        ASSERT(waitpid(shell, &status, 0) == shell);
        /* Still alive one second later: remote legs depend on this. */
        struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};
        nanosleep(&ts, NULL); /* real-clock: proving the kernel did NOT send
                               * the parent-death signal requires waiting on
                               * the real scheduler; absence has no seam */
        ASSERT(pge_alive(fixture));
        ASSERT(kill(fixture, SIGTERM) == 0);
        ASSERT(pge_wait_gone(fixture, 5));
        unlink(pidfile);
        rmdir(dir);
        PASS();
    }
    _test_next:;
    return failures;
}

static int test_unknown_flag_fails_closed(void)
{
    int failures = 0;
    TEST("process-group-exec: an unknown flag is a usage error, never an "
         "executed filename") {
        const char *launcher = pge_launcher();
        ASSERT(launcher != NULL);
        char line[512];
        int n = snprintf(line, sizeof(line), "'%s' --bogus-flag true",
                         launcher);
        ASSERT(n > 0 && (size_t)n < sizeof(line));
        FILE *p = popen(line, "r");
        ASSERT(p != NULL);
        while (fgets(line, sizeof(line), p)) {
        }
        int rc = pclose(p);
        ASSERT(rc != 0);
        int exit_code = WEXITSTATUS(rc);
        ASSERT_EQ(exit_code, 2);
        PASS();
    }
    _test_next:;
    return failures;
}

int test_process_group_exec(void)
{
    int failures = 0;
    failures += test_die_with_parent_reaps_fixture();
    failures += test_plain_setsid_survives();
    failures += test_unknown_flag_fails_closed();
    return failures;
}

#else /* _WIN32 */

int test_process_group_exec(void)
{
    int failures = 0;
    TEST("process-group-exec: POSIX parent-death legs covered by the "
         "Windows Job Object acceptance") {
        printf("  SKIP (process-group-exec posix legs) windows host runs "
               "the job-object acceptance binary\n");
        PASS();
    }
    _test_next:;
    return failures;
}

#endif
