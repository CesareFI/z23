/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Complete bounded sibling scan and DB version fence for attachment. */
#include "build_fabric_attach_ledger_internal.h"

#include "base/log_macros.h"
#include "base/safe_alloc.h"
#include "util/ar_step_readonly.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool bfat_ledger_version(struct node_db *ndb, sqlite3_int64 *out)
{
    sqlite3_stmt *st = NULL;
    if (!ndb || !out ||
        sqlite3_prepare_v2(ndb->db, "PRAGMA data_version", -1, &st, NULL) !=
            SQLITE_OK)
        LOG_RETURN(false, "build_fabric", "attach data_version prepare failed");
    int rc = AR_STEP_ROW_READONLY(st);
    bool ok = rc == SQLITE_ROW &&
              sqlite3_column_type(st, 0) == SQLITE_INTEGER;
    if (ok) *out = sqlite3_column_int64(st, 0);
    rc = ok ? AR_STEP_ROW_READONLY(st) : SQLITE_ERROR;
    ok = ok && rc == SQLITE_DONE;
    if (sqlite3_finalize(st) != SQLITE_OK) ok = false;
    if (!ok)
        LOG_ERROR("build_fabric", "attach data_version read incomplete");
    return ok;
}

/* A partial sibling prefix can never settle a job. The caller holds BEGIN
 * IMMEDIATE and has fenced every DB change since its complete donor scan. */
bool bfat_ledger_settle_job(struct node_db *ndb,
                            const struct db_build_job *job, int64_t now)
{
    struct db_build_action *siblings = zcl_malloc(
        (BFAT_SCAN_CAP + 1u) * sizeof(*siblings), "build.attach.siblings");
    if (!siblings)
        LOG_RETURN(false, "build_fabric", "attach sibling allocation failed");
    int count = db_build_job_actions_checked(ndb, job->job_id, siblings,
                                              BFAT_SCAN_CAP + 1u);
    if (count < 1 || count > BFAT_SCAN_CAP) {
        free(siblings);
        LOG_RETURN(false, "build_fabric", "attach sibling history incomplete");
    }
    bool all_done = true, all_cache_hit = true;
    for (int i = 0; i < count; i++) {
        bool accepted = strcmp(siblings[i].state, "ACCEPTED") == 0 ||
            strcmp(siblings[i].state, "CACHE_HIT") == 0;
        all_done = all_done && accepted;
        all_cache_hit = all_cache_hit &&
            strcmp(siblings[i].state, "CACHE_HIT") == 0;
    }
    bool ok = true;
    if (all_done) {
        struct db_build_job done_job = *job;
        const char *outcome = all_cache_hit ? "CACHE_HIT" : "ACCEPTED";
        (void)snprintf(done_job.state, sizeof(done_job.state), "%s", outcome);
        (void)snprintf(done_job.outcome, sizeof(done_job.outcome), "%s",
                       outcome);
        done_job.updated_at = now;
        ok = db_build_job_save(ndb, &done_job);
    }
    free(siblings);
    return ok;
}
