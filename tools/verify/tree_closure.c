/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * Fixed-TU verifier prerequisite: canonical whole-tree byte identity.
 * This program does not attest, sign, or authorize reuse. In particular,
 * same-UID ownership checks cannot replace a root-owned read-only jail. */
#define _GNU_SOURCE
#include "base/hex.h"
#include "base/safe_alloc.h"
#include "base/serialize_le.h"
#include "sha3/sha3.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_ENTRIES 200000u
#define MAX_DIR_ENTRIES 16384u
#define MAX_DEPTH 64u
#define MAX_BYTES (4ULL * 1024ULL * 1024ULL * 1024ULL)

struct walker {
    struct sha3_256_ctx hash;
    const char *root;
    uid_t owner;
    uint64_t bytes;
    unsigned entries;
    const char *error;
};

static void put_u64(struct sha3_256_ctx *h, uint64_t n)
{
    unsigned char b[8];
    zcl_write_u64_le(b, n);
    sha3_256_write(h, b, sizeof(b));
}

static void put_bytes(struct sha3_256_ctx *h, const void *p, size_t n)
{
    put_u64(h, n);
    sha3_256_write(h, p, n);
}

static int name_order(const void *a, const void *b)
{
    const char *const *x = a, *const *y = b;
    return strcmp(*x, *y);
}

static int same_stat(const struct stat *a, const struct stat *b)
{
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           a->st_mode == b->st_mode && a->st_uid == b->st_uid &&
           a->st_size == b->st_size &&
           a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
}

static int safe_owner(struct walker *w, const struct stat *st, int symlink)
{
    if (st->st_uid != w->owner) { w->error = "owner_mismatch"; return 0; }
    if (!symlink && (st->st_mode & 0022) != 0) {
        w->error = "writable_entry";
        return 0;
    }
    return 1;
}

static int tree_path(char out[PATH_MAX], const char *root, const char *rel)
{
    int n = snprintf(out, PATH_MAX, "%s%s%s", root, *rel ? "/" : "", rel);
    return n >= 0 && n < PATH_MAX;
}

/* realpath resolves every link in a chain. A dangling link is refused: it
 * cannot silently become a readable input without changing this tree. */
static int safe_link(struct walker *w, const char *rel)
{
    char path[PATH_MAX], resolved[PATH_MAX];
    if (!tree_path(path, w->root, rel) || !realpath(path, resolved)) {
        w->error = "symlink_unresolved";
        return 0;
    }
    size_t n = strlen(w->root);
    if (strncmp(resolved, w->root, n) != 0 ||
        (resolved[n] != '/' && resolved[n] != '\0')) {
        w->error = "symlink_escapes_root";
        return 0;
    }
    return 1;
}

static int visit(struct walker *w, int parent, const char *name,
                 const char *rel, unsigned depth);

struct dir_names {
    char **v;
    size_t count, cap;
};

static void free_names(struct dir_names *names)
{
    for (size_t i = 0; i < names->count; i++) free(names->v[i]);
    free(names->v);
}

static int add_name(struct walker *w, struct dir_names *names, const char *name)
{
    if (names->count == MAX_DIR_ENTRIES) {
        w->error = "directory_limit";
        return 0;
    }
    if (names->cap == names->count) {
        size_t next = names->cap ? names->cap * 2 : 32;
        char **grown = zcl_realloc(names->v, next * sizeof(*names->v),
                                   "tree_closure_names");
        if (!grown) { w->error = "allocation_failed"; return 0; }
        names->v = grown;
        names->cap = next;
    }
    names->v[names->count] = zcl_strdup(name, "tree_closure_name");
    if (!names->v[names->count]) {
        w->error = "allocation_failed";
        return 0;
    }
    names->count++;
    return 1;
}

static int read_names(struct walker *w, int fd, struct dir_names *names)
{
    int dupfd = dup(fd);
    if (dupfd < 0) { w->error = "directory_open_failed"; return 0; }
    DIR *dir = fdopendir(dupfd);
    if (!dir) { close(dupfd); w->error = "directory_open_failed"; return 0; }
    int ok = 1;
    errno = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0) continue;
        if (!add_name(w, names, ent->d_name)) { ok = 0; break; }
        errno = 0;
    }
    if (errno != 0 && ok) { w->error = "directory_read_failed"; ok = 0; }
    if (closedir(dir) != 0 && ok) { w->error = "directory_close_failed"; ok = 0; }
    if (ok) qsort(names->v, names->count, sizeof(*names->v), name_order);
    return ok;
}

