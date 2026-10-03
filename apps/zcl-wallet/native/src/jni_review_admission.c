/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_review_internal.h"
#include "jni_draft_internal.h"
#include "zcl_keys.h"

/* Keep the parsed public transaction out of the caller's wire/source frame.
 * The core codec owns the predicates; no local wire offsets are interpreted. */
zcl_status zcl_jni_review_admit(JNIEnv *env, jobjectArray previous, const uint8_t *wire, size_t length)
{
    size_t count = 0;
    zcl_transparent_tx transaction = {0};
    zcl_status status = zcl_jni_draft_count(env, previous, ZCL_TX_INPUT_MAX, &count);
    if (status == ZCL_OK) status = zcl_transaction_parse(wire, length, &transaction);
    if (status == ZCL_OK && count != transaction.input_count) status = ZCL_INVALID_ARGUMENT;
    zcl_secure_zero(&transaction, sizeof(transaction));
    return status;
}
