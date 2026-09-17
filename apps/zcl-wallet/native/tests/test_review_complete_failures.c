/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Completion fault at %d\n", __LINE__); abort(); } } while (0)
static zcl_review_owner owner;
static uint8_t output[ZCL_TX_WIRE_MAX];
static unsigned fault, samples, providers, wipes;
static const void *staging;

static void filled(const void *pointer, size_t length, uint8_t value)
{
    const uint8_t *bytes = pointer;
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

zcl_status zcl_review_p2pkh_wire(zcl_review_owner *state, uint64_t id, uint64_t now,
    const zcl_review_block *block, const zcl_signature *signatures, size_t count,
    uint8_t *wire, size_t capacity, size_t *length)
{
    CHECK(state == &owner && id == 1 && now == 100 && block != NULL && signatures != NULL && count == 1);
    CHECK(providers++ == 0 && samples == 1 && capacity == ZCL_TX_WIRE_MAX && wire != output);
    CHECK(zcl_review_live(state, id, now) == ZCL_OK);
    filled(wire, capacity, 0); filled(output, sizeof(output), 0xa5);
    staging = wire;
    memset(wire, 0x39, capacity); *length = 3;
    if (fault == 2) *length = 0;
    if (fault == 3) *length = SIZE_MAX;
    return fault == 1 ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

void zcl_secure_zero(void *pointer, size_t length)
{
    CHECK(wipes++ == 0 && pointer != NULL && length >= ZCL_TX_WIRE_MAX);
    CHECK(length <= ZCL_TX_WIRE_MAX + 2 * sizeof(size_t));
    if (providers != 0) CHECK(pointer == staging);
    memset(pointer, 0, length);
    filled(pointer, length, 0);
}

static zcl_status clock_read(void *context, uint64_t *now)
{
    CHECK(context == NULL && now != NULL && samples < 2 && wipes == 0);
    ++samples;
    filled(output, sizeof(output), 0xa5);
    *now = samples == 1 ? 100 : 200;
    if (samples == 2) {
        CHECK(providers == 1 && staging != NULL);
        filled(staging, ZCL_TX_WIRE_MAX, 0x39);
        if (fault == 4) return ZCL_IO_FAILURE;
        if (fault == 5) *now = 1000;
    }
    return fault == 6 ? ZCL_IO_FAILURE : ZCL_OK;
}

static void run_case(unsigned mode, zcl_status expected)
{
    fault = mode; samples = 0; providers = 0; wipes = 0; staging = NULL;
    memset(&owner, 0, sizeof(owner));
    owner.issued = 1; owner.data.id = 1; owner.data.last_ms = 100; owner.data.deadline_ms = 1000;
    memset(output, 0xa5, sizeof(output));
    const zcl_review_clock clock = {clock_read, NULL};
    const zcl_review_block block = {0};
    const zcl_signature signature = {0};
    size_t length = SIZE_MAX;
    CHECK(zcl_review_p2pkh_complete(&owner, 1, &clock, &block, &signature, 1,
        output, SIZE_MAX, &length) == expected);
    CHECK(wipes == 1);
    CHECK(providers == (fault == 6 ? 0U : 1U));
    CHECK(samples == ((fault == 0 || fault == 4 || fault == 5) ? 2U : 1U));
    if (expected == ZCL_OK) {
        CHECK(length == 3); filled(output, length, 0x39);
        filled(output + length, sizeof(output) - length, 0xa5);
    } else { CHECK(length == SIZE_MAX); filled(output, sizeof(output), 0xa5); }
}

int main(void)
{
    const zcl_status statuses[] = {ZCL_OK, ZCL_CRYPTO_FAILURE, ZCL_INVALID_ENCODING,
        ZCL_INVALID_ENCODING, ZCL_IO_FAILURE, ZCL_TIMED_OUT, ZCL_IO_FAILURE};
    for (unsigned i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) run_case(i, statuses[i]);
    puts("Completion faults: private staging, dirty provider refusal and full retirement passed");
    return 0;
}
