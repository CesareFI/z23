/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "commitment_fixture.h"
#include <stdlib.h>
#include <string.h>
static commitment_fixture fixture, before;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3 || size > 4096) return 0;
    const uint32_t width = (uint32_t)data[1] + 1;
    if (!commitment_fixture_init(&fixture, data[0] % 8U, width, data[2] % width)) abort();
    const size_t maximum = fixture.request.source_length < size - 3 ? fixture.request.source_length : size - 3;
    for (size_t i = 0; i < maximum; ++i) fixture.funding.wire[0][i] ^= data[i + 3];
    if (!commitment_fixture_bind(&fixture)) abort();
    zcl_v4_source source = {0};
    const zcl_status expected = zcl_v4_source_inspect(fixture.request.source, fixture.request.source_length, 0, &source);
    memcpy(&before, &fixture, sizeof(fixture));
    zcl_source_commitment output, unchanged;
    memset(&output, 0xa5, sizeof(output)); memcpy(&unchanged, &output, sizeof(output));
    const zcl_status status = zcl_v4_source_commitment_check(&fixture.request, &output);
    if (status != expected || memcmp(&fixture, &before, sizeof(fixture)) != 0) abort();
    if (status != ZCL_OK) { if (memcmp(&output, &unchanged, sizeof(output)) != 0) abort(); }
    else if (memcmp(output.source.transaction_id, fixture.request.transaction_id, 32) != 0 ||
             memcmp(output.header.hash, fixture.request.header_id, 32) != 0) abort();
    return 0;
}
