/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _GNU_SOURCE
#include "emulator_reap.h"
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Written once by the loader before SDK worker creation; immutable afterward.
 * This Linux ELF adapter is only loaded by the exact-SDK launcher. It must not
 * be injected into arbitrary applications or treated as a general libc shim. */
static zcl_host_wait next_wait;

static void missing_wait(void)
{
    const char message[] = "Emulator reaping: libc waitpid unavailable\n";
    if (write(STDERR_FILENO, message, sizeof(message) - 1) < 0) _exit(125);
    _exit(125);
}

__attribute__((constructor)) static void prepare_wait(void)
{
    void *symbol = dlsym(RTLD_NEXT, "waitpid");
    _Static_assert(sizeof(symbol) == sizeof(next_wait), "Linux ELF function pointer ABI");
    memcpy(&next_wait, &symbol, sizeof(next_wait));
    if (next_wait == NULL) missing_wait();
}

__attribute__((visibility("default"))) pid_t waitpid(pid_t child, int *status, int options)
{
    if (next_wait == NULL) missing_wait();
    bool accepted = false;
    const int saved_errno = errno;
    if (child > 0 && status == NULL && options == WNOHANG) {
        Dl_info info;
        memset(&info, 0, sizeof(info));
        void *caller = __builtin_return_address(0);
        if (dladdr(caller, &info) != 0 && info.dli_fname != NULL)
            accepted = zcl_emulator_reap_site((uintptr_t)caller,
                (uintptr_t)info.dli_fbase, info.dli_fname, strnlen(info.dli_fname, 4096));
    }
    errno = saved_errno;
    return zcl_emulator_reap_wait(next_wait, accepted, child, status, options);
}
