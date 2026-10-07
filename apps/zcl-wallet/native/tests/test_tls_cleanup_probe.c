/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_alloc.h"
#include "zcl_tls_alloc.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static void positive_control(void)
{
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    void *allocation = zcl_tls_calloc(1, 16);
    assert(allocation != NULL && heap.count == 1);
    zcl_tls_free(allocation);
    assert(zcl_tls_heap_clear(&heap));
    zcl_tls_heap_leave();
}

static void residual_child(void)
{
    const struct rlimit no_core = {0, 0};
    assert(setrlimit(RLIMIT_CORE, &no_core) == 0);
    zcl_tls_heap heap = {0};
    assert(zcl_tls_heap_enter(&heap));
    void *allocation = zcl_tls_calloc(1, 16);
    assert(allocation != NULL && heap.count == 1);
    /* A real owned allocation must be observed before emergency reclamation. */
    (void)zcl_tls_heap_clear(&heap);
    _Exit(0); /* Reached only if the observer is missing or fails to refuse. */
}

int main(void)
{
    positive_control();
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) residual_child();
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    assert(waited == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    positive_control();
    return 0;
}
