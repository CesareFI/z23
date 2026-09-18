/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "source_commitment_internal.h"
#include "zcl_keys.h"
#include <string.h>

static zcl_status header_identity(const zcl_source_commitment_request *request,
    zcl_header_view *view)
{
    const zcl_status status = zcl_header_inspect(request->header, request->header_length,
        request->network, request->height, view);
    if (status != ZCL_OK) return status;
    return memcmp(view->hash, request->header_id, 32) == 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
}

static zcl_status source_identity(const zcl_source_commitment_request *request,
    bool mixed, zcl_source_view *view)
{
    const zcl_status status = mixed
        ? zcl_source_inspect(request->source, request->source_length, request->output_index, view)
        : zcl_v4_source_inspect(request->source, request->source_length, request->output_index, view);
    if (status != ZCL_OK) return status;
    return memcmp(view->transaction_id, request->transaction_id, 32) == 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
}

static zcl_status commitment_check(const zcl_source_commitment_request *request,
    bool mixed, zcl_source_commitment *output)
{
    if (request == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->branch == NULL) return ZCL_INVALID_ARGUMENT;
    if (request->source_length == 64) return ZCL_UNSUPPORTED;
    zcl_source_commitment candidate = {0};
    zcl_status status = header_identity(request, &candidate.header);
    if (status == ZCL_OK) status = source_identity(request, mixed, &candidate.source);
    if (status == ZCL_OK) status = zcl_merkle_branch_check(candidate.source.transaction_id,
        request->branch, candidate.header.merkle);
    if (status == ZCL_OK) *output = candidate;
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}

zcl_status zcl_v4_source_commitment_check(const zcl_source_commitment_request *request,
    zcl_source_commitment *output)
{
    return commitment_check(request, false, output);
}

zcl_status zcl_source_commitment_check(const zcl_source_commitment_request *request,
    zcl_source_commitment *output)
{
    return commitment_check(request, true, output);
}
