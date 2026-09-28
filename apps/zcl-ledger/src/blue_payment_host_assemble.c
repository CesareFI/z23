/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_host_assemble.h"
#include "zcl_tx_review.h"
#include "zcl_tx_script_facts.h"

#include <string.h>

typedef struct {
    const uint8_t *wire;
    size_t offsets[ZCL_TX_STREAM_MAX_INPUTS];
    size_t script_lengths[ZCL_TX_STREAM_MAX_INPUTS];
    size_t count;
} input_offsets;

static bool overlaps(const void *left, size_t left_length,
    const void *right, size_t right_length) {
    uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    return a <= b ? b - a < left_length : a - b < right_length;
}

static bool collect_empty_input(void *context, const zcl_tx_input *input) {
    input_offsets *offsets = context;
    if (!input || input->index != offsets->count ||
        offsets->count >= ZCL_TX_STREAM_MAX_INPUTS ||
        input->script_length || input->script <= offsets->wire)
        return false;
    size_t position = (size_t)(input->script - offsets->wire) - 1;
    if (offsets->wire[position] != 0) return false;
    offsets->offsets[offsets->count++] = position;
    return true;
}

static bool collect_signed_input(void *context, const zcl_tx_input *input) {
    input_offsets *offsets = context;
    if (!input || input->index != offsets->count ||
        offsets->count >= ZCL_TX_STREAM_MAX_INPUTS ||
        input->script <= offsets->wire || input->script_length > 252)
        return false;
    size_t position = (size_t)(input->script - offsets->wire) - 1;
    if (offsets->wire[position] != input->script_length) return false;
    offsets->offsets[offsets->count] = position;
    offsets->script_lengths[offsets->count++] = input->script_length;
    return true;
}

static bool canonical_signature(
    const blue_payment_verified_signature *signature) {
    if ((signature->public_key[0] != 2 && signature->public_key[0] != 3) ||
        signature->der_length < 8 ||
        signature->der_length > BLUE_ECDSA_DER_MAX) return false;
    uint8_t normalized[BLUE_ECDSA_DER_MAX];
    size_t length = 0;
    return blue_ecdsa_der_low_s(signature->der, signature->der_length,
               normalized, &length) &&
           length == signature->der_length &&
           memcmp(normalized, signature->der, length) == 0;
}

static size_t put_script(uint8_t *output,
    const blue_payment_verified_signature *signature) {
    size_t length = signature->der_length;
    output[0] = (uint8_t)(length + 36);
    output[1] = (uint8_t)(length + 1);
    memcpy(output + 2, signature->der, length);
    output[length + 2] = 1;
    output[length + 3] = 33;
    memcpy(output + length + 4, signature->public_key, 33);
    return length + 37;
}

static bool unsigned_v4(const uint8_t *wire, size_t length, size_t count) {
    return wire && count && count <= ZCL_TX_STREAM_MAX_INPUTS &&
        length >= 8 && length <= ZCL_TX_REVIEW_MAX_BYTES &&
        wire[0] == 4 && !wire[1] && !wire[2] && wire[3] == 0x80 &&
        memcmp(wire + 4, "\x85\x20\x2f\x89", 4) == 0;
}

static bool review_unsigned(const uint8_t *wire, size_t length,
    size_t count, zcl_tx_review *review, input_offsets *offsets) {
    return zcl_tx_review_parse(wire, length, review) == 0 &&
        review->transparent_inputs == count &&
        review->transparent_outputs && !review->sapling_spends &&
        !review->sapling_outputs && !review->sprout_joinsplits &&
        !review->value_balance_zat &&
        zcl_tx_inputs_visit(wire, length,
            collect_empty_input, offsets) == 0 && offsets->count == count;
}

static bool assembled_length(const blue_payment_verified_signature *signatures,
    const uint8_t (*expected_digests)[32], size_t count,
    size_t unsigned_length, size_t capacity, size_t *needed) {
    size_t size = unsigned_length;
    for (size_t i = 0; i < count; ++i) {
        size_t added = (size_t)signatures[i].der_length + 36;
        if (signatures[i].index != i ||
            (signatures[i].path != BLUE_PAYMENT_INPUT_EXTERNAL &&
             signatures[i].path != BLUE_PAYMENT_INPUT_INTERNAL) ||
            memcmp(signatures[i].digest, expected_digests[i], 32) != 0 ||
            !canonical_signature(&signatures[i]) ||
            size > capacity || size > ZCL_TX_REVIEW_MAX_BYTES ||
            added > capacity - size ||
            added > ZCL_TX_REVIEW_MAX_BYTES - size)
            return false;
        size += added;
    }
    *needed = size;
    return true;
}

