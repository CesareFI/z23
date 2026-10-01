/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Complete-view recovery scan for persisted swarm downloads. */

#include "package_swarm_resume.h"

#include "package_store_priv.h"
#include "vcs/package_swarm_node.h"

#include "base/hex.h"
#include "util/log_macros.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SWARM_RESUME_LOG "vcs.swarm.resume"

struct resume_names {
    char names[VCS_SWARM_RESUME_SCAN_MAX][65];
    size_t count;
};

static bool resume_name_is_dot(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static bool resume_collect_names(DIR *dir, struct resume_names *out)
{
    size_t entries_seen = 0;
    for (;;) {
        errno = 0;
        struct dirent *ent = readdir(dir);
        if (!ent)
            return errno == 0;
        if (resume_name_is_dot(ent->d_name))
            continue;
        if (entries_seen >= VCS_SWARM_RESUME_SCAN_MAX)
            return false;
        entries_seen++;
        if (!store_name_is_hex64(ent->d_name))
            continue;
        memcpy(out->names[out->count], ent->d_name, 65);
        out->count++;
    }
}

static int resume_name_cmp(const void *a, const void *b)
{
    return memcmp(a, b, 65);
}

static bool resume_record_matches_name(
    const struct vcs_swarm_record *record, const char *name)
{
    uint8_t named_root[32];
    return zcl_hex_decode_lower(name, named_root, sizeof(named_root)) &&
           memcmp(named_root, record->root, sizeof(named_root)) == 0;
}

static bool resume_apply_name(const char *dir, const char *name,
                              vcs_swarm_resume_apply_fn apply, void *ctx)
{
    char path[STORE_PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (n <= 0 || (size_t)n >= sizeof(path))
        return false;
    struct vcs_swarm_record record;
    if (!vcs_swarm_record_load(path, &record) ||
        !resume_record_matches_name(&record, name)) {
        LOG_WARN(SWARM_RESUME_LOG, "discarding corrupt or misnamed "
                 "download record %s", name);
        if (unlink(path) != 0)
            LOG_WARN(SWARM_RESUME_LOG,
                     "failed to remove bad download record %s", name);
        return true;
    }
    return apply(&record, name, path, ctx);
}

bool vcs_swarm_resume_scan(const char *zcode_dir,
                           vcs_swarm_resume_apply_fn apply, void *ctx)
{
    if (!zcode_dir || !apply)
        LOG_FAIL(SWARM_RESUME_LOG, "invalid recovery scan input");
    char dir[STORE_PATH_MAX];
    int n = snprintf(dir, sizeof(dir), "%s/downloads", zcode_dir);
    if (n <= 0 || (size_t)n >= sizeof(dir))
        LOG_FAIL(SWARM_RESUME_LOG, "download recovery path too long");
    DIR *stream = opendir(dir);
    if (!stream) {
        if (errno == ENOENT)
            return true;
        LOG_FAIL(SWARM_RESUME_LOG, "cannot open download recovery directory");
    }
    struct resume_names names = {0};
    bool complete = resume_collect_names(stream, &names);
    if (closedir(stream) != 0)
        complete = false;
    if (!complete)
        LOG_FAIL(SWARM_RESUME_LOG, "download recovery scan incomplete");
    if (names.count > 1)
        qsort(names.names, names.count, sizeof(names.names[0]),
              resume_name_cmp);
    for (size_t i = 0; i < names.count; i++)
        if (!resume_apply_name(dir, names.names[i], apply, ctx))
            return false;
    return true;
}
