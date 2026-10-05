/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_draft_internal.h"
#include "jni_review_internal.h"
#include "transaction_review_internal.h"
#include "zcl_keys.h"

zcl_status zcl_jni_prepare_full_review(JNIEnv *env, zcl_review_owner *owner,
    jobjectArray previous, jobjectArray destinations, jlongArray parameters,
    zcl_network network, uint64_t now, uint64_t *id)
{
    zcl_jni_full_sources copy = {0};
    zcl_draft_request request = {0};
    uint8_t wire[ZCL_TX_WIRE_MAX] = {0};
    size_t length = 0;
    request.network = network;
    zcl_status status = zcl_jni_draft_count(env, destinations, ZCL_TX_OUTPUT_MAX, &request.output_count);
    if (status == ZCL_OK)
        status = zcl_jni_draft_count(env, previous, ZCL_TX_INPUT_MAX, &request.input_count);
    /* Java array lengths are immutable. Admit the bounded scalar and address
     * packets before capturing or allocating up to 816000 source bytes. */
    if (status == ZCL_OK) status = zcl_jni_draft_parameters(env, parameters, &request);
    if (status == ZCL_OK) status = zcl_jni_draft_destinations(env, destinations, &request);
    if (status == ZCL_OK) status = zcl_jni_full_sources_copy(env, previous, &copy);
    for (size_t i = 0; i < copy.count; ++i) request.inputs[i].previous = copy.sources[i];
    if (status == ZCL_OK) status = zcl_jni_draft_full_wire(&request, wire, sizeof(wire), &length);
    if (status == ZCL_OK)
        status = zcl_review_open_full_sources(owner, wire, length, network,
            copy.sources, copy.count, request.maximum_fee, now, id);
    /* Construction and opening see the same C-owned bytes. Clear every borrowed
     * descriptor and current wire before the one source allocation retires. */
    zcl_secure_zero(&request, sizeof(request));
    zcl_secure_zero(wire, sizeof(wire));
    zcl_jni_full_sources_clear(&copy);
    return status;
}
