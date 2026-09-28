/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * Root-owned fixed-result launcher preflight. No observation is issued by
 * this binary until the separate worker/ACK and publisher protocol qualifies. */
#define _GNU_SOURCE
#include "base/hex.h"
#include "platform/os_proc.h"
#include "sha3/sha3.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define POLICY "/etc/z23verify/fixed_result.policy"
#define PINS "/etc/z23verify/fixed_result.pins"
#define ARGS "/etc/z23verify/fixed_result_strict.args"
#define FILTER "/etc/z23verify/fixed_result.seccomp.bpf"
#define WORKER "/usr/local/libexec/z23-fixed-result-worker"
#define BWRAP "/usr/bin/bwrap"
#define TREE "/usr/local/libexec/z23-tree-closure"
#define SOURCE_IMAGE "/var/lib/z23verify/images/fixed_result/source"
#define TOOL_IMAGE "/var/lib/z23verify/images/fixed_result/tool"
#define CHECK_IMAGE "/var/lib/z23verify/images/fixed_result/check"

static const char *const pin_names[] = {
    "strict_args_sha3", "source_image_sha3", "tool_image_sha3",
    "worker_sha3", "launcher_sha3", "check_image_sha3", "env_sha3",
    "policy_sha3", "seccomp_filter_sha3", "bwrap_sha3", "tree_closure_sha3"
};

enum { PIN_COUNT = sizeof(pin_names) / sizeof(pin_names[0]) };

static const char *const policy_text =
    "z23verify.fixed_result.policy.v1\n"
    "compiler_uid=60093\n"
    "compiler_gid=60093\n"
    "recorded_cwd=/zclassic23\n"
    "source_image=" SOURCE_IMAGE "\n"
    "tool_image=" TOOL_IMAGE "\n"
    "check_image=" CHECK_IMAGE "\n"
    "worker=" WORKER "\n"
    "bwrap=" BWRAP "\n"
    "tree_closure=" TREE "\n"
    "seccomp_filter=" FILTER "\n"
    "launches=/var/lib/z23verify/launches\n"
    "socket=/run/z23verify/fixed_result.launch.sock\n"
    "request_timeout_ms=30000\n"
    "worker_timeout_ms=30000\n"
    "mounts=tool_ro,source_ro,work_private_tmpfs,tmp_private_tmpfs,dev_null\n"
    "namespaces=user,mount,net,pid,ipc,uts,cgroup\n"
    "network=none\n"
    "fd_inheritance=stdio_only\n";

static int refuse(const char *reason)
{
    fprintf(stderr, "fixed_result_launcher_refuse=%s\n", reason);
    return 2;
}

