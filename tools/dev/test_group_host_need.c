/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Evaluate declared test-group host needs against an exact tree. */

#include "test_group_host_need.h"
#include "test_group_catalog.h"

#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const struct zcl_test_group_host_need g_host_needs[] = {
#define ZCL_TEST_GROUP_NEED(full_id_, kind_, value_) \
    {kind_, full_id_, value_, NULL},
#define ZCL_TEST_GROUP_BUILD_NEED(full_id_, value_, target_) \
    {ZCL_HOST_NEED_BUILD, full_id_, value_, target_},
#include "test_group_host_needs.def"
#undef ZCL_TEST_GROUP_BUILD_NEED
#undef ZCL_TEST_GROUP_NEED
};

#define ZCL_HOST_NEED_COUNT \
    (sizeof(g_host_needs) / sizeof(g_host_needs[0]))

const char *
zcl_test_group_host_need_kind_name(enum zcl_test_group_host_need_kind kind)
{
    switch (kind) {
    case ZCL_HOST_NEED_NONE:
        return "none";
    case ZCL_HOST_NEED_FILE:
        return "file";
    case ZCL_HOST_NEED_ENV:
        return "env";
    case ZCL_HOST_NEED_C23_TOOLCHAIN:
        return "toolchain";
    case ZCL_HOST_NEED_BUILD:
        return "build";
    default:
        return NULL;
    }
}

/* A BUILD row names its Make target; no other kind carries one. */
static bool host_need_target_valid(const struct zcl_test_group_host_need *row)
{
    bool has_target = row->target && row->target[0];
    return (row->kind == ZCL_HOST_NEED_BUILD) == has_target &&
           (has_target || !row->target);
}

/* One row's own shape: a known non-NONE kind, a non-empty value naming a
 * registered group, and a target exactly when the kind is BUILD. A row that
 * fails this is a table error, not a host fact. */
static bool host_need_row_valid(size_t i)
{
    const struct zcl_test_group_host_need *row = &g_host_needs[i];
    if (!row->group || !row->group[0] || !row->value || !row->value[0] ||
        row->kind == ZCL_HOST_NEED_NONE ||
        !zcl_test_group_host_need_kind_name(row->kind) ||
        !host_need_target_valid(row)) {
        fprintf(stderr,
                "test_group_host_need: row %zu declares an unknown kind or an "
                "empty field\n", i);
        return false;
    }
    if (!zcl_test_group_catalog_contains(row->group)) {
        fprintf(stderr,
                "test_group_host_need: row %zu names unregistered group '%s'\n",
                i, row->group);
        return false;
    }
    return true;
}

/* A group may have one host gate plus distinct BUILD targets. The gate is
 * resolved before a BUILD row so universal selection cannot silently treat
 * a missing host capability as a buildable input. */
static bool host_need_pair_valid(size_t i, size_t j)
{
    const struct zcl_test_group_host_need *a = &g_host_needs[i];
    const struct zcl_test_group_host_need *b = &g_host_needs[j];
    if (strcmp(a->group, b->group) != 0)
        return true;
    if (a->kind == ZCL_HOST_NEED_BUILD && b->kind == ZCL_HOST_NEED_BUILD &&
        strcmp(a->target, b->target) != 0)
        return true;
    if ((a->kind == ZCL_HOST_NEED_BUILD) !=
        (b->kind == ZCL_HOST_NEED_BUILD))
        return true;
    fprintf(stderr,
            "test_group_host_need: group '%s' declares two needs that are "
            "not distinct BUILD targets\n", a->group);
    return false;
}

bool zcl_test_group_host_needs_valid(void)
{
    for (size_t i = 0; i < ZCL_HOST_NEED_COUNT; i++) {
        if (!host_need_row_valid(i))
            return false;
        for (size_t j = 0; j < i; j++)
            if (!host_need_pair_valid(i, j))
                return false;
    }
    return true;
}

/* Is `target` already one of the first `n` collected needs? */
static bool host_need_target_listed(const struct zcl_test_group_host_need *needs,
                                    size_t n, const char *target)
{
    for (size_t k = 0; k < n; k++)
        if (strcmp(needs[k].target, target) == 0)
            return true;
    return false;
}

bool zcl_test_group_build_needs_add(const char *group,
                                    struct zcl_test_group_host_need *needs,
                                    size_t cap, size_t *n)
{
    struct zcl_test_group_host_need gate;
    if (!needs || !n || *n > cap || !zcl_test_group_host_need(group, &gate))
        return false;
    for (size_t i = 0; i < ZCL_HOST_NEED_COUNT; i++) {
        const struct zcl_test_group_host_need *row = &g_host_needs[i];
        if (row->kind != ZCL_HOST_NEED_BUILD ||
            strcmp(row->group, group) != 0 ||
            host_need_target_listed(needs, *n, row->target))
            continue;
        if (*n >= cap) {
            fprintf(stderr,
                    "test_group_host_need: no room for '%s' build need '%s'\n",
                    group, row->target);
            return false;
        }
        needs[(*n)++] = *row;
    }
    return true;
}

bool zcl_test_group_host_need(const char *group,
                              struct zcl_test_group_host_need *out)
{
    if (!out)
        return false;
    out->kind = ZCL_HOST_NEED_NONE;
    out->group = NULL;
    out->value = NULL;
    out->target = NULL;
    if (!group || !group[0] || !zcl_test_group_catalog_contains(group)) {
        fprintf(stderr,
                "test_group_host_need: '%s' is not a registered test group\n",
                group ? group : "(null)");
        return false;
    }
    if (!zcl_test_group_host_needs_valid())
        return false;
    for (size_t i = 0; i < ZCL_HOST_NEED_COUNT; i++) {
        if (strcmp(g_host_needs[i].group, group) != 0)
            continue;
        if (g_host_needs[i].kind != ZCL_HOST_NEED_BUILD) {
            *out = g_host_needs[i];
            return true;
        }
        if (out->kind == ZCL_HOST_NEED_NONE)
            *out = g_host_needs[i];
    }
    return true;
}

