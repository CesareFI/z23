/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_sync_watch.h"
#include "zcl_keys.h"
#include <string.h>

void zcl_sync_watch_close(zcl_sync_watch *watch)
{
    if (watch != NULL) memset(watch, 0, sizeof(*watch));
}

zcl_status zcl_sync_watch_init(zcl_sync_watch *watch, const uint8_t *address, size_t length,
    zcl_network network, const uint8_t *source_id, size_t source_length)
{
    if (watch == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_sync_watch_close(watch);
    if (source_id == NULL || source_length != sizeof(watch->source_id)) return ZCL_INVALID_ARGUMENT;
    zcl_address parsed = {0};
    const zcl_status status = zcl_address_parse(address, length, network, &parsed);
    zcl_secure_zero(&parsed, sizeof(parsed));
    if (status != ZCL_OK) return status;
    memcpy(watch->address, address, sizeof(watch->address));
    memcpy(watch->source_id, source_id, sizeof(watch->source_id));
    watch->network = network;
    watch->initialized = true;
    return ZCL_OK;
}

zcl_status zcl_sync_watch_init_with_history(zcl_sync_watch *watch, const uint8_t *address,
    size_t length, zcl_network network, const uint8_t *source_id, size_t source_length)
{
    const zcl_status status = zcl_sync_watch_init(watch, address, length, network, source_id, source_length);
    if (status == ZCL_OK) watch->include_history = true;
    return status;
}

static zcl_status stop(zcl_sync_watch *watch, zcl_status reason)
{
    const zcl_status status = zcl_sync_abort(&watch->attempt, reason);
    watch->last_fault = status;
    watch->in_flight = false;
    return status;
}

static zcl_status clock_update(zcl_sync_watch *watch, uint64_t now_ms)
{
    if (now_ms < watch->clock_ms) {
        memset(&watch->last, 0, sizeof(watch->last));
        watch->has_report = false;
        /* A new explicit attempt may recover using the new clock epoch. */
        watch->clock_ms = now_ms;
        return stop(watch, ZCL_IO_UNCERTAIN);
    }
    watch->clock_ms = now_ms;
    if (watch->in_flight && now_ms >= watch->deadline_ms) return stop(watch, ZCL_TIMED_OUT);
    return ZCL_OK;
}

zcl_status zcl_sync_watch_check_attempt(const zcl_sync_watch *watch, uint64_t token)
{
    if (watch == NULL || !watch->initialized) return ZCL_INVALID_ARGUMENT;
    if (token == 0 || token != watch->sequence || !watch->in_flight) return ZCL_CANCELLED;
    return ZCL_OK;
}

static zcl_status begin_limits(uint64_t now_ms, uint64_t timeout_ms, uint32_t first_id,
    bool include_history)
{
    if (timeout_ms == 0 || timeout_ms > ZCL_SYNC_TIMEOUT_MAX_MS || now_ms > UINT64_MAX - timeout_ms)
        return ZCL_OUT_OF_RANGE;
    const uint32_t remaining_ids = include_history ? 6 : 5;
    if (first_id == 0 || first_id > UINT32_MAX - remaining_ids) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status start_attempt(zcl_sync_watch *watch, uint32_t first_id)
{
    if (watch->include_history)
        return zcl_sync_start_with_history(&watch->attempt, watch->address,
            sizeof(watch->address), watch->network, first_id);
    return zcl_sync_start(&watch->attempt, watch->address,
        sizeof(watch->address), watch->network, first_id);
}

zcl_status zcl_sync_watch_begin(zcl_sync_watch *watch, uint64_t now_ms, uint64_t timeout_ms,
    uint32_t first_id, uint64_t *token)
{
    if (watch == NULL || !watch->initialized || token == NULL) return ZCL_INVALID_ARGUMENT;
    if (watch->in_flight) return ZCL_BUSY;
    const zcl_status limits = begin_limits(now_ms, timeout_ms, first_id, watch->include_history);
    if (limits != ZCL_OK) return limits;
    if (watch->sequence == UINT64_MAX) return ZCL_RESOURCE_EXHAUSTED;
    const zcl_status clock = clock_update(watch, now_ms);
    if (clock != ZCL_OK) return clock;
    const zcl_status status = start_attempt(watch, first_id);
    if (status != ZCL_OK) return status;
    watch->deadline_ms = now_ms + timeout_ms;
    watch->in_flight = true;
    *token = ++watch->sequence;
    return ZCL_OK;
}

zcl_status zcl_sync_watch_request(zcl_sync_watch *watch, uint64_t token, uint64_t now_ms,
    uint8_t *output, size_t capacity, size_t *length)
{
    const zcl_status owner = zcl_sync_watch_check_attempt(watch, token);
    if (owner != ZCL_OK) return owner;
    const zcl_status clock = clock_update(watch, now_ms);
    if (clock != ZCL_OK) return clock;
    return zcl_sync_request(&watch->attempt, output, capacity, length);
}

static zcl_status publish(zcl_sync_watch *watch, uint64_t now_ms)
{
    zcl_sync_report report = {0};
    const zcl_status status = zcl_sync_get_report(&watch->attempt, &report);
    if (status == ZCL_OK) {
        watch->last = report;
        watch->observed_ms = now_ms;
        watch->has_report = true;
        watch->in_flight = false;
        watch->last_fault = ZCL_OK;
    }
    zcl_secure_zero(&report, sizeof(report));
    return status == ZCL_OK ? ZCL_OK : stop(watch, status);
}

zcl_status zcl_sync_watch_reply(zcl_sync_watch *watch, uint64_t token, uint64_t now_ms,
    const uint8_t *frame, size_t length)
{
    const zcl_status owner = zcl_sync_watch_check_attempt(watch, token);
    if (owner != ZCL_OK) return owner;
    const zcl_status clock = clock_update(watch, now_ms);
    if (clock != ZCL_OK) return clock;
    const zcl_status status = zcl_sync_reply(&watch->attempt, frame, length);
    if (status != ZCL_OK) return stop(watch, status);
    if (watch->attempt.phase == ZCL_SYNC_DONE) return publish(watch, now_ms);
    return ZCL_OK;
}

zcl_status zcl_sync_watch_fail(zcl_sync_watch *watch, uint64_t token, zcl_status reason)
{
    const zcl_status owner = zcl_sync_watch_check_attempt(watch, token);
    if (owner != ZCL_OK) return owner;
    if (reason <= ZCL_OK || reason > ZCL_TLS_FAILURE) return ZCL_INVALID_ARGUMENT;
    return stop(watch, reason);
}

zcl_status zcl_sync_watch_snapshot(zcl_sync_watch *watch, uint64_t now_ms,
    zcl_sync_snapshot *snapshot)
{
    if (watch == NULL || !watch->initialized || snapshot == NULL) return ZCL_INVALID_ARGUMENT;
    /* Clock errors are represented in the returned snapshot's last_fault. */
    (void)clock_update(watch, now_ms);
    zcl_sync_snapshot result = {0};
    result.refreshing = watch->in_flight;
    /* clock_update stopped every expired attempt, so this subtraction is safe. */
    if (result.refreshing) result.next_change_ms = watch->deadline_ms - now_ms;
    result.last_fault = watch->last_fault;
    memcpy(result.source_id, watch->source_id, sizeof(result.source_id));
    if (watch->has_report) {
        result.report = watch->last;
        result.age_ms = now_ms - watch->observed_ms;
        result.freshness = ZCL_BALANCE_STALE;
        if (!watch->in_flight && watch->last_fault == ZCL_OK && result.age_ms < ZCL_SYNC_FRESH_MS) {
            result.freshness = ZCL_BALANCE_UNVERIFIED;
            result.next_change_ms = ZCL_SYNC_FRESH_MS - result.age_ms;
        }
    }
    *snapshot = result;
    zcl_secure_zero(&result, sizeof(result));
    return ZCL_OK;
}
