/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Start one command as the leader of a private process group. On POSIX this
 * is setsid(2)/setpgid(2) then execvp. On Windows the equivalent bounded
 * tree is a kill-on-close Job Object: this process stays the supervisor,
 * so the harness PID is the job owner, and closing it reaps descendants.
 */

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Append `count` copies of ch to out[*used..], never writing past cap-1
 * (the final NUL slot). Factored out of quote_arg() so its repeated
 * bounded-append loops do not each add to that function's own complexity. */
static void append_repeated(char *out, size_t cap, size_t *used, char ch,
                            size_t count)
{
    for (size_t i = 0; i < count && *used + 1 < cap; i++)
        out[(*used)++] = ch;
}

/* MS command-line quoting escapes a literal '"' as (2n+1) backslashes plus
 * a quote, where n is the run of backslashes immediately preceding it. */
static void quote_arg_emit_escaped_quote(char *out, size_t cap, size_t *used,
                                         size_t slashes)
{
    append_repeated(out, cap, used, '\\', slashes * 2u + 1u);
    append_repeated(out, cap, used, '"', 1);
}

static size_t quote_arg(char *out, size_t cap, const char *arg)
{
    size_t used = 0, slashes = 0;
    bool quote = !arg[0] || strpbrk(arg, " \t\n\v\"") != NULL;
    if (quote) append_repeated(out, cap, &used, '"', 1);
    for (const char *p = arg;; p++) {
        if (*p == '\\') { slashes++; continue; }
        if (*p == '"') {
            quote_arg_emit_escaped_quote(out, cap, &used, slashes);
        } else {
            if (*p == 0 && quote) slashes *= 2u;
            append_repeated(out, cap, &used, '\\', slashes);
            if (*p == 0) break;
            append_repeated(out, cap, &used, *p, 1);
        }
        slashes = 0;
    }
    if (quote) append_repeated(out, cap, &used, '"', 1);
    if (cap) out[used < cap ? used : cap - 1] = 0;
    return used;
}

static bool build_command(int argc, char **argv, char *out, size_t cap)
{
    size_t used = 0;
    if (!out || cap < 2) return false;
    out[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            if (used + 1 >= cap) return false;
            out[used++] = ' ';
        }
        size_t wrote = quote_arg(out + used, cap - used, argv[i]);
        if (wrote + used + 1 >= cap) return false;
        used += wrote;
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "process-group-exec: usage: process-group-exec COMMAND [ARG ...]\n");
        return 2;
    }

    char command[32768];
    if (!build_command(argc, argv, command, sizeof(command))) {
        fprintf(stderr, "process-group-exec: command line is too long\n");
        return 2;
    }

    HANDLE job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job ||
        !SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits))) {
        if (job) CloseHandle(job);
        fprintf(stderr, "process-group-exec: cannot create job object (win32=%lu)\n",
                (unsigned long)GetLastError());
        return 126;
    }

    STARTUPINFOA startup = {0};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process = {0};
    BOOL started = CreateProcessA(
        NULL, command, NULL, NULL, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
    if (!started) {
        fprintf(stderr, "process-group-exec: cannot execute %s (win32=%lu)\n",
                argv[1], (unsigned long)GetLastError());
        CloseHandle(job);
        return 127;
    }
    if (!AssignProcessToJobObject(job, process.hProcess)) {
        fprintf(stderr, "process-group-exec: cannot assign job (win32=%lu)\n",
                (unsigned long)GetLastError());
        (void)TerminateProcess(process.hProcess, 126);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(job);
        return 126;
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) {
        fprintf(stderr, "process-group-exec: cannot resume child (win32=%lu)\n",
                (unsigned long)GetLastError());
        (void)TerminateProcess(process.hProcess, 126);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(job);
        return 126;
    }
    CloseHandle(process.hThread);

    DWORD wait = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    if (wait == WAIT_OBJECT_0)
        (void)GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(job);
    return (int)exit_code;
}

#else

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

int main(int argc, char **argv)
{
    int die_with_parent = 0;
    int first = 1;
    if (argc < 2) {
        fprintf(stderr,
                "process-group-exec: usage: process-group-exec "
                "[--die-with-parent] COMMAND [ARG ...]\n");
        return 2;
    }
    /* Exactly one optional leading flag; anything else that starts with
     * '-' is a usage error rather than a silently executed filename, so
     * a misspelled flag fails closed instead of launching the wrong
     * thing. */
    if (argv[1][0] == '-') {
        if (strcmp(argv[1], "--die-with-parent") == 0) {
            die_with_parent = 1;
            first = 2;
        } else {
            fprintf(stderr,
                    "process-group-exec: unknown flag %s "
                    "(usage: process-group-exec [--die-with-parent] "
                    "COMMAND [ARG ...])\n",
                    argv[1]);
            return 2;
        }
        if (argc < 3) {
            fprintf(stderr,
                    "process-group-exec: --die-with-parent needs COMMAND\n");
            return 2;
        }
    }

    if (setsid() < 0 && setpgid(0, 0) < 0) {
        fprintf(stderr, "process-group-exec: cannot create process group: %s\n",
                strerror(errno));
        return 126;
    }

    if (die_with_parent) {
#if defined(__linux__) && defined(PR_SET_PDEATHSIG)
        /* The kernel reaps this whole tree when the launching shell dies,
         * so a crashed driver cannot strand a fixture daemon on a shared
         * port. Deliberately NOT the default: the remote journey legs
         * rely on plain setsid survival between the driver's ssh calls.
         * Applied before exec and after setsid; prctl survives both. */
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0) {
            fprintf(stderr,
                    "process-group-exec: cannot request parent-death "
                    "signal: %s\n",
                    strerror(errno));
            return 126;
        }
        /* Race closure: if the parent died between fork and prctl, the
         * signal was never armed for that death. Check once now. */
        if (getppid() == 1) {
            fprintf(stderr,
                    "process-group-exec: parent already exited before "
                    "parent-death signal was armed\n");
            return 125;
        }
#else
        /* Portable honesty: no kernel parent-death supervision here. The
         * caller's own cleanup and the port diagnostic carry the orphan
         * case on this platform. */
        fprintf(stderr,
                "process-group-exec: --die-with-parent is unavailable on "
                "this platform; continuing without parent-death "
                "supervision\n");
#endif
    }

    execvp(argv[first], &argv[first]);
    fprintf(stderr, "process-group-exec: cannot execute %s: %s\n", argv[first],
            strerror(errno));
    return 127;
}

#endif
