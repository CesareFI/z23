/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_draft_internal.h"

static zcl_status draft_u32(jlong value, uint32_t *output)
{
    if (value < 0 || (uint64_t)value > UINT32_MAX) return ZCL_OUT_OF_RANGE;
    *output = (uint32_t)value;
    return ZCL_OK;
}

static zcl_status draft_money(jlong value, uint64_t *output)
{
    if (value < 0 || (uint64_t)value > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    *output = (uint64_t)value;
    return ZCL_OK;
}

static zcl_status draft_header(const jlong *values, zcl_draft_request *request)
{
    zcl_status status = draft_u32(values[0], &request->lock_time);
    if (status != ZCL_OK) return status;
    status = draft_u32(values[1], &request->expiry_height);
    if (status != ZCL_OK) return status;
    if (request->expiry_height >= ZCL_TX_EXPIRY_LIMIT) return ZCL_OUT_OF_RANGE;
    return draft_money(values[2], &request->maximum_fee);
}

static zcl_status draft_input_fields(const jlong *values, zcl_draft_request *request)
{
    for (size_t i = 0; i < request->input_count; ++i) {
        const size_t offset = 3 + 2 * i; /* Counts checked before this private loop. */
        zcl_status status = draft_u32(values[offset], &request->inputs[i].output_index);
        if (status != ZCL_OK) return status;
        status = draft_u32(values[offset + 1], &request->inputs[i].sequence);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status draft_output_fields(const jlong *values, zcl_draft_request *request)
{
    const size_t first = 3 + 2 * request->input_count;
    for (size_t i = 0; i < request->output_count; ++i) {
        const zcl_status status = draft_money(values[first + i], &request->outputs[i].value);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

static zcl_status field_shape(const jlong *values, size_t count, const zcl_draft_request *request)
{
    if (values == NULL || request == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->input_count == 0 || request->input_count > ZCL_TX_INPUT_MAX) return ZCL_OUT_OF_RANGE;
    if (request->output_count == 0 || request->output_count > ZCL_TX_OUTPUT_MAX) return ZCL_OUT_OF_RANGE;
    return count == 3 + 2 * request->input_count + request->output_count ? ZCL_OK : ZCL_INVALID_ARGUMENT;
}

zcl_status zcl_jni_draft_fields(const jlong *values, size_t count, zcl_draft_request *request)
{
    zcl_status status = field_shape(values, count, request);
    if (status != ZCL_OK) return status;
    status = draft_header(values, request);
    if (status != ZCL_OK) return status;
    status = draft_input_fields(values, request);
    if (status != ZCL_OK) return status;
    return draft_output_fields(values, request);
}