static int walk_names(struct walker *w, int fd, const char *rel,
                      unsigned depth, const struct dir_names *names)
{
    for (size_t i = 0; i < names->count; i++) {
        char child[PATH_MAX];
        int n = snprintf(child, sizeof(child), "%s%s%s", rel,
                         *rel ? "/" : "", names->v[i]);
        if (n < 0 || n >= (int)sizeof(child)) {
            w->error = "path_limit";
            return 0;
        }
        if (!visit(w, fd, names->v[i], child, depth + 1)) return 0;
    }
    return 1;
}

static int visit_dir(struct walker *w, int fd, const char *rel,
                     const struct stat *before, unsigned depth)
{
    struct dir_names names = {0};
    int ok = read_names(w, fd, &names);
    if (ok) ok = walk_names(w, fd, rel, depth, &names);
    free_names(&names);
    struct stat after;
    if (ok && (fstat(fd, &after) != 0 || !same_stat(before, &after))) {
        w->error = "directory_changed";
        ok = 0;
    }
    return ok;
}

static int visit_link(struct walker *w, int parent, const char *name,
                      const char *rel, const struct stat *st)
{
    char target[PATH_MAX];
    ssize_t n = readlinkat(parent, name, target, sizeof(target));
    if (n < 0 || n == (ssize_t)sizeof(target)) {
        w->error = "symlink_read_failed";
        return 0;
    }
    target[n] = '\0';
    if (target[0] == '/' || !safe_link(w, rel)) {
        if (!w->error) w->error = "symlink_absolute";
        return 0;
    }
    struct stat after;
    if (fstatat(parent, name, &after, AT_SYMLINK_NOFOLLOW) != 0 ||
        !same_stat(st, &after)) {
        w->error = "entry_changed";
        return 0;
    }
    put_bytes(&w->hash, target, (size_t)n);
    return 1;
}

static int visit_file(struct walker *w, int fd, const struct stat *st)
{
    if (st->st_size < 0 || (uint64_t)st->st_size > MAX_BYTES - w->bytes) {
        w->error = "byte_limit";
        return 0;
    }
    put_u64(&w->hash, (uint64_t)st->st_size);
    unsigned char buf[16384];
    uint64_t got = 0;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { w->error = "entry_read_failed"; return 0; }
        if (n == 0) break;
        sha3_256_write(&w->hash, buf, (size_t)n);
        got += (uint64_t)n;
        if (got > (uint64_t)st->st_size) {
            w->error = "entry_changed";
            return 0;
        }
    }
    if (got != (uint64_t)st->st_size) {
        w->error = "entry_changed";
        return 0;
    }
    w->bytes += got;
    return 1;
}

static int visit_opened(struct walker *w, int parent, const char *name,
                        const char *rel, unsigned depth,
                        const struct stat *st, int is_dir)
{
    int flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW;
    if (is_dir) flags |= O_DIRECTORY;
    int fd = openat(parent, name, flags);
    if (fd < 0) { w->error = "entry_open_failed"; return 0; }
    struct stat opened;
    int ok = fstat(fd, &opened) == 0 && same_stat(st, &opened);
    if (!ok) w->error = "entry_changed";
    if (ok && is_dir) ok = visit_dir(w, fd, rel, &opened, depth);
    if (ok && !is_dir) ok = visit_file(w, fd, st);
    struct stat after;
    if (ok && (fstat(fd, &after) != 0 || !same_stat(&opened, &after))) {
        w->error = "entry_changed";
        ok = 0;
    }
    if (close(fd) != 0 && ok) { w->error = "entry_close_failed"; ok = 0; }
    return ok;
}

