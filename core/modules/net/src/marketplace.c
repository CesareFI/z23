/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Purpose: default-off optional marketplace admission at transport edges.
 * Blockchain messages and the verified file service never use these ports. */
#include "net/marketplace.h"
#include "net/msgprocessor.h"
#include "core/serialize.h"
#include "event/event.h"
#include "util/util.h"
#include "util/log_macros.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t g_policy_mutex;
static pthread_once_t g_policy_once = PTHREAD_ONCE_INIT;
static marketplace_root_filter_fn g_root_filter;
static marketplace_wire_filter_fn g_wire_filter;
static uint64_t g_generation = 1;

static void policy_mutex_init(void)
{
    pthread_mutexattr_t attr;
    int rc = pthread_mutexattr_init(&attr);
    if (rc != 0) { LOG_ERROR("market", "mutex attributes failed: %d", rc); abort(); }
    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (rc == 0) rc = pthread_mutex_init(&g_policy_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (rc != 0) { LOG_ERROR("market", "policy mutex failed: %d", rc); abort(); }
}

void marketplace_lock(void)
{
    pthread_once(&g_policy_once, policy_mutex_init);
    pthread_mutex_lock(&g_policy_mutex);
}

void marketplace_unlock(void)
{
    pthread_mutex_unlock(&g_policy_mutex);
}

bool marketplace_enabled(void)
{
    return strcmp(GetArg("-marketplace", "0"), "1") == 0;
}

uint64_t marketplace_generation(void)
{
    marketplace_lock();
    uint64_t generation = g_generation;
    marketplace_unlock();
    return generation;
}

void marketplace_invalidate_pending(void)
{
    marketplace_lock();
    if (g_generation == UINT64_MAX) {
        LOG_ERROR("market", "refusal generation exhausted; refusing reuse");
        abort();
    }
    g_generation++;
    marketplace_unlock();
}

bool marketplace_generation_current(uint64_t generation)
{
    return marketplace_enabled() && generation == marketplace_generation();
}

bool marketplace_message(const char *command)
{
    static const char *const commands[] = {
        "zfilelist", "zfileoffer", "zfilechal", "zfileproof", "zfilepay",
        "zswapquote", "zswapaccept", "zswappartial",
    };
    if (!command) return false;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
        if (strcmp(command, commands[i]) == 0) return true;
    return false;
}

static bool path_prefix(const char *path, const char *prefix)
{
    size_t n = strlen(prefix);
    return strncmp(path, prefix, n) == 0 &&
           (path[n] == '\0' || path[n] == '/' || path[n] == '?');
}

bool marketplace_path(const char *path)
{
    static const char *const prefixes[] = {
        "/market", "/yardsale", "/store", "/api/market",
        "/api/market-contents", "/api/v1/market", "/api/v1/market-contents",
    };
    if (!path) return false;
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        if (path_prefix(path, prefixes[i])) return true;
    return false;
}

bool marketplace_rpc(const char *method)
{
    if (!method) return false;
    /* Local removal must remain available while participation is disabled. */
    if (strcmp(method, "zmarket_refuse") == 0) return false;
    return strncmp(method, "zmarket_", 8) == 0 ||
           strncmp(method, "zswap_", 6) == 0 ||
           strncmp(method, "yardsale_", 9) == 0 ||
           strncmp(method, "storebuy_", 9) == 0 ||
           strncmp(method, "store_", 6) == 0;
}

bool marketplace_path_disabled(const char *path)
{
    return marketplace_path(path) && !marketplace_enabled();
}

bool marketplace_app_disabled(const char *id, size_t size)
{
    if (!id) return true;
    bool market_app = (size == 4 && memcmp(id, "shop", 4) == 0) ||
                      (size == 5 && memcmp(id, "store", 5) == 0) ||
                      (size == 8 && memcmp(id, "yardsale", 8) == 0);
    return market_app && !marketplace_enabled();
}

void marketplace_set_filters(marketplace_root_filter_fn root_filter,
                             marketplace_wire_filter_fn wire_filter)
{
    marketplace_lock();
    g_root_filter = root_filter;
    g_wire_filter = wire_filter;
    marketplace_invalidate_pending();
    marketplace_unlock();
}

bool marketplace_root_allowed(const uint8_t root[32])
{
    if (!marketplace_enabled() || !root) return false;
    marketplace_lock();
    marketplace_root_filter_fn filter = g_root_filter;
    bool allowed = filter && filter(root);
    marketplace_unlock();
    return allowed;
}

bool marketplace_wire_allowed(const char *command, const uint8_t *wire,
                              size_t size)
{
    if (!marketplace_message(command)) return true;
    if (!marketplace_enabled()) return false;
    marketplace_lock();
    marketplace_wire_filter_fn filter = g_wire_filter;
    bool allowed = filter && filter(command, wire, size);
    marketplace_unlock();
    return allowed;
}

bool marketplace_dispatch(const struct msg_dispatch_entry *entry,
                                      struct msg_processor *mp,
                                      struct p2p_node *node,
                                      struct byte_stream *stream)
{
    if (!marketplace_wire_allowed(entry->command, stream->data, stream->size)) {
        event_emitf(EV_BACKPRESSURE_REJECT, (uint32_t)node->id,
                    "%s reason=marketplace-policy", entry->command);
        return true; /* Optional data refused; blockchain peer stays connected. */
    }
    return entry->handler(mp, node, stream);
}
