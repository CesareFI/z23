/* Copyright 2026 CesareFI. Licensed under Apache-2.0.
 * Test fixture: inject a directory-read error into the real generator. */
#if !defined(_WIN32)
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static struct dirent *fault_readdir(DIR *dir);
#define readdir fault_readdir
#define main generator_main
#include "../../../tools/gen_templates.c"
#undef main
#undef readdir

static int fail_next;

static struct dirent *fault_readdir(DIR *dir)
{
    if (fail_next) {
        fputs("gen_templates_readdir_fault: injected EIO\n", stderr);
        errno = EIO;
        return NULL;
    }
    struct dirent *entry = readdir(dir);
    if (entry) {
        size_t len = strlen(entry->d_name);
        if (len >= 6 && strcmp(entry->d_name + len - 6, ".chtml") == 0)
            fail_next = 1;
    }
    return entry;
}

int main(int argc, char **argv)
{
    return generator_main(argc, argv);
}
#else
int gen_templates_readdir_fault_unavailable(void) { return 0; }
#endif
