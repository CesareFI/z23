/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_reference.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static signed_review_fixture baselines[4], fixture;
static bool initialized;
static zcl_review_owner model, before;
static zcl_review_snapshot snapshot;
static uint8_t expected_wire[ZCL_TX_WIRE_MAX];
typedef struct { uint8_t before[8], wire[ZCL_TX_WIRE_MAX], after[8]; } wire_box;
typedef struct {
    zcl_review_owner *owner;
    const zcl_review_block *block;
    const zcl_signature *signatures;
    uint64_t id, now;
    size_t count, capacity;
    bool null_wire, null_length;
} invocation;

static void initialize(void)
{
    if (initialized) return;
    for (size_t i = 0; i < 4; ++i)
        if (!signed_review_fixture_init(&baselines[i], (zcl_network)(i / 2), i % 2 == 0 ? 1 : 8, 16)) abort();
    initialized = true;
}

static uint64_t read_time(const uint8_t *data)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) value |= (uint64_t)data[i] << (8 * i);
    return value;
}

static void prepare_state(unsigned state)
{
    const uint64_t id = fixture.id;
    if (state == 1 || state == 4) { if (zcl_review_cancel(&fixture.owner, id) != ZCL_OK) abort(); }
    if (state == 2 && zcl_review_snapshot_get(&fixture.owner, id, 90100, &snapshot) != ZCL_TIMED_OUT) abort();
    if (state == 3 && zcl_review_snapshot_get(&fixture.owner, id, 99, &snapshot) != ZCL_CANCELLED) abort();
    if (state == 5) zcl_review_clear(&fixture.owner);
    if (state != 4) return;
    zcl_previous_transaction sources[8] = {{0}};
    for (size_t i = 0; i < fixture.spending.input_count; ++i) {
        sources[i].wire = fixture.funding_wire; sources[i].length = fixture.funding_length;
    }
    if (zcl_review_open(&fixture.owner, fixture.unsigned_wire, fixture.unsigned_length,
        fixture.block.network, sources, fixture.spending.input_count, 500, 100, &fixture.id) != ZCL_OK) abort();
}

static void mutate_block(const uint8_t *data)
{
    const uint32_t value = (uint32_t)data[1] | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16);
    switch (data[0] % 7) {
        case 1: fixture.block.network = (zcl_network)(data[1] % 3); break;
        case 2: fixture.block.height = value; break;
        case 3: fixture.block.height = UINT32_MAX; break;
        case 4: fixture.block.lock_time_cutoff = UINT64_MAX; break;
        case 5: fixture.block.height = fixture.spending.expiry_height + data[1] % 2; break;
        case 6: fixture.block.lock_time_cutoff = INT64_MAX; break;
        default: break;
    }
}

static void mutate_signatures(const uint8_t *data, size_t size, const signed_review_fixture *baseline)
{
    for (size_t offset = 16; offset + 4 <= size; offset += 4) {
        const uint8_t *step = data + offset;
        zcl_signature *signature = &fixture.signatures[step[0] % fixture.spending.input_count];
        switch (step[1] % 4) {
            case 0: signature->der[step[2] % 72] ^= step[3]; break;
            case 1: signature->public_key[step[2] % 33] ^= step[3]; break;
            case 2: signature->der_len = step[2] == 255 ? SIZE_MAX : (size_t)step[2] * 256 + step[3]; break;
            default: *signature = baseline->signatures[step[2] % baseline->spending.input_count]; break;
        }
    }
}

static invocation request(const uint8_t *data)
{
    invocation call;
    memset(&call, 0, sizeof(call));
    call.owner = (data[0] & 4U) != 0 ? NULL : &fixture.owner;
    call.block = (data[0] & 8U) != 0 ? NULL : &fixture.block;
    call.signatures = (data[0] & 16U) != 0 ? NULL : fixture.signatures;
    call.null_wire = (data[0] & 32U) != 0; call.null_length = (data[0] & 64U) != 0;
    call.id = (data[2] & 64U) != 0 ? fixture.id + 1 : (data[2] & 32U) != 0 ? 1 : fixture.id;
    const uint64_t raw = read_time(data + 4);
    call.now = (data[1] & 64U) != 0 ? raw : 100 + raw % 90001;
    call.count = (data[2] & 128U) != 0 ? data[2] % 10 : fixture.spending.input_count;
    if (data[2] == 255) call.count = SIZE_MAX;
    call.capacity = data[3] < 128 ? ZCL_TX_WIRE_MAX : (size_t)(data[3] - 128) * 16;
    return call;
}

