/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include "transaction_review_internal.h"
#include "transaction_sighash.h"
#include "transaction_context.h"
#include "context_reference.h"
#ifdef ZCL_SIGHASH_ORACLE
#include "sighash_oracle.h"
#endif
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
#ifdef ZCL_FULL_SOURCE_REVIEW
#include "source_assessment_fixture.h"
static source_assessment_fixture full_fixture;
#define fixture full_fixture.base
#define zcl_review_open zcl_review_open_full_sources
#else
static assessment_fixture fixture;
#endif
static uint8_t draft[ZCL_TX_WIRE_MAX];
static size_t draft_length;
static bool initialized;

typedef struct {
    uint64_t issued;
    uint64_t active;
    uint64_t opened;
    uint64_t latest;
} review_model;

static uint64_t read_time(const uint8_t *data)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) value |= (uint64_t)data[i] << (8 * i);
    return value;
}

static uint64_t selected_time(const review_model *model, uint8_t mode, uint64_t raw)
{
    switch (mode % 7) {
    case 0: return model->latest;
    case 1: return model->opened + 89999; /* Successful openings guarantee room. */
    case 2: return model->opened + 90000;
    case 3: return model->latest == 0 ? 0 : model->latest - 1;
    case 4: return UINT64_MAX - raw % 90002;
    case 5: return raw % 100000;
    default: return raw;
    }
}

static uint64_t selected_id(const review_model *model, uint8_t mode)
{
    switch (mode % 5) {
    case 0: return model->active;
    case 1: return model->issued;
    case 2: return model->issued + 1; /* Bounded to at most 65 issues here. */
    case 3: return UINT64_MAX;
    default: return 0;
    }
}

static zcl_status read_model(review_model *model, uint64_t id, uint64_t now)
{
    if (id == 0 || id != model->active) return ZCL_CANCELLED;
    if (now < model->latest) {
        model->active = 0;
        return ZCL_CANCELLED;
    }
    /* Subtraction from opening time independently checks the fixed lifetime. */
    if (now - model->opened >= 90000) {
        model->active = 0;
        return ZCL_TIMED_OUT;
    }
    model->latest = now;
    return ZCL_OK;
}

static void open_operation(zcl_review_owner *owner, review_model *model, uint64_t now)
{
    uint64_t id = UINT64_MAX;
    zcl_review_owner before;
    memcpy(&before, owner, sizeof(before));
    const zcl_status status = zcl_review_open(owner, draft, draft_length, ZCL_MAINNET,
        fixture.sources, 2, 500, now, &id);
    zcl_status expected = ZCL_OK;
    if (model->active != 0) expected = ZCL_BUSY;
    else if (now > UINT64_MAX - 90000) expected = ZCL_OUT_OF_RANGE;
    if (status != expected) abort();
    if (status != ZCL_OK) {
        if (id != UINT64_MAX || memcmp(owner, &before, sizeof(before)) != 0) abort();
        return;
    }
    ++model->issued;
    model->active = model->issued;
    model->opened = model->latest = now;
    if (id != model->active) abort();
}

static void check_context(const zcl_review_context *context)
{
    if (context->lock_time != fixture.spending.lock_time) abort();
    if (context->expiry_height != fixture.spending.expiry_height) abort();
    for (size_t i = 0; i < fixture.spending.input_count; ++i) {
        const zcl_review_input *input = &context->inputs[i];
        if (memcmp(input->previous_txid, fixture.spending.inputs[i].previous_txid, 32) != 0) abort();
        if (input->previous_index != fixture.spending.inputs[i].previous_index) abort();
        if (input->sequence != fixture.spending.inputs[i].sequence) abort();
    }
}

static void check_assessment(const zcl_transaction_assessment *assessment)
{
    if (assessment->fee != 500 || assessment->input_total != 11000) abort();
    if (assessment->output_total != 10500 || assessment->serialized_size != draft_length) abort();
    if (assessment->network != ZCL_MAINNET || assessment->maximum_fee != 500) abort();
}

