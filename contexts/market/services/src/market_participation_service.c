/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Purpose: compose operator refusal with the existing marketplace codecs.
 * Policy never accepts a signature or changes blockchain validation. */
#include "services/market_participation_service.h"
#include "models/file_offer.h"
#include "models/market_refusal.h"
#include "models/zswap_ad.h"
#include "net/marketplace.h"
#include "core/serialize.h"
#include "zswap/zswap_ceremony.h"
#include "zswap/zswap_yardsale.h"
#include "util/log_macros.h"

#include <stdatomic.h>
#include <string.h>

static _Atomic(struct node_db *) g_policy_db;

static bool root_allowed(const uint8_t root[32])
{
    return db_market_refusal_allows(atomic_load(&g_policy_db), root);
}

static bool signed_offer_allowed(const uint8_t *wire, size_t size)
{
    struct file_offer offer;
    return file_offer_auth_decode(wire, size, &offer) == FILE_OFFER_AUTH_OK &&
           root_allowed(offer.root_hash);
}

static bool quote_allowed(const uint8_t *wire, size_t size)
{
    struct zswap_quote_v1 quote;
    uint8_t root[32];
    return zswap_quote_decode(wire, size, &quote) == ZSWAP_QUOTE_OK &&
           zswap_quote_root(&quote, root) == ZSWAP_QUOTE_OK &&
           root_allowed(root);
}

static bool accept_allowed(const uint8_t *wire, size_t size)
{
    struct zswap_accept_v1 accept;
    return zswap_accept_decode(wire, size, &accept) == ZSWAP_CEREMONY_OK &&
           root_allowed(accept.quote_root);
}

static bool partial_allowed(const uint8_t *wire, size_t size)
{
    struct zswap_partial_v1 partial;
    return zswap_partial_decode(wire, size, &partial) == ZSWAP_CEREMONY_OK &&
           root_allowed(partial.quote_root);
}

static bool payment_allowed(const uint8_t *wire, size_t size)
{
    struct file_payment payment;
    struct file_offer offer;
    return file_payment_auth_decode(wire, size, &payment) == FILE_PAYMENT_AUTH_OK &&
           db_file_offer_find_by_id(atomic_load(&g_policy_db),
                                    payment.offer_id, &offer) &&
           root_allowed(offer.root_hash);
}

static bool legacy_list_rows_allowed(struct byte_stream *stream)
{
    uint8_t count = 0;
    if (!stream_read_u8(stream, &count)) return false;
    for (unsigned i = 0; i < count; i++) {
        struct file_offer offer;
        if (!file_offer_deserialize(&offer, stream) ||
            !root_allowed(offer.root_hash)) return false;
    }
    return stream->read_pos == stream->size;
}

static bool legacy_list_allowed(const uint8_t *wire, size_t size)
{
    struct byte_stream stream;
    stream_init_from_data(&stream, wire, size);
    bool allowed = legacy_list_rows_allowed(&stream);
    stream_free(&stream);
    return allowed;
}

static bool chunk_message_allowed(const uint8_t *wire, size_t size)
{
    /* Both legacy challenge and proof codecs start with the content root.
     * Their existing handlers still enforce the remaining message shape. */
    return wire && size >= 32 && root_allowed(wire);
}

static bool wire_allowed(const char *command, const uint8_t *wire, size_t size)
{
    static const struct {
        const char *command;
        bool (*allows)(const uint8_t *, size_t);
    } rules[] = {
        {"zfilelist", legacy_list_allowed},
        {"zfileoffer", signed_offer_allowed},
        {"zfilechal", chunk_message_allowed},
        {"zfileproof", chunk_message_allowed},
        {"zfilepay", payment_allowed},
        {"zswapquote", quote_allowed},
        {"zswapaccept", accept_allowed},
        {"zswappartial", partial_allowed},
    };
    if (!wire || !command) return false;
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++)
        if (strcmp(command, rules[i].command) == 0)
            return rules[i].allows(wire, size);
    return false;
}

void market_participation_set_context(struct node_db *ndb)
{
    marketplace_lock();
    atomic_store(&g_policy_db, ndb);
    marketplace_set_filters(root_allowed, wire_allowed);
    marketplace_unlock();
}

static struct zcl_result refuse_locked(const uint8_t root[32])
{
    if (!root) return ZCL_ERR(-1, "listing root is required");
    struct node_db *ndb = atomic_load(&g_policy_db);
    struct market_refusal refusal;
    memcpy(refusal.root, root, sizeof(refusal.root));
    if (!db_market_refusal_save(ndb, &refusal))
        return ZCL_ERR(-2, "listing refusal could not be persisted");
    marketplace_invalidate_pending();
    file_market_forget(root);
    zswap_yardsale_forget(root);
    if (!db_file_offer_delete(ndb, root) || !db_zswap_ad_delete(ndb, root))
        return ZCL_ERR(-3, "listing is refused; projection cleanup failed");
    LOG_INFO("market", "local listing refusal persisted and projections removed");
    return ZCL_OK;
}

struct zcl_result market_participation_refuse(const uint8_t root[32])
{
    marketplace_lock();
    struct zcl_result result = refuse_locked(root);
    marketplace_unlock();
    return result;
}

bool market_participation_offer_allowed(const uint8_t offer_id[32])
{
    struct file_offer offer;
    return marketplace_enabled() && offer_id &&
           db_file_offer_find_by_id(atomic_load(&g_policy_db), offer_id, &offer) &&
           marketplace_root_allowed(offer.root_hash);
}
