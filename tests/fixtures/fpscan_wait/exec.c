/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Test-only launcher: make child status unavailable to the scanner. */
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct sigaction ignored = { .sa_handler = SIG_IGN };
    if (argc < 2 || sigemptyset(&ignored.sa_mask) != 0 ||
        sigaction(SIGCHLD, &ignored, NULL) != 0)
        return 2;
    alarm(10);
    execv(argv[1], argv + 1);
    perror("fpscan_wait_exec: execv");
    return 2;
}