static zcl_status model_live(const invocation *call)
{
    if (call->owner == NULL || call->block == NULL || call->signatures == NULL || call->null_wire || call->null_length)
        return ZCL_INVALID_ARGUMENT;
    if (call->id == 0 || call->id != model.data.id) return ZCL_CANCELLED;
    if (call->now < model.data.last_ms) { memset(&model.data, 0, sizeof(model.data)); return ZCL_CANCELLED; }
    if (call->now >= model.data.deadline_ms) { memset(&model.data, 0, sizeof(model.data)); return ZCL_TIMED_OUT; }
    model.data.last_ms = call->now;
    if (call->count == 0 || call->count > 8 || call->count != model.data.assessment.input_count) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static void check_bytes(const wire_box *box, bool valid, size_t length, size_t expected_length)
{
    if (valid && (length != expected_length || memcmp(box->wire, expected_wire, length) != 0)) abort();
    if (!valid && length != SIZE_MAX) abort();
    for (size_t i = valid ? length : 0; i < sizeof(box->wire); ++i) if (box->wire[i] != 0xa5) abort();
    for (size_t i = 0; i < 8; ++i) if (box->before[i] != 0xa5 || box->after[i] != 0xa5) abort();
}

static void execute(const signed_review_fixture *baseline, const invocation *call)
{
    wire_box box;
    memset(&box, 0xa5, sizeof(box)); size_t length = SIZE_MAX, expected_length = 0;
    memcpy(&model, &fixture.owner, sizeof(model));
    const zcl_status initial = model_live(call);
    bool valid = false;
    if (initial == ZCL_OK)
        valid = signed_review_reference(baseline, call->block, call->signatures, call->count, expected_wire, &expected_length)
            && call->capacity >= expected_length;
    const zcl_status status = zcl_review_p2pkh_wire(call->owner, call->id, call->now, call->block, call->signatures,
        call->count, call->null_wire ? NULL : box.wire, call->capacity, call->null_length ? NULL : &length);
    if (initial != ZCL_OK && status != initial) abort();
    if ((status == ZCL_OK) != valid || memcmp(&model, &fixture.owner, sizeof(model)) != 0) abort();
    check_bytes(&box, valid, length, expected_length);
}

typedef struct { uint64_t times[2]; size_t calls; } completion_clock;

static zcl_status sample_completion(void *context, uint64_t *now)
{
    completion_clock *clock = context;
    if (clock == NULL || now == NULL || clock->calls >= 2) abort();
    *now = clock->times[clock->calls++];
    return ZCL_OK;
}

static zcl_status model_completion(uint64_t finish)
{
    if (finish < model.data.last_ms) { memset(&model.data, 0, sizeof(model.data)); return ZCL_CANCELLED; }
    if (finish >= model.data.deadline_ms) { memset(&model.data, 0, sizeof(model.data)); return ZCL_TIMED_OUT; }
    model.data.last_ms = finish;
    return ZCL_OK;
}

static void execute_completion(const signed_review_fixture *baseline, const invocation *call, uint64_t finish)
{
    wire_box box;
    memset(&box, 0xa5, sizeof(box)); size_t length = SIZE_MAX, expected_length = 0;
    memcpy(&fixture.owner, &before, sizeof(before));
    memcpy(&model, &before, sizeof(before));
    zcl_status expected = model_live(call);
    bool assembled = false;
    if (expected == ZCL_OK)
        assembled = signed_review_reference(baseline, call->block, call->signatures, call->count,
            expected_wire, &expected_length) && call->capacity >= expected_length;
    if (assembled) expected = model_completion(finish);
    completion_clock clock = {{call->now, finish}, 0};
    const zcl_review_clock source = {sample_completion, &clock};
    const zcl_status status = zcl_review_p2pkh_complete(call->owner, call->id, &source,
        call->block, call->signatures, call->count, call->null_wire ? NULL : box.wire,
        call->capacity, call->null_length ? NULL : &length);
    if (expected != ZCL_OK && status != expected) abort();
    const bool valid = assembled && expected == ZCL_OK;
    if ((status == ZCL_OK) != valid || memcmp(&model, &fixture.owner, sizeof(model)) != 0) abort();
    const size_t calls = expected == ZCL_INVALID_ARGUMENT ? 0 : assembled ? 2 : 1;
    if (clock.calls != calls) abort();
    check_bytes(&box, valid, length, expected_length);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 16 || size > 144) return 0;
    initialize();
    const signed_review_fixture *baseline = &baselines[data[0] % 4];
    memcpy(&fixture, baseline, sizeof(fixture));
    prepare_state((data[1] & 7U) % 6);
    mutate_block(data + 12); mutate_signatures(data, size, baseline);
    const invocation call = request(data);
    memcpy(&before, &fixture.owner, sizeof(before));
    execute(baseline, &call);
    const uint64_t raw_finish = read_time(data + 8);
    const uint64_t finish = (data[12] & 128U) != 0 ? raw_finish : 100 + raw_finish % 90001;
    execute_completion(baseline, &call, finish);
    zcl_review_clear(&fixture.owner);
    return 0;
}