static void snapshot_operation(zcl_review_owner *owner, review_model *model, uint64_t id, uint64_t now)
{
    struct { uint64_t before; zcl_review_snapshot value; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_review_snapshot original;
    memcpy(&original, &box.value, sizeof(original));
    const zcl_status expected = read_model(model, id, now);
    if (zcl_review_snapshot_get(owner, id, now, &box.value) != expected) abort();
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (expected != ZCL_OK) {
        if (memcmp(&original, &box.value, sizeof(original)) != 0) abort();
        return;
    }
    if (box.value.remaining_ms != 90000 - (now - model->opened)) abort();
    check_assessment(&box.value.assessment);
    check_context(&box.value.context);
    /* Caller is free to mutate its copy; no subsequent operation may see it. */
    memset(&box.value, 0, sizeof(box.value));
}

static void copy_operation(zcl_review_owner *owner, review_model *model, uint64_t id,
                           uint64_t now, size_t capacity)
{
    uint8_t wire[ZCL_TX_WIRE_MAX];
    memset(wire, 0xa5, sizeof(wire));
    size_t length = SIZE_MAX;
    zcl_status expected = read_model(model, id, now);
    if (expected == ZCL_OK && capacity < draft_length) expected = ZCL_BUFFER_TOO_SMALL;
    if (zcl_review_copy_wire(owner, id, now, wire, capacity, &length) != expected) abort();
    size_t unchanged = 0;
    if (expected == ZCL_OK) {
        if (length != draft_length || memcmp(wire, draft, length) != 0) abort();
        unchanged = length;
    } else if (length != SIZE_MAX) abort();
    for (size_t i = unchanged; i < sizeof(wire); ++i) if (wire[i] != 0xa5) abort();
    memset(wire, 0, sizeof(wire));
}

static void cancel_operation(zcl_review_owner *owner, review_model *model, uint64_t id)
{
    const bool match = id != 0 && id == model->active;
    if (zcl_review_cancel(owner, id) != (match ? ZCL_OK : ZCL_CANCELLED)) abort();
    if (match) model->active = 0;
}

static void expected_digest(uint32_t branch, uint8_t *expected)
{
    const zcl_tx_output *previous = &fixture.previous[0].outputs[0];
#ifdef ZCL_SIGHASH_ORACLE
    zcl_test_sighash_all(draft, draft_length, 0, previous->script, previous->script_len,
        previous->value, branch, expected, 32);
#else
    if (zcl_transaction_sighash_all(&fixture.spending, 0, previous->script, previous->script_len,
        previous->value, branch, expected, 32) != ZCL_OK) abort();
#endif
}

static void hash_operation(zcl_review_owner *owner, review_model *model, uint64_t id,
                           uint64_t now, const uint8_t *step)
{
    uint8_t digest[64], expected[32];
    memset(digest, 0xa5, sizeof(digest));
    const size_t index = step[2] == 255 ? SIZE_MAX : step[2] % 4;
    const size_t capacity = step[3] % 65;
    const uint32_t branch = (uint32_t)(read_time(step + 2) & UINT32_MAX);
    zcl_status status = read_model(model, id, now);
    if (status == ZCL_OK && capacity < 32) status = ZCL_BUFFER_TOO_SMALL;
    if (status == ZCL_OK && index >= 2) status = ZCL_OUT_OF_RANGE;
    if (status == ZCL_OK && index == 1) status = ZCL_UNSUPPORTED;
    if (zcl_review_sighash_p2pkh(owner, id, now, index, branch, digest, capacity) != status) abort();
    if (status == ZCL_OK) {
        expected_digest(branch, expected);
        if (memcmp(digest, expected, sizeof(expected)) != 0) abort();
    }
    for (size_t i = status == ZCL_OK ? 32 : 0; i < sizeof(digest); ++i) if (digest[i] != 0xa5) abort();
}

static bool model_final(const zcl_review_block *block)
{
    if (fixture.spending.inputs[0].sequence == UINT32_MAX && fixture.spending.inputs[1].sequence == UINT32_MAX)
        return true;
    const uint32_t lock = fixture.spending.lock_time;
    if (lock == 0) return true;
    if (lock >= UINT32_C(500000000)) return block->lock_time_cutoff > lock;
    return block->height > lock;
}

static zcl_status model_context(const zcl_review_block *block, size_t index, uint32_t *branch)
{
    if (index >= 2) return ZCL_OUT_OF_RANGE;
    if (block->network != ZCL_MAINNET) return ZCL_UNSUPPORTED;
    if (block->lock_time_cutoff > INT64_MAX) return ZCL_OUT_OF_RANGE;
    const zcl_status status = zcl_test_context_branch(block->network, block->height, branch);
    if (status != ZCL_OK) return status;
    const uint32_t expiry = fixture.spending.expiry_height;
    if (expiry != 0 && expiry < block->height) return ZCL_OUT_OF_RANGE;
    if (!model_final(block)) return ZCL_OUT_OF_RANGE;
    return index == 1 ? ZCL_UNSUPPORTED : ZCL_OK;
}

static zcl_review_block candidate_block(const uint8_t *step)
{
    static const uint32_t heights[] = {19, 20, 476968, 476969, 585318, 585322, 78856};
    const uint64_t raw = read_time(step + 2);
    zcl_review_block block = {(zcl_network)(step[2] % 3), (uint32_t)(raw & UINT32_MAX), raw};
    const size_t selection = step[4] % 8;
    if (selection < 7) block.height = heights[selection];
    if (step[5] % 3 == 0) block.lock_time_cutoff = 0;
    if (step[5] % 3 == 1) block.lock_time_cutoff = INT64_MAX;
    return block;
}

static void context_operation(zcl_review_owner *owner, review_model *model, uint64_t id,
    uint64_t now, const uint8_t *step)
{
    uint8_t digest[64], expected[32];
    memset(digest, 0xa5, sizeof(digest));
    const size_t capacity = step[3] % 65;
    const size_t index = step[6] == 255 ? SIZE_MAX : step[6] % 4;
    const zcl_review_block block = candidate_block(step);
    uint32_t branch = 0;
    zcl_status status = read_model(model, id, now);
    if (status == ZCL_OK && capacity < 32) status = ZCL_BUFFER_TOO_SMALL;
    if (status == ZCL_OK) status = model_context(&block, index, &branch);
    if (zcl_review_sighash_context(owner, id, now, index, &block, digest, capacity) != status) abort();
    if (status == ZCL_OK) {
        expected_digest(branch, expected);
        if (memcmp(digest, expected, 32) != 0) abort();
    }
    for (size_t i = status == ZCL_OK ? 32 : 0; i < sizeof(digest); ++i) if (digest[i] != 0xa5) abort();
}

static void arbitrary_branch(uint64_t fields)
{
    const zcl_network network = (zcl_network)((fields >> 32) % 3);
    const uint32_t height = (uint32_t)(fields & UINT32_MAX);
    uint32_t branch = UINT32_MAX, expected = UINT32_MAX;
    const zcl_status status = zcl_test_context_branch(network, height, &expected);
    if (zcl_transaction_v4_branch(network, height, &branch) != status || branch != expected) abort();
}

static void invariant(const zcl_review_owner *owner, const review_model *model)
{
    static const zcl_review_data zero;
    if (owner->issued != model->issued) abort();
    if (model->active == 0) {
        if (memcmp(&owner->data, &zero, sizeof(zero)) != 0) abort();
        return;
    }
    if (owner->data.id != model->active || owner->data.wire_length != draft_length) abort();
    if (memcmp(owner->data.wire, draft, draft_length) != 0) abort();
    if (owner->data.last_ms != model->latest) abort();
}

static void operation(zcl_review_owner *owner, review_model *model, const uint8_t *step)
{
    const uint64_t now = selected_time(model, step[1], read_time(step + 2));
    const uint64_t id = selected_id(model, step[0] / 8);
    switch (step[0] % 8) {
    case 0: open_operation(owner, model, now); break;
    case 1: snapshot_operation(owner, model, id, now); break;
    case 2: copy_operation(owner, model, id, now, ZCL_TX_WIRE_MAX); break;
    case 3: copy_operation(owner, model, id, now, step[2]); break;
    case 4: cancel_operation(owner, model, id); break;
    case 5: hash_operation(owner, model, id, now, step); break;
    case 6: context_operation(owner, model, id, now, step); break;
    default: zcl_review_clear(owner); model->active = 0; break;
    }
    invariant(owner, model);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 640) return 0;
    if (!initialized) {
#ifdef ZCL_FULL_SOURCE_REVIEW
        if (!source_assessment_init(&full_fixture, 7)) abort();
#else
        if (!assessment_fixture_init(&fixture)) abort();
#endif
        initialized = true;
    }
    const uint64_t fields = size >= 8 ? read_time(data) : 0;
    arbitrary_branch(fields);
    fixture.spending.lock_time = (uint32_t)(fields & UINT32_MAX);
    fixture.spending.expiry_height = (uint32_t)(fields % ZCL_TX_EXPIRY_LIMIT);
    fixture.spending.inputs[0].sequence = (uint32_t)(fields >> 32);
    fixture.spending.inputs[1].sequence = UINT32_MAX - fixture.spending.inputs[0].sequence;
    if (zcl_transaction_serialize(&fixture.spending, draft, sizeof(draft), &draft_length) != ZCL_OK) abort();
    zcl_review_owner owner = {0};
    review_model model = {0};
    open_operation(&owner, &model, 0);
    for (size_t offset = 0; size - offset >= 10; offset += 10) operation(&owner, &model, data + offset);
    zcl_review_clear(&owner);
    model.active = 0;
    invariant(&owner, &model);
    return 0;
}
