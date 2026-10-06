/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Linux LD_PRELOAD probe that turns the FIRST fread(3) call of the
 *          process into a short read (returns 0). Loaded into
 *          build/bin/z23-clang-manifest by the semantic_sensor short-read
 *          case, it deterministically reproduces the cm_read_file failure
 *          mode that a real filesystem race only hits by luck: ftell sized
 *          the buffer, the allocation succeeded, and the read then came up
 *          empty. The sensor must report that manifest as "unreadable" and
 *          must never print an uninitialized reason. Keyed on
 *          CM_SHORT_READ=1; without the variable every call passes through. */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static size_t (*cm_real_fread)(void *ptr, size_t size, size_t nmemb,
                               FILE *stream);
static int cm_armed;
static int cm_fired;

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (cm_real_fread == NULL) {
        *(void **)&cm_real_fread = dlsym(RTLD_NEXT, "fread");
        if (cm_real_fread == NULL)
            return 0;
    }
    if (cm_armed && !cm_fired) {
        cm_fired = 1;
        (void)ptr;
        (void)size;
        (void)nmemb;
        (void)stream;
        return 0;
    }
    return cm_real_fread(ptr, size, nmemb, stream);
}

__attribute__((constructor)) static void cm_short_read_arm(void)
{
    const char *env = getenv("CM_SHORT_READ");
    cm_armed = env != NULL && env[0] == '1' && env[1] == '\0';
}