static bool lower_hex(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (!((s[i] >= '0' && s[i] <= '9') ||
              (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}

/* Walk every ancestor; a safe leaf under a writable/symlinked directory is
 * not a root pin. This also refuses a fixture under the shared checkout. */
static bool owned_component(const char *path, uid_t owner, bool directory)
{
    struct stat st;
    if (lstat(path, &st) != 0 || st.st_uid != owner ||
        (st.st_mode & 0022) != 0) return false;
    if (directory) return S_ISDIR(st.st_mode);
    return S_ISREG(st.st_mode) && st.st_nlink == 1;
}

static bool trusted_path(const char *path, uid_t owner, bool directory)
{
    char part[PATH_MAX];
    size_t len = strlen(path);
    if (len < 2 || len >= sizeof(part) || path[0] != '/') return false;
    memcpy(part, path, len + 1);
    if (!owned_component("/", owner, true)) return false;
    for (size_t i = 1; i <= len; i++) {
        if (part[i] != '/' && part[i] != '\0') continue;
        char saved = part[i];
        part[i] = '\0';
        if (!owned_component(part, owner, saved != '\0' || directory))
            return false;
        part[i] = saved;
    }
    return true;
}

static bool safe_root_file(const struct stat *st, off_t limit)
{
    return S_ISREG(st->st_mode) && st->st_uid == 0 && st->st_nlink == 1 &&
           (st->st_mode & 0022) == 0 &&
           st->st_size >= 0 && st->st_size <= limit;
}

static bool stable_file(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_size == b->st_size &&
           a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static bool hash_stream(int fd, off_t limit, struct sha3_256_ctx *h)
{
    uint8_t bytes[16384];
    off_t count = 0;
    for (;;) {
        ssize_t n = read(fd, bytes, sizeof(bytes));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return false;
        if (n == 0) return count == limit;
        count += n;
        if (count > limit) return false;
        sha3_256_write(h, bytes, (size_t)n);
    }
}

static bool hash_fd(int fd, char out[65])
{
    struct stat before, after;
    if (fstat(fd, &before) != 0 ||
        !safe_root_file(&before, 128 * 1024 * 1024)) return false;
    struct sha3_256_ctx h;
    sha3_256_init(&h);
    bool ok = hash_stream(fd, before.st_size, &h);
    bool after_ok = fstat(fd, &after) == 0;
    if (!ok || !after_ok || !stable_file(&before, &after)) return false;
    uint8_t digest[32];
    sha3_256_finalize(&h, digest);
    zcl_hex_encode(digest, sizeof(digest), out);
    return true;
}

static bool hash_file(const char *path, char out[65])
{
    if (!trusted_path(path, 0, false)) return false;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    bool ok = hash_fd(fd, out);
    return close(fd) == 0 && ok;
}

static bool read_fixed_bytes(int fd, char *out, size_t length)
{
    size_t at = 0;
    while (at < length) {
        ssize_t n = read(fd, out + at, length - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        at += (size_t)n;
    }
    return true;
}

static bool read_exact_root_file(const char *path, char *out, size_t cap,
                                 size_t *length)
{
    if (!trusted_path(path, 0, false)) return false;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    struct stat before, after;
    bool ok = fstat(fd, &before) == 0 &&
              safe_root_file(&before, (off_t)cap - 1) && before.st_size > 0;
    if (ok) ok = read_fixed_bytes(fd, out, (size_t)before.st_size);
    bool after_ok = fstat(fd, &after) == 0;
    int close_result = close(fd);
    if (!ok || !after_ok || close_result != 0 ||
        !stable_file(&before, &after) || !safe_root_file(&after, (off_t)cap - 1))
        return false;
    if (memchr(out, '\0', (size_t)before.st_size)) return false;
    out[before.st_size] = '\0';
    *length = (size_t)before.st_size;
    return true;
}

static bool parse_pins(const char *bytes, size_t length,
                       char pin[PIN_COUNT][65])
{
    static const char schema[] = "z23verify.fixed_result.pins.v1\n";
    if (memchr(bytes, '\0', length)) return false;
    size_t remaining = length;
    if (remaining < sizeof(schema) - 1 ||
        strncmp(bytes, schema, sizeof(schema) - 1) != 0) return false;
    const char *p = bytes + sizeof(schema) - 1;
    remaining -= sizeof(schema) - 1;
    for (size_t i = 0; i < PIN_COUNT; i++) {
        size_t key = strlen(pin_names[i]);
        if (remaining < key + 1 + 64 + 1) return false;
        if (strncmp(p, pin_names[i], key) != 0 || p[key] != '=') return false;
        p += key + 1;
        if (!lower_hex(p, 64) || p[64] != '\n') return false;
        memcpy(pin[i], p, 64);
        pin[i][64] = '\0';
        if (strspn(pin[i], "0") == 64) return false;
        p += 65;
        remaining -= key + 1 + 65;
    }
    return remaining == 0;
}

static bool pin_file(const char *path, const char expected[65])
{
    char actual[65];
    return hash_file(path, actual) && strcmp(actual, expected) == 0;
}

static bool pin_self(const char expected[65])
{
    if (os_proc_self_exe_identity() != OS_PROC_IMAGE_IDENTITY_RUNNING_IMAGE)
        return false;
    FILE *image = os_proc_open_self_exe();
    if (!image) return false;
    char actual[65];
    bool ok = hash_fd(fileno(image), actual);
    if (fclose(image) != 0) return false;
    return ok && strcmp(actual, expected) == 0;
}

static bool pinned_environment(const char expected[65])
{
    static const char env[] =
        "z23verify.fixed_result.env.v1\n"
        "LC_ALL=C\nTZ=UTC\nTMPDIR=/tmp\nPATH=/usr/bin:/bin\n";
    struct sha3_256_ctx h;
    uint8_t digest[32];
    char actual[65];
    sha3_256_init(&h);
    sha3_256_write(&h, (const uint8_t *)env, sizeof(env) - 1);
    sha3_256_finalize(&h, digest);
    zcl_hex_encode(digest, sizeof(digest), actual);
    return strcmp(actual, expected) == 0;
}

static void exec_tree_child(int write_fd, const char *path)
{
    if (dup2(write_fd, STDOUT_FILENO) < 0) _exit(127);
    if (close_range(3, ~0u, 0) != 0) _exit(127);
    char *const args[] = {(char *)TREE, "hash", (char *)path, "0", NULL};
    char *const env[] = {"LC_ALL=C", "PATH=/usr/bin:/bin", NULL};
    execve(TREE, args, env);
    _exit(127);
}

static bool capture_tree(int fd, char output[512], size_t *used)
{
    *used = 0;
    for (;;) {
        struct pollfd ready = {.fd = fd, .events = POLLIN};
        if (poll(&ready, 1, 120000) <= 0) return false;
        ssize_t n = read(fd, output + *used, 511 - *used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return false;
        if (n == 0) return true;
        *used += (size_t)n;
        if (*used == 511) return false;
    }
}

static bool wait_tree(pid_t pid, bool captured)
{
    if (!captured) (void)kill(pid, SIGKILL);
    int status = 0;
    pid_t done;
    do { done = waitpid(pid, &status, 0); }
    while (done < 0 && errno == EINTR);
    return captured && done == pid && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}

static bool tree_root_matches(const char *path, const char expected[65])
{
    if (!trusted_path(path, 0, true)) return false;
    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC) != 0) return false;
    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return false; }
    if (pid == 0) exec_tree_child(pipefd[1], path);
    close(pipefd[1]);
    char output[512];
    size_t used = 0;
    bool ok = capture_tree(pipefd[0], output, &used);
    close(pipefd[0]);
    if (!wait_tree(pid, ok)) return false;
    output[used] = '\0';
    return used > 10 + 64 && strncmp(output, "tree_sha3=", 10) == 0 &&
           lower_hex(output + 10, 64) && output[74] == ' ' &&
           memcmp(output + 10, expected, 64) == 0;
}

static const char *check_policy_and_pins(char pins[PIN_COUNT][65])
{
    char pins_bytes[2048], policy_bytes[2048];
    size_t pins_length = 0, policy_length = 0;
    if (!read_exact_root_file(PINS, pins_bytes, sizeof(pins_bytes),
                              &pins_length) ||
        !parse_pins(pins_bytes, pins_length, pins)) return "pins_unsafe";
    if (!read_exact_root_file(POLICY, policy_bytes, sizeof(policy_bytes),
                              &policy_length) ||
        policy_length != strlen(policy_text) ||
        memcmp(policy_bytes, policy_text, policy_length) != 0 ||
        !pin_file(POLICY, pins[7])) return "policy_mismatch";
    return NULL;
}

static const char *check_files(char pins[PIN_COUNT][65])
{
    if (!pin_file(ARGS, pins[0])) return "strict_args_mismatch";
    if (!pin_file(WORKER, pins[3])) return "worker_mismatch";
    if (!pin_self(pins[4])) return "launcher_mismatch";
    if (!pin_file(FILTER, pins[8])) return "filter_mismatch";
    if (!pin_file(BWRAP, pins[9])) return "bwrap_mismatch";
    if (!pin_file(TREE, pins[10])) return "tree_checker_mismatch";
    if (!pinned_environment(pins[6])) return "environment_mismatch";
    return NULL;
}

static const char *check_images(char pins[PIN_COUNT][65])
{
    if (!tree_root_matches(SOURCE_IMAGE, pins[1]))
        return "source_image_mismatch";
    if (!tree_root_matches(TOOL_IMAGE, pins[2]))
        return "tool_image_mismatch";
    if (!tree_root_matches(CHECK_IMAGE, pins[5]))
        return "check_image_mismatch";
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "preflight") != 0)
        return refuse("request_shape_unsupported");
    if (getuid() != 0 || geteuid() != 0) return refuse("root_required");
    char pins[PIN_COUNT][65];
    const char *reason = check_policy_and_pins(pins);
    if (reason == NULL) reason = check_files(pins);
    if (reason == NULL) reason = check_images(pins);
    if (reason != NULL) return refuse(reason);
    puts("pinned_material_ok=1 attest_eligible=0");
    return refuse("isolation_unqualified");
}
