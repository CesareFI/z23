/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_JNI_REVIEW_INTERNAL_H
#define ZCL_JNI_REVIEW_INTERNAL_H
#include "jni_support.h"
#include "zcl_transaction_review.h"

#define ZCL_REVIEW_PACKET_MAX ((size_t)20 + 17 * ZCL_TX_INPUT_MAX + 7 * ZCL_TX_OUTPUT_MAX)

/* One bounded allocation owns every Java input byte for one open call. */
typedef struct {
    uint8_t draft[ZCL_TX_WIRE_MAX];
    size_t draft_length;
    uint8_t previous[ZCL_TX_INPUT_MAX][ZCL_TX_WIRE_MAX];
    zcl_previous_transaction sources[ZCL_TX_INPUT_MAX];
    size_t count;
} zcl_jni_review_inputs;

/* Internal projection writes private scratch only; discard on any failure. */
zcl_status zcl_jni_review_values(const zcl_review_snapshot *snapshot,
    jlong values[ZCL_REVIEW_PACKET_MAX], size_t *length);
#endif
