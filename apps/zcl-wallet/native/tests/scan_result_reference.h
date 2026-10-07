/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_SCAN_RESULT_REFERENCE_H
#define ZCL_SCAN_RESULT_REFERENCE_H
#include "zcl_wallet.h"

/* Test-only coherence check, not an independent payment-parser oracle. Borrowed
 * public text and request remain stable for this synchronous call. No padding
 * or unused label/message tail is compared; every semantic field is checked. */
bool scan_result_matches(const uint8_t *text, size_t length, zcl_network network,
                         const zcl_payment_request *request);
#endif
