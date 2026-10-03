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

/* Invocation-only full-source owner. Captured refs/lengths precede allocation;
 * Java refs retire before C preparation. Exactly1..8 spans of1..102000 bytes. */
typedef struct {
    jbyteArray arrays[ZCL_TX_INPUT_MAX];
    size_t lengths[ZCL_TX_INPUT_MAX];
    zcl_previous_transaction sources[ZCL_TX_INPUT_MAX];
    size_t count, total;
    uint8_t *bytes;
} zcl_jni_full_sources;

/* Caller passes zero-initialized private scratch. Captured Java references
 * always retire before return; clear must follow on success or failure. */
zcl_status zcl_jni_full_sources_copy(JNIEnv *env, jobjectArray previous, zcl_jni_full_sources *copy);
void zcl_jni_full_sources_clear(zcl_jni_full_sources *copy);

/* Caller owns the copied draft and holds the review mutex. Parse/count refusal
 * precedes source capture; success grants no review, funding or chain authority. */
zcl_status zcl_jni_review_admit(JNIEnv *env, jobjectArray previous, const uint8_t *wire, size_t length);

/* Caller checked JNI arguments and holds the single review mutex throughout.
 * No source/ref/pointer survives; failure preserves owner/id and JNI exception. */
zcl_status zcl_jni_open_full_review(JNIEnv *env, zcl_review_owner *owner,
    jbyteArray draft, jobjectArray previous, zcl_network network,
    uint64_t fee, uint64_t now, uint64_t *id);
zcl_status zcl_jni_prepare_full_review(JNIEnv *env, zcl_review_owner *owner,
    jobjectArray previous, jobjectArray destinations, jlongArray parameters,
    zcl_network network, uint64_t now, uint64_t *id);

/* Internal projection writes private scratch only; discard on any failure. */
zcl_status zcl_jni_review_values(const zcl_review_snapshot *snapshot,
    jlong values[ZCL_REVIEW_PACKET_MAX], size_t *length);
#endif
