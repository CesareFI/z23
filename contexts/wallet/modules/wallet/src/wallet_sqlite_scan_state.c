/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Wallet scan state in node_state.
 *
 * 'wallet_scan_height' holds best_block_height, the height every stored
 * confirmation depth is measured from. 'wallet_scan_retry_from' exists only
 * while a boot catch-up's range is unread: it names the lowest height the
 * next boot must rescan. Keeping the two apart lets a short read keep the
 * depths it did establish without letting the wallet record the range as
 * scanned. Both are written by the flush, in its transaction. */

#include "wallet/wallet_sqlite.h"
#include "wallet/wallet.h"
#include "util/ar_step_readonly.h"
#include "util/log_macros.h"
#include <stdint.h>
#include <string.h>

#define WSS_RETRY_WRITE_SQL \
    "INSERT OR REPLACE INTO node_state(key,value)" \
    " VALUES('wallet_scan_retry_from',?)"
#define WSS_RETRY_CLEAR_SQL \
    "DELETE FROM node_state WHERE key='wallet_scan_retry_from'"
#define WSS_RETRY_READ_SQL \
    "SELECT value FROM node_state WHERE key='wallet_scan_retry_from'"

static bool wss_write_retry(struct wallet_sqlite *ws, bool pending,
                            int retry_from)
{
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(ws->db,
                           pending ? WSS_RETRY_WRITE_SQL : WSS_RETRY_CLEAR_SQL,
                           -1, &s, NULL) != SQLITE_OK || !s)
        LOG_FAIL("wallet_sqlite", "write_scan_retry: prepare failed: %s",
                 sqlite3_errmsg(ws->db));
    int32_t h = (int32_t)retry_from;
    if (pending)
        sqlite3_bind_blob(s, 1, &h, 4, SQLITE_TRANSIENT);
    bool ok = AR_STEP_WRITE(s) == SQLITE_DONE;
    sqlite3_finalize(s);
    if (!ok)
        LOG_FAIL("wallet_sqlite", "write_scan_retry: step failed: %s",
                 sqlite3_errmsg(ws->db));
    return true;
}

bool wallet_sqlite_write_scan_state(struct wallet_sqlite *ws,
                                    const struct wallet *w)
{
    if (!ws || !ws->open || !w)
        LOG_FAIL("wallet_sqlite", "write_scan_state: not open");
    return wallet_sqlite_write_scan_height(ws, w->best_block_height) &&
           wss_write_retry(ws, w->scan_retry_pending, w->scan_retry_from);
}

bool wallet_sqlite_read_scan_retry(struct wallet_sqlite *ws, int *retry_from)
{
    if (!ws || !ws->open || !retry_from)
        return false;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(ws->db, WSS_RETRY_READ_SQL, -1, &s, NULL) !=
            SQLITE_OK || !s) {
        LOG_WARN("wallet_sqlite", "read_scan_retry: prepare failed: %s",
                 sqlite3_errmsg(ws->db));
        sqlite3_finalize(s);
        return false;
    }
    bool ok = false;
    if (AR_STEP_ROW_READONLY(s) == SQLITE_ROW) {
        const void *data = sqlite3_column_blob(s, 0);
        if (data && sqlite3_column_bytes(s, 0) >= 4) {
            int32_t h;
            memcpy(&h, data, 4);
            *retry_from = h;
            ok = true;
        }
    }
    sqlite3_finalize(s);
    return ok;
}

bool wallet_sqlite_read_scan_state(struct wallet_sqlite *ws, struct wallet *w)
{
    if (!w)
        return false;
    int height = 0;
    bool have_height = wallet_sqlite_read_scan_height(ws, &height);
    if (have_height)
        w->best_block_height = height;
    int retry_from = 0;
    w->scan_retry_pending = wallet_sqlite_read_scan_retry(ws, &retry_from);
    w->scan_retry_from = retry_from > 0 ? retry_from : 0;
    return have_height;
}
