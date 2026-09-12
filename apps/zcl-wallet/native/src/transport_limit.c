/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "transport_internal.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

struct zcl_net_cancel { atomic_bool requested; };

zcl_status zcl_net_cancel_create(zcl_net_cancel **output)
{
    if (output == NULL || *output != NULL) return ZCL_INVALID_ARGUMENT;
    /* Constant size; caller is sole owner after successful publication. */
    zcl_net_cancel *cancel = malloc(sizeof(*cancel));
    if (cancel == NULL) return ZCL_RESOURCE_EXHAUSTED;
    atomic_init(&cancel->requested, false);
    *output = cancel;
    return ZCL_OK;
}

void zcl_net_cancel_request(zcl_net_cancel *cancel)
{
    if (cancel != NULL) atomic_store_explicit(&cancel->requested, true, memory_order_relaxed);
}

void zcl_net_cancel_destroy(zcl_net_cancel **cancel)
{
    if (cancel == NULL) return;
    free(*cancel);
    *cancel = NULL;
}

zcl_status zcl_net_now(uint64_t *milliseconds)
{
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return ZCL_IO_FAILURE;
    if (now.tv_sec < 0 || now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
        return ZCL_IO_FAILURE;
    if ((uint64_t)now.tv_sec > (UINT64_MAX - 999) / 1000) return ZCL_OUT_OF_RANGE;
    *milliseconds = (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
    return ZCL_OK;
}

zcl_status zcl_net_limit_start(const zcl_net_cancel *cancel, uint32_t milliseconds,
                               zcl_net_limit *limit)
{
    if (cancel == NULL || limit == NULL) return ZCL_INVALID_ARGUMENT;
    if (milliseconds == 0 || milliseconds > 60000) return ZCL_OUT_OF_RANGE;
    uint64_t now = 0;
    const zcl_status status = zcl_net_now(&now);
    if (status != ZCL_OK) return status;
    if (now > UINT64_MAX - milliseconds) return ZCL_OUT_OF_RANGE;
    if (atomic_load_explicit(&cancel->requested, memory_order_relaxed)) return ZCL_CANCELLED;
    *limit = (zcl_net_limit){cancel, now + milliseconds};
    return ZCL_OK;
}

zcl_status zcl_net_limit_check(const zcl_net_limit *limit)
{
    if (limit == NULL || limit->cancel == NULL) return ZCL_INVALID_ARGUMENT;
    if (atomic_load_explicit(&limit->cancel->requested, memory_order_relaxed)) return ZCL_CANCELLED;
    uint64_t now = 0;
    const zcl_status status = zcl_net_now(&now);
    if (status != ZCL_OK) return status;
    if (now >= limit->deadline_ms) return ZCL_TIMED_OUT;
    return limit->deadline_ms - now > 60000 ? ZCL_OUT_OF_RANGE : ZCL_OK;
}

zcl_status zcl_net_fail(zcl_transport *transport, zcl_status status)
{
    transport->fault = status;
    transport->verified = false;
    return status;
}