static size_t write_scripts(const uint8_t *wire, size_t length,
    const input_offsets *offsets,
    const blue_payment_verified_signature *signatures,
    uint8_t *output) {
    size_t read = 0, written = 0;
    for (size_t i = 0; i < offsets->count; ++i) {
        size_t bytes = offsets->offsets[i] - read;
        memcpy(output + written, wire + read, bytes);
        written += bytes;
        written += put_script(output + written, &signatures[i]);
        read = offsets->offsets[i] + 1;
    }
    memcpy(output + written, wire + read, length - read);
    return written + length - read;
}

static bool same_review(const uint8_t *output, size_t length,
    const zcl_tx_review *original) {
    zcl_tx_review signed_review;
    return zcl_tx_review_parse(output, length, &signed_review) == 0 &&
        signed_review.transparent_inputs == original->transparent_inputs &&
        signed_review.transparent_outputs == original->transparent_outputs &&
        signed_review.transparent_output_zat ==
            original->transparent_output_zat &&
        signed_review.lock_time == original->lock_time &&
        signed_review.expiry_height == original->expiry_height &&
        !signed_review.sapling_spends && !signed_review.sapling_outputs &&
        !signed_review.sprout_joinsplits &&
        !signed_review.value_balance_zat;
}

static bool same_wire_except_scripts(const uint8_t *unsigned_wire,
    size_t unsigned_length, const input_offsets *original,
    const uint8_t *signed_wire, size_t signed_length,
    const blue_payment_verified_signature *signatures) {
    input_offsets signed_inputs = {.wire = signed_wire};
    if (zcl_tx_inputs_visit(signed_wire, signed_length,
            collect_signed_input, &signed_inputs) != 0 ||
        signed_inputs.count != original->count) return false;
    size_t source = 0, result = 0;
    for (size_t i = 0; i < original->count; ++i) {
        size_t span = original->offsets[i] - source;
        if (signed_inputs.offsets[i] - result != span ||
            memcmp(unsigned_wire + source, signed_wire + result, span) != 0 ||
            signed_inputs.script_lengths[i] !=
                (size_t)signatures[i].der_length + 36)
            return false;
        source = original->offsets[i] + 1;
        result = signed_inputs.offsets[i] + 1 +
            signed_inputs.script_lengths[i];
    }
    return signed_length - result == unsigned_length - source &&
        memcmp(unsigned_wire + source, signed_wire + result,
            unsigned_length - source) == 0;
}

bool blue_payment_host_assemble(const uint8_t *unsigned_wire,
    size_t unsigned_length,
    const blue_payment_verified_signature *signatures,
    const uint8_t (*expected_digests)[32], size_t count,
    uint8_t *output, size_t capacity, size_t *output_length) {
    if (output_length) *output_length = 0;
    if (!output_length || !signatures || !expected_digests || !output ||
        !unsigned_v4(unsigned_wire, unsigned_length, count)) return false;
    zcl_tx_review review;
    input_offsets offsets = {.wire = unsigned_wire};
    size_t needed = 0;
    if (!review_unsigned(unsigned_wire, unsigned_length, count,
            &review, &offsets) ||
        !assembled_length(signatures, expected_digests, count, unsigned_length,
            capacity, &needed) ||
        overlaps(unsigned_wire, unsigned_length, output, needed) ||
        overlaps(signatures, count * sizeof *signatures, output, needed) ||
        overlaps(expected_digests, count * sizeof *expected_digests,
            output, needed)) return false;
    size_t written = write_scripts(unsigned_wire, unsigned_length,
        &offsets, signatures, output);
    if (written != needed || !same_review(output, written, &review) ||
        !same_wire_except_scripts(unsigned_wire, unsigned_length, &offsets,
            output, written, signatures)) {
        memset(output, 0, written);
        return false;
    }
    *output_length = written;
    return true;
}
