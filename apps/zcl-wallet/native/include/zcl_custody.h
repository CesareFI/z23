/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CUSTODY_H
#define ZCL_CUSTODY_H
#include "zcl_wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Normalized platform observations, not Android enum values or a wire format.
 * The platform adapter must obtain these from AndroidKeyStore KeyInfo, not
 * from preferences, a server, or caller assertions. Policy checks cannot prove
 * hardware authenticity on a compromised OS or a virtualized test device. */
enum { ZCL_KEY_TEE = 1, ZCL_KEY_STRONGBOX = 2 };
enum {
    ZCL_KEY_AES_GCM_ONLY = 1, /* AES; only encrypt/decrypt, GCM, no padding. */
    ZCL_KEY_GENERATED = 2,
    ZCL_KEY_AUTH_REQUIRED = 4,
    ZCL_KEY_HARDWARE_AUTH = 8,
    ZCL_KEY_ON_BODY = 16
};
enum { ZCL_AUTH_STRONG_BIOMETRIC = 1, ZCL_AUTH_DEVICE_CREDENTIAL = 2 };
typedef struct {
    uint32_t key_bits;
    uint32_t hardware;
    uint32_t flags;
    int32_t authentication_seconds;
    uint32_t authentication_methods;
} zcl_wrapping_policy;

/* Pure validation; no allocation, retained pointer, secret or mutable state.
 * Accepts both documented (-1) and current provider (0) per-use encodings. */
zcl_status zcl_wrapping_policy_check(const zcl_wrapping_policy *policy);

#define ZCL_AUTH_WINDOW_MS UINT64_C(90000)
/* Pending prompt/foreground-continuation lifetime only, never an alternative
 * to hardware per-use authentication. Times must come from the same monotonic
 * clock including device sleep. A backward clock or age >=90s fails closed.
 * Subtraction follows ordering checks; no absolute deadline addition occurs. */
zcl_status zcl_authentication_window_check(uint64_t started_ms, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
