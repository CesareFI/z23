/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_custody.h"

zcl_status zcl_authentication_window_check(uint64_t started_ms, uint64_t now_ms)
{
    if (now_ms < started_ms) return ZCL_IO_UNCERTAIN;
    if (now_ms - started_ms >= ZCL_AUTH_WINDOW_MS) return ZCL_TIMED_OUT;
    return ZCL_OK;
}

zcl_status zcl_authentication_window_remaining(uint64_t started_ms, uint64_t now_ms,
                                              uint64_t *remaining_ms)
{
    if (remaining_ms == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status status = zcl_authentication_window_check(started_ms, now_ms);
    if (status != ZCL_OK) return status;
    *remaining_ms = ZCL_AUTH_WINDOW_MS - (now_ms - started_ms);
    return ZCL_OK;
}

zcl_status zcl_setup_window_remaining(uint64_t started_ms, uint64_t now_ms,
                                     uint64_t *remaining_ms)
{
    if (remaining_ms == NULL) return ZCL_INVALID_ARGUMENT;
    if (now_ms < started_ms) return ZCL_IO_UNCERTAIN;
    const uint64_t elapsed = now_ms - started_ms;
    if (elapsed >= ZCL_SETUP_WINDOW_MS) return ZCL_TIMED_OUT;
    *remaining_ms = ZCL_SETUP_WINDOW_MS - elapsed;
    return ZCL_OK;
}

zcl_status zcl_wrapping_policy_check(const zcl_wrapping_policy *policy)
{
    if (policy == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (policy->key_bits != 256)
        return ZCL_UNSUPPORTED;
    if (policy->hardware != ZCL_KEY_TEE && policy->hardware != ZCL_KEY_STRONGBOX)
        return ZCL_UNSUPPORTED;
    const uint32_t required = ZCL_KEY_AES_GCM_ONLY | ZCL_KEY_GENERATED |
                              ZCL_KEY_AUTH_REQUIRED | ZCL_KEY_HARDWARE_AUTH;
    if (policy->flags != required)
        return ZCL_UNSUPPORTED;
    if (policy->authentication_seconds != 0 && policy->authentication_seconds != -1)
        return ZCL_UNSUPPORTED;
    if (policy->authentication_methods != (ZCL_AUTH_STRONG_BIOMETRIC | ZCL_AUTH_DEVICE_CREDENTIAL))
        return ZCL_UNSUPPORTED;
    return ZCL_OK;
}
