/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review completion at %d\n", __LINE__); abort(); } } while (0)
static signed_review_fixture fixture;
static uint8_t expected[ZCL_TX_WIRE_MAX];
static size_t expected_length;
static struct { uint8_t before[8], bytes[ZCL_TX_WIRE_MAX], after[8]; } output;
typedef struct {
    uint64_t values[2];
    size_t calls, fail_at, omit_at;
} test_clock;

static zcl_status read_clock(void *context, uint64_t *now)
{
    test_clock *clock = context;
    CHECK(clock != NULL && now != NULL && clock->calls < 2);
    const size_t index = clock->calls++;
    if (clock->calls != clock->omit_at) *now = clock->values[index];
    return clock->calls == clock->fail_at ? ZCL_IO_FAILURE : ZCL_OK;
}

static void setup(zcl_network network, size_t inputs, size_t outputs)
{
    CHECK(signed_review_fixture_init(&fixture, network, inputs, outputs));
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 100, &fixture.block,
        fixture.signatures, inputs, expected, sizeof(expected), &expected_length) == ZCL_OK);
}

static void call(test_clock *clock, size_t capacity, zcl_status status, size_t calls)
{
    const zcl_review_clock source = {read_clock, clock};
    memset(&output, 0xa5, sizeof(output));
    size_t length = SIZE_MAX;
    CHECK(zcl_review_p2pkh_complete(&fixture.owner, fixture.id, &source, &fixture.block,
        fixture.signatures, fixture.spending.input_count, output.bytes, capacity, &length) == status);
    CHECK(clock->calls == calls);
    for (size_t i = 0; i < sizeof(output.before); ++i)
        CHECK(output.before[i] == 0xa5 && output.after[i] == 0xa5);
    if (status == ZCL_OK) {
        CHECK(length == expected_length && memcmp(output.bytes, expected, length) == 0);
        CHECK(fixture.owner.data.last_ms == clock->values[1]);
        CHECK(fixture.owner.data.deadline_ms == 100 + ZCL_REVIEW_LIFETIME_MS);
    } else CHECK(length == SIZE_MAX);
    for (size_t i = status == ZCL_OK ? length : 0; i < sizeof(output.bytes); ++i)
        CHECK(output.bytes[i] == 0xa5);
}

static void profiles(void)
{
    for (size_t network = 0; network < 2; ++network) {
        for (size_t inputs = 1; inputs <= ZCL_TX_INPUT_MAX; ++inputs) {
            for (size_t outputs = 1; outputs <= ZCL_TX_OUTPUT_MAX; outputs += 15) {
                setup((zcl_network)network, inputs, outputs);
                test_clock clock = {{200, 300}, 0, 0, 0};
                call(&clock, SIZE_MAX, ZCL_OK, 2);
                clock = (test_clock){{300, 300}, 0, 0, 0};
                call(&clock, expected_length, ZCL_OK, 2);
                clock = (test_clock){{300, 300}, 0, 0, 0};
                call(&clock, expected_length - 1, ZCL_BUFFER_TOO_SMALL, 1);
            }
        }
    }
}

static void expiry_and_rollback(void)
{
    const uint64_t deadline = 100 + ZCL_REVIEW_LIFETIME_MS;
    const uint64_t starts[] = {99, deadline, 200, 200, 200, 200};
    const uint64_t ends[] = {200, deadline, 199, deadline, UINT64_MAX, deadline - 1};
    const zcl_status statuses[] = {ZCL_CANCELLED, ZCL_TIMED_OUT, ZCL_CANCELLED,
        ZCL_TIMED_OUT, ZCL_TIMED_OUT, ZCL_OK};
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        setup(ZCL_MAINNET, 1, 1);
        test_clock clock = {{starts[i], ends[i]}, 0, 0, 0};
        call(&clock, sizeof(output.bytes), statuses[i], i < 2 ? 1 : 2);
        CHECK(fixture.owner.issued == fixture.id);
        if (statuses[i] != ZCL_OK) {
            const zcl_review_data empty = {0};
            CHECK(memcmp(&fixture.owner.data, &empty, sizeof(empty)) == 0);
        }
    }
}

static void clock_failures(void)
{
    for (size_t phase = 1; phase <= 2; ++phase) {
        setup(ZCL_TESTNET, 1, 1);
        test_clock clock = {{200, UINT64_MAX}, 0, phase, 0};
        call(&clock, sizeof(output.bytes), ZCL_IO_FAILURE, phase);
        CHECK(fixture.owner.data.id == fixture.id);
        CHECK(fixture.owner.data.last_ms == (phase == 1 ? 100 : 200));
        setup(ZCL_TESTNET, 1, 1);
        clock = (test_clock){{200, 300}, 0, 0, phase};
        call(&clock, sizeof(output.bytes), ZCL_TIMED_OUT, phase);
        CHECK(fixture.owner.data.id == 0);
    }
}

static void assembly_refusals(void)
{
    setup(ZCL_MAINNET, 1, 1);
    fixture.signatures[0].der[0] ^= 1;
    test_clock clock = {{200, 300}, 0, 0, 0};
    call(&clock, sizeof(output.bytes), ZCL_INVALID_ENCODING, 1);
    setup(ZCL_MAINNET, 1, 1);
    fixture.block.network = ZCL_TESTNET;
    clock = (test_clock){{200, 300}, 0, 0, 0};
    call(&clock, sizeof(output.bytes), ZCL_UNSUPPORTED, 1);
    setup(ZCL_MAINNET, 1, 1);
    CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    clock = (test_clock){{200, 300}, 0, 0, 0};
    call(&clock, sizeof(output.bytes), ZCL_CANCELLED, 1);
}

static void missing_clock(void)
{
    setup(ZCL_MAINNET, 1, 1);
    size_t length = SIZE_MAX;
    const zcl_review_clock clock = {NULL, NULL};
    memset(&output, 0xa5, sizeof(output));
    CHECK(zcl_review_p2pkh_complete(&fixture.owner, fixture.id, NULL, &fixture.block,
        fixture.signatures, 1, output.bytes, sizeof(output.bytes), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_p2pkh_complete(&fixture.owner, fixture.id, &clock, &fixture.block,
        fixture.signatures, 1, output.bytes, sizeof(output.bytes), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(length == SIZE_MAX && fixture.owner.data.last_ms == 100);
    for (size_t i = 0; i < sizeof(output.bytes); ++i) CHECK(output.bytes[i] == 0xa5);
}

int main(void)
{
    profiles(); expiry_and_rollback(); clock_failures(); assembly_refusals(); missing_clock();
    puts("Review completion: exact signed bytes, final-sample deadlines and refusal atomicity passed");
    return 0;
}
