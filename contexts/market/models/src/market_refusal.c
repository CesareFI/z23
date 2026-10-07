/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Purpose: persistent operator refusal roots for optional marketplace data. */
#include "models/market_refusal.h"
#include "models/query_builder.h"
#include "base/hex.h"
#include "util/ar_step_readonly.h"
#include "util/log_macros.h"

#include <stdio.h>

DEFINE_MODEL_CALLBACKS(market_refusal)

static void refusal_key(const uint8_t root[32], char key[96])
{
    char hex[65];
    zcl_hex_encode(root, 32, hex);
    snprintf(key, 96, "market.refused.%s", hex);
}

bool db_market_refusal_validate(const struct market_refusal *record,
                                struct ar_errors *errors)
{
    ar_errors_clear(errors);
    if (!record) {
        ar_errors_add(errors, "refusal", "is missing");
        return false;
    }
    validates_presence_of(errors, record, root);
    return !ar_errors_any(errors);
}

bool db_market_refusal_save(struct node_db *ndb,
                            const struct market_refusal *record)
{
    if (!ndb || !ndb->open)
        LOG_FAIL("market", "refusal save requires an open node database");
    struct ar_callbacks *cbs = db_market_refusal_callbacks();
    AR_BEGIN_SAVE(cbs, "market_refusal", record, db_market_refusal_validate);
    char key[96];
    refusal_key(record->root, key);
    bool ok = node_db_state_set(ndb, key, record->root, sizeof(record->root));
    AR_FINISH_SAVE(cbs, record, ok);
}

bool db_market_refusal_allows(struct node_db *ndb, const uint8_t root[32])
{
    if (!ndb || !ndb->open || !root)
        LOG_FAIL("market", "refusal query requires a database and root");
    char key[96];
    refusal_key(root, key);
    struct qb q;
    qb_select(&q, QB_T_node_state);
    qb_select_count_star(&q);
    qb_where_text(&q, QB_C_node_state_key, QB_EQ, key);
    sqlite3_stmt *stmt = NULL;
    if (!QB_PREPARE(ndb, &q, stmt))
        LOG_FAIL("market", "cannot prepare refusal lookup");
    bool row = AR_STEP_ROW(stmt);
    bool allowed = row && sqlite3_column_int64(stmt, 0) == 0;
    AR_FINALIZE(stmt);
    if (!row)
        LOG_ERROR("market", "refusal lookup failed; refusing listing");
    return allowed;
}
