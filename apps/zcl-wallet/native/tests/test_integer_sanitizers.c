/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Deliberately faulty, process-isolated host probes. The ordinary UBSan control
 * returns0; the additional integer checks must diagnose and stop each probe.
 * No wallet/provider code or private input enters this executable. */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int unsigned_wrap(void)
{
    volatile size_t value = SIZE_MAX;
    ++value;
    return value == 0 ? 0 : 1;
}

static int unsigned_truncation(void)
{
    volatile unsigned char value = UCHAR_MAX;
    ++value;
    return value == 0 ? 0 : 1;
}

static int signed_conversion(void)
{
    volatile signed char value = SCHAR_MAX;
    ++value;
    return value == SCHAR_MIN ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && argv[1][0] != '\0' && argv[1][1] == '\0') {
        if (argv[1][0] == '0') return unsigned_wrap();
        if (argv[1][0] == '1') return unsigned_truncation();
        if (argv[1][0] == '2') return signed_conversion();
    }
    fputs("Invalid integer sanitizer probe argument\n", stderr);
    return 2;
}
