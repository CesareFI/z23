/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Exercise the file-local process waiter without scanner allocations. */
#define main fpscan_scan_main
#include "../../../tools/fingerprint_scan.c"
#undef main

int main(void)
{
    pid_t child = fork();
    if (child < 0)
        return 2;
    if (child == 0)
        _exit(7);
    if (fp_wait(child) != 7)
        return 1;

    child = fork();
    if (child < 0)
        return 2;
    if (child == 0) {
        alarm(3);
        (void)fp_wait(-1);
        _exit(0);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child)
        return 2;
    return WIFEXITED(status) && WEXITSTATUS(status) == 2 ? 0 : 1;
}
