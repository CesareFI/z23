/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Schema-preflight ownership observation must not become ownership repair.
 * All descriptors and pathnames below belong to disposable local fixtures. */

#include "test/test_core.h"
#include "models/database_lifetime.h"
#include "platform/time_compat.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
static bool ownership_same(const struct stat *a, const struct stat *b)
{
    return a->st_uid == b->st_uid && a->st_gid == b->st_gid &&
        a->st_mode == b->st_mode && a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
        a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

/* Borrow fd; always restore the caller's thread-local scope, including errno
 * capture on a refused operation. No assertion may jump past scope leave. */
static int preflight_chown(int fd, uid_t owner, gid_t group, int *error)
{
    struct db_lifetime_scope scope;
    db_lifetime_scope_enter(&scope, "test.schema_preflight",
                            DB_LIFETIME_SCHEMA_PREFLIGHT, 0);
    int rc = fchown(fd, owner, group);
    *error = errno;
    db_lifetime_scope_leave(&scope);
    return rc;
}

static int ownership_observation(int fd)
{
    int failures = 0;
    TEST("db ownership: preflight observes without changing metadata") {
        struct stat before, after;
        int error = 0;
        ASSERT(fstat(fd, &before) == 0);
        platform_sleep_ms(50);
        for (int i = 0; i < 8; i++)
            ASSERT(preflight_chown(fd, before.st_uid, before.st_gid, &error) == 0);
        ASSERT(preflight_chown(fd, (uid_t)-1, (gid_t)-1, &error) == 0);
        ASSERT(fstat(fd, &after) == 0);
        ASSERT(ownership_same(&before, &after));
        PASS();
    } _test_next:;
    return failures;
}

static int ownership_refusals(int fd)
{
    int failures = 0;
    TEST("db ownership: preflight refuses owner/group changes and invalid fd") {
        struct stat before, after;
        int error = 0;
        ASSERT(fstat(fd, &before) == 0);
        ASSERT(preflight_chown(fd, before.st_uid ^ 1u, before.st_gid, &error) == -1);
        ASSERT(error == EPERM);
        ASSERT(preflight_chown(fd, before.st_uid, before.st_gid ^ 1u, &error) == -1);
        ASSERT(error == EPERM);
        ASSERT(preflight_chown(-1, before.st_uid, before.st_gid, &error) == -1);
        ASSERT(error == EBADF);
        ASSERT(fstat(fd, &after) == 0);
        ASSERT(ownership_same(&before, &after));
        PASS();
    } _test_next:;
    return failures;
}

/* Positive control: real fchown of even the current owner changes ctime.
 * This works without root and makes a blanket no-op interposer fail. */
static bool mutable_chown_observed(int fd)
{
    struct stat before, after;
    if (fstat(fd, &before) != 0)
        return false;
    platform_sleep_ms(50);
    return fchown(fd, before.st_uid, before.st_gid) == 0 &&
        fstat(fd, &after) == 0 && !ownership_same(&before, &after);
}

static bool nested_scope_restored(int fd)
{
    struct db_lifetime_scope outer, inner;
    struct stat before, after;
    db_lifetime_scope_enter(&outer, "test.preflight.outer",
                            DB_LIFETIME_SCHEMA_PREFLIGHT, 7);
    db_lifetime_scope_enter(&inner, "test.mutable.inner",
                            DB_LIFETIME_HANDLE_OWNER, 11);
    bool mutable_ok = mutable_chown_observed(fd);
    db_lifetime_scope_leave(&inner);
    bool ok = mutable_ok && db_lifetime_scope_generation() == 7 &&
        fstat(fd, &before) == 0;
    platform_sleep_ms(50);
    ok = ok && fchown(fd, before.st_uid, before.st_gid) == 0 &&
        fstat(fd, &after) == 0 && ownership_same(&before, &after);
    db_lifetime_scope_leave(&outer);
    return ok;
}

struct ownership_thread {
    int borrowed_fd;
    bool observed;
};

static void *ownership_thread_run(void *arg)
{
    struct ownership_thread *thread = arg;
    thread->observed = mutable_chown_observed(thread->borrowed_fd);
    return NULL;
}

static bool scope_is_thread_local(int fd)
{
    struct db_lifetime_scope scope;
    db_lifetime_scope_enter(&scope, "test.preflight.parent",
                            DB_LIFETIME_SCHEMA_PREFLIGHT, 0);
    struct ownership_thread thread = { .borrowed_fd = fd };
    pthread_t worker;
    int rc = pthread_create(&worker, NULL, ownership_thread_run, &thread);
    bool ok = false;
    if (rc == 0) {
        if (pthread_join(worker, NULL) != 0) {
            fprintf(stderr, "db ownership: cannot join fixture worker\n");
            abort(); /* Do not release its borrowed stack or descriptor. */
        }
        ok = thread.observed;
    }
    db_lifetime_scope_leave(&scope);
    return ok;
}

static int ownership_scope_controls(int fd)
{
    int failures = 0;
    TEST("db ownership: mutable, nested and other-thread operations stay live") {
        ASSERT(mutable_chown_observed(fd));
        ASSERT(nested_scope_restored(fd));
        ASSERT(mutable_chown_observed(fd));
        ASSERT(scope_is_thread_local(fd));
        PASS();
    } _test_next:;
    return failures;
}

static int ownership_repair_control(int fd)
{
    int failures = 0;
    /* Only root may change both IDs arbitrarily. Non-root CI still exercises
     * real fchown above; the root run additionally proves actual repair. */
    if (geteuid() != 0) {
        printf("db ownership: changed-owner control requires root (not run)\n");
        return failures;
    }
    TEST("db ownership: ordinary operation retains actual ownership repair") {
        struct stat before, changed, restored;
        ASSERT(fstat(fd, &before) == 0);
        int change_rc = fchown(fd, before.st_uid ^ 1u, before.st_gid ^ 1u);
        int change_error = errno;
        int stat_rc = fstat(fd, &changed);
        /* Restore before any assertion, even if the observed change fails. */
        int restore_rc = fchown(fd, before.st_uid, before.st_gid);
        if (change_rc != 0 || restore_rc != 0)
            fprintf(stderr, "db ownership: mutable control change=%d errno=%d "
                    "restore=%d errno=%d (root needs ownership capability)\n",
                    change_rc, change_error, restore_rc, errno);
        ASSERT(change_rc == 0 && stat_rc == 0 && restore_rc == 0);
        ASSERT(changed.st_uid == (before.st_uid ^ 1u));
        ASSERT(changed.st_gid == (before.st_gid ^ 1u));
        ASSERT(fstat(fd, &restored) == 0);
        ASSERT(restored.st_uid == before.st_uid && restored.st_gid == before.st_gid);
        PASS();
    } _test_next:;
    return failures;
}
#endif

int test_db_preflight_ownership(void);

int test_db_preflight_ownership(void)
{
#if defined(__linux__)
    /* This function owns the temporary file. Helpers borrow fd until joined. */
    char path[] = "./test-tmp/db-preflight-owner-XXXXXX";
    if (mkdir("./test-tmp", 0700) != 0 && errno != EEXIST)
        return 1;
    int fd = mkstemp(path);
    if (fd < 0)
        return 1;
    int failures = ownership_observation(fd);
    failures += ownership_refusals(fd);
    failures += ownership_scope_controls(fd);
    failures += ownership_repair_control(fd);
    if (close(fd) != 0)
        failures++;
    if (unlink(path) != 0)
        failures++;
    return failures;
#else
    printf("db ownership: Linux syscall boundary is not applicable on this platform\n");
    return 0;
#endif
}
