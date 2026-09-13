/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_REVIEW_INTERNAL_H
#define ZCL_TRANSACTION_REVIEW_INTERNAL_H
#include "zcl_transaction_review.h"

/* Separate unit keeps the owned parsed transaction out of the publisher's
 * optimized stack frame. Caller supplies private, initialized candidate data. */
zcl_status zcl_review_prepare(const uint8_t *wire, size_t length, zcl_network network,
                              const zcl_previous_transaction *previous, size_t previous_count,
                              uint64_t maximum_fee, zcl_review_data *candidate);
#endif
