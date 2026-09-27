/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Private bounded DB scan and version fence for build attachment. */
#ifndef ZCL_BUILD_FABRIC_ATTACH_LEDGER_INTERNAL_H
#define ZCL_BUILD_FABRIC_ATTACH_LEDGER_INTERNAL_H

#include "models/build_fabric.h"

enum { BFAT_SCAN_CAP = 256 };

bool bfat_ledger_version(struct node_db *ndb, sqlite3_int64 *out);
bool bfat_ledger_settle_job(struct node_db *ndb,
                            const struct db_build_job *job, int64_t now);

#endif
