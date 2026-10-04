/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * utxo_reimport_flag - implementation. See header for the contract.
 *
 * Owns the durable `<datadir>/needs_reimport` sentinel shared by validation
 * self-heal and boot recovery. Validation decides when to set it; boot checks
 * and clears it exactly once before chain mutators start.
 */

#include "storage/utxo_reimport_flag.h"
#include "platform/private_file.h"

#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

static _Atomic uint64_t g_urf_staging_sequence;

static bool urf_replace(const char *flag_path)
{
    char resolved[512], parent[512], staging_path[576];
    if (!platform_private_destination_resolve(
            flag_path, resolved, sizeof(resolved), parent, sizeof(parent)))
        return false;

    struct platform_private_file staged;
    platform_private_file_init(&staged);
    bool created = false;
    for (unsigned attempt = 0; attempt < 1024 && !created; attempt++) {
        uint64_t seq = atomic_fetch_add_explicit(
            &g_urf_staging_sequence, 1, memory_order_relaxed);
        int n = snprintf(staging_path, sizeof(staging_path), "%s.tmp.%llu",
                         resolved, (unsigned long long)seq);
        if (n <= 0 || (size_t)n >= sizeof(staging_path))
            break;
        created = platform_private_file_create(staging_path, &staged);
    }
    bool ok = created &&
              platform_private_file_write_at(&staged, "1\n", 2, 0) &&
              platform_private_file_truncate(&staged, 2) &&
              platform_private_file_flush(&staged) &&
              platform_private_file_replace(&staged, staging_path, resolved) &&
              platform_private_parent_flush(parent);
    platform_private_file_close(&staged);
    if (created && !ok)
        (void)platform_private_file_unlink_missing_ok(staging_path);
    return ok;
}

bool utxo_reimport_flag_check_and_clear(const char *datadir)
{
    if (!datadir)
        return false;

    char flag_path[512];
    snprintf(flag_path, sizeof(flag_path),
             "%s/needs_reimport", datadir);

    FILE *flag = fopen(flag_path, "r");
    if (!flag)
        return false;

    char buf[8] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, flag);
    (void)n;  /* tolerate short / empty reads — content drives the bool */
    fclose(flag);
    /* Unconditional clear: even if the byte was not '1' we remove the
     * marker so a malformed write cannot loop forever. */
    remove(flag_path);

    if (buf[0] == '1') {
        fprintf(stderr,  // obs-ok:storage-primitive-info
                "[storage] utxo_reimport_flag: set — cleared "
                "and signalling reimport (datadir=%s)\n", datadir);
        return true;
    }
    return false;
}

bool utxo_reimport_flag_set(const char *datadir)
{
    if (!datadir)
        return false;

    char flag_path[512];
    int path_len = snprintf(flag_path, sizeof(flag_path),
                            "%s/needs_reimport", datadir);
    if (path_len < 0 || (size_t)path_len >= sizeof(flag_path))
        return false;

    /* Preserve the exact 2-byte "1\n" payload the previous inline
     * writer produced — keeps the on-disk format identical. */
    bool ok = urf_replace(flag_path);
    if (!ok) {
        fprintf(stderr,  // obs-ok:storage-primitive-error
                "[storage] utxo_reimport_flag_set: durable replace(%s) "
                "failed\n", flag_path);
    }
    return ok;
}