#if defined(__linux__) && defined(__x86_64__)
/* A compiler probe is a host fact, not a passed test. Use only the root-owned
 * system compiler route whose libclang runtime the proof already binds.
 * stderr is discarded; the named host-gated row carries the refusal. */
static bool root_owned_regular_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && st.st_uid == 0 &&
           S_ISREG(st.st_mode) && (st.st_mode & 022) == 0;
}

static bool c23_compiler_accepts(const char *path)
{
#if defined(__linux__) && defined(__x86_64__)
    static const char source[] =
        "constexpr int x = 1; int main(void) { return x; }\n";
    if (!root_owned_regular_file(path))
        return false;
    int fd[2];
    if (pipe(fd) != 0)
        return false;
    ssize_t wrote = write(fd[1], source, sizeof(source) - 1);
    if (wrote != (ssize_t)(sizeof(source) - 1)) {
        close(fd[0]); close(fd[1]);
        return false;
    }
    pid_t child = fork();
    if (child < 0) {
        close(fd[0]); close(fd[1]);
        return false;
    }
    if (child == 0) {
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd < 0 || dup2(fd[0], STDIN_FILENO) < 0 ||
            dup2(null_fd, STDOUT_FILENO) < 0 ||
            dup2(null_fd, STDERR_FILENO) < 0)
            _exit(127);
        close(fd[0]); close(fd[1]); close(null_fd);
        char *const args[] = {(char *)path, "-std=c23", "-x", "c",
                              "-fsyntax-only", "-", NULL};
        execv(path, args);
        _exit(127);
    }
    close(fd[0]); close(fd[1]);
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    (void)path;
    return false;
#endif
}

#endif /* Linux x86_64 compiler probes */

static bool c23_fuzz_toolchain_ready(void)
{
    /* Match the Make route that leaves the sensor with no RUNPATH and the
     * exact system libclang 18 soname. A newer installed header prefix wins
     * Make's selection and needs its own loaded-library proof binding. */
    static int cached = -1;
    if (cached < 0) {
#if defined(__linux__) && defined(__x86_64__)
        static const char *const competing_headers[] = {
            "/usr/lib/llvm-20/include/clang-c/Index.h",
            "/usr/lib/llvm-21/include/clang-c/Index.h",
            "/usr/lib/llvm-19/include/clang-c/Index.h",
            "/usr/lib/llvm-18/include/clang-c/Index.h",
        };
        bool fallback = true;
        for (size_t i = 0; i < sizeof(competing_headers) /
                                   sizeof(competing_headers[0]); i++)
            if (access(competing_headers[i], F_OK) == 0)
                fallback = false;
        struct stat lib;
        fallback = fallback &&
            stat("/lib/x86_64-linux-gnu/libclang-18.so.18", &lib) == 0 &&
            S_ISREG(lib.st_mode) && lib.st_uid == 0 &&
            (lib.st_mode & 022) == 0;
        cached = fallback &&
                 c23_compiler_accepts("/usr/lib/llvm-18/bin/clang") &&
                 c23_compiler_accepts("/usr/bin/gcc");
#else
        cached = 0;
#endif
    }
    return cached == 1;
}

/* Does `<root>/<value>` exist? Only existence is asked: a proof generation
 * either carries the artifact or it does not, and a deeper probe would make
 * selection depend on the artifact's content. */
static bool host_need_file_present(const char *root, const char *value)
{
    char path[PATH_MAX];
    struct stat st;
    int n = snprintf(path, sizeof(path), "%s/%s", root, value);
    if (n <= 0 || (size_t)n >= sizeof(path)) {
        fprintf(stderr,
                "test_group_host_need: file need path too long under '%s'\n",
                root);
        return false;
    }
    return stat(path, &st) == 0;
}

bool zcl_test_group_host_need_met(const char *root,
                                  const struct zcl_test_group_host_need *need)
{
    if (!need)
        return false;
    if (need->kind != ZCL_HOST_NEED_NONE && (!need->value || !need->value[0]))
        return false;
    switch (need->kind) {
    case ZCL_HOST_NEED_NONE:
        return true;
    case ZCL_HOST_NEED_FILE:
    case ZCL_HOST_NEED_BUILD: {
        if (!root || !root[0])
            return false;
        return host_need_file_present(root, need->value);
    }
    case ZCL_HOST_NEED_ENV: {
        const char *set = getenv(need->value);
        return set != NULL && set[0] != '\0';
    }
    case ZCL_HOST_NEED_C23_TOOLCHAIN:
        return strcmp(need->value, "bound-libclang18-full-c23") == 0 &&
               c23_fuzz_toolchain_ready();
    default:
        fprintf(stderr,
                "test_group_host_need: unknown need kind %d for '%s'\n",
                (int)need->kind, need->group ? need->group : "(null)");
        return false;
    }
}

bool zcl_test_group_host_need_selectable(
    const char *root, const struct zcl_test_group_host_need *need)
{
    if (!need)
        return false;
    if (need->kind == ZCL_HOST_NEED_NONE || need->kind == ZCL_HOST_NEED_BUILD)
        return true;
    return zcl_test_group_host_need_met(root, need);
}
