/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_header_inspect
#undef zcl_v4_source_inspect
#undef zcl_merkle_branch_check
#undef zcl_secure_zero
#include "commitment_fixture.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Commitment retirement at %d\n", __LINE__); abort(); } } while (0)
static commitment_fixture fixture;
static unsigned fault, calls, clears;
static uintptr_t header_address, source_address;
zcl_status zcl_commitment_test_header(const uint8_t *, size_t, zcl_network, uint32_t, zcl_header_view *);
zcl_status zcl_commitment_test_source(const uint8_t *, size_t, uint32_t, zcl_v4_source *);
zcl_status zcl_commitment_test_branch(const uint8_t[32], const zcl_merkle_branch *, const uint8_t[32]);
void zcl_commitment_test_zero(void *, size_t);

zcl_status zcl_commitment_test_header(const uint8_t *wire, size_t length, zcl_network network,
    uint32_t height, zcl_header_view *view)
{
    CHECK(++calls == 1 && clears == 0);
    CHECK(wire == fixture.request.header && length == fixture.request.header_length);
    CHECK(network == fixture.request.network && height == fixture.request.height);
    header_address = (uintptr_t)view;
    if (fault == 1) { memset(view, 0xa5, sizeof(*view)); return ZCL_CRYPTO_FAILURE; }
    return zcl_header_inspect(wire, length, network, height, view);
}

zcl_status zcl_commitment_test_source(const uint8_t *wire, size_t length, uint32_t index, zcl_v4_source *view)
{
    CHECK(++calls == 2 && clears == 0);
    CHECK(wire == fixture.request.source && length == fixture.request.source_length && index == fixture.request.output_index);
    source_address = (uintptr_t)view;
    if (fault == 2) { memset(view, 0xa5, sizeof(*view)); return ZCL_CRYPTO_FAILURE; }
    return zcl_v4_source_inspect(wire, length, index, view);
}

zcl_status zcl_commitment_test_branch(const uint8_t id[32], const zcl_merkle_branch *branch, const uint8_t root[32])
{
    CHECK(++calls == 3 && clears == 0 && branch == fixture.request.branch);
    CHECK(memcmp(id, fixture.request.transaction_id, 32) == 0);
    for (size_t i = 0; i < 32; ++i) CHECK(root[i] == fixture.header[67 - i]);
    return fault == 3 ? ZCL_CRYPTO_FAILURE : zcl_merkle_branch_check(id, branch, root);
}

void zcl_commitment_test_zero(void *buffer, size_t length)
{
    CHECK(clears++ == 0 && length == sizeof(zcl_source_commitment));
    zcl_source_commitment *work = buffer;
    CHECK((uintptr_t)&work->header == header_address);
    if (calls >= 2) CHECK((uintptr_t)&work->source == source_address);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
}

static void run(zcl_status expected, unsigned expected_calls)
{
    calls = clears = 0;
    zcl_source_commitment value, before;
    memset(&value, 0xa5, sizeof(value)); memcpy(&before, &value, sizeof(value));
    CHECK(zcl_v4_source_commitment_check(&fixture.request, &value) == expected);
    CHECK(calls == expected_calls && clears == (expected_calls == 0 ? 0U : 1U));
    if (expected != ZCL_OK) CHECK(memcmp(&value, &before, sizeof(value)) == 0);
}

int main(void)
{
    CHECK(commitment_fixture_init(&fixture, 7, 3, 2));
    for (fault = 1; fault <= 3; ++fault) run(ZCL_CRYPTO_FAILURE, fault);
    fault = 0; run(ZCL_OK, 3);
    fixture.request.header_id[0] ^= 1; run(ZCL_INVALID_ENCODING, 1); fixture.request.header_id[0] ^= 1;
    fixture.request.transaction_id[0] ^= 1; run(ZCL_INVALID_ENCODING, 2); fixture.request.transaction_id[0] ^= 1;
    fixture.request.source_length = 64; run(ZCL_UNSUPPORTED, 0);
    fixture.request.branch = NULL; run(ZCL_INVALID_ARGUMENT, 0);
    puts("Commitment dirty-component failure, ordering and retirement checks passed"); return 0;
}