static int visit(struct walker *w, int parent, const char *name,
                 const char *rel, unsigned depth)
{
    if (depth > MAX_DEPTH || w->entries == MAX_ENTRIES) {
        w->error = "tree_limit";
        return 0;
    }
    struct stat st;
    if (fstatat(parent, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        w->error = "entry_stat_failed";
        return 0;
    }
    int is_dir = S_ISDIR(st.st_mode), is_file = S_ISREG(st.st_mode);
    int is_link = S_ISLNK(st.st_mode);
    if (!is_dir && !is_file && !is_link) {
        w->error = "special_entry";
        return 0;
    }
    if (!safe_owner(w, &st, is_link)) return 0;
    w->entries++;
    unsigned char type = (unsigned char)(is_dir ? 'D' : is_file ? 'F' : 'L');
    sha3_256_write(&w->hash, &type, 1);
    put_bytes(&w->hash, rel, strlen(rel));
    put_u64(&w->hash, (uint64_t)(st.st_mode & 07777));
    put_u64(&w->hash, (uint64_t)st.st_uid);
    if (is_link) return visit_link(w, parent, name, rel, &st);
    return visit_opened(w, parent, name, rel, depth, &st, is_dir);
}

static int safe_parent_chain(const char *root, uid_t owner)
{
    char path[PATH_MAX];
    size_t n = strlen(root);
    if (n < 2 || n >= sizeof(path) || root[0] != '/') return 0;
    memcpy(path, root, n + 1);
    for (;;) {
        struct stat st;
        if (lstat(path, &st) != 0 || !S_ISDIR(st.st_mode) ||
            (st.st_uid != 0 && st.st_uid != owner) ||
            (st.st_mode & 0022) != 0) return 0;
        char *slash = strrchr(path, '/');
        if (slash == path) { path[1] = '\0'; break; }
        if (!slash) return 0;
        *slash = '\0';
    }
    struct stat st;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode) &&
           st.st_uid == 0 && (st.st_mode & 0022) == 0;
}

static int parse_root(const char *path, const char *owner_arg, uid_t *owner)
{
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(owner_arg, &end, 10);
    char resolved[PATH_MAX];
    if (errno || !end || end == owner_arg || *end || parsed > UINT32_MAX ||
        !realpath(path, resolved) || strcmp(path, resolved) != 0 ||
        !safe_parent_chain(path, (uid_t)parsed)) return 0;
    *owner = (uid_t)parsed;
    return 1;
}

static int hash_root(struct walker *w, int rootfd)
{
    struct stat before, after;
    int ok = fstat(rootfd, &before) == 0 &&
             safe_owner(w, &before, 0);
    if (ok) {
        unsigned char type = 'D';
        sha3_256_write(&w->hash, &type, 1);
        put_bytes(&w->hash, "", 0);
        put_u64(&w->hash, (uint64_t)(before.st_mode & 07777));
        put_u64(&w->hash, (uint64_t)before.st_uid);
        w->entries++;
    }
    ok = ok && visit_dir(w, rootfd, "", &before, 0) &&
             fstat(rootfd, &after) == 0 && same_stat(&before, &after);
    if (!ok && !w->error) w->error = "root_changed";
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 4 || strcmp(argv[1], "hash") != 0) {
        fprintf(stderr, "usage: tree-closure hash /absolute/tree expected_uid\n");
        return 2;
    }
    uid_t owner;
    if (!parse_root(argv[2], argv[3], &owner)) {
        fprintf(stderr, "tree_closure_refuse=unsafe_root\n");
        return 2;
    }
    struct walker w = { .root = argv[2], .owner = owner };
    sha3_256_init(&w.hash);
    static const char domain[] = "z23.verify.tree.v1";
    put_bytes(&w.hash, domain, sizeof(domain) - 1);
    int rootfd = open(argv[2], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (rootfd < 0) {
        fprintf(stderr, "tree_closure_refuse=root_open_failed\n");
        return 2;
    }
    int ok = hash_root(&w, rootfd);
    if (close(rootfd) != 0 && ok) { w.error = "root_close_failed"; ok = 0; }
    if (!ok) {
        fprintf(stderr, "tree_closure_refuse=%s\n", w.error);
        return 2;
    }
    unsigned char digest[32];
    char hex[65];
    sha3_256_finalize(&w.hash, digest);
    zcl_hex_encode(digest, sizeof(digest), hex);
    printf("tree_sha3=%s entries=%u bytes=%llu attest_eligible=0\n",
           hex, w.entries, (unsigned long long)w.bytes);
    return 0;
}
