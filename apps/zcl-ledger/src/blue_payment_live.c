/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_live.h"

#include <openssl/sha.h>
#include <string.h>

typedef struct {
    zcl_tx_stream *stream;
    blue_payment_live_plan *plan;
    uint32_t seen;
} output_capture;

static bool capture_end(void *context, uint32_t index,
    uint64_t amount_zat, zcl_tx_stream_output_type type,
    const uint8_t hash160[20]) {
    output_capture *capture = context;
    (void)amount_zat;
    (void)type;
    (void)hash160;
    if (index != capture->seen || index >= capture->plan->count)
        return false;
    capture->plan->output_end[index] = capture->stream->received + 1;
    ++capture->seen;
    return true;
}

bool blue_payment_live_prepare(const uint8_t *wire, size_t length,
    uint32_t branch_id, blue_payment_live_plan *plan) {
    if (!plan) return false;
    memset(plan, 0, sizeof *plan);
    blue_payment_live_plan checked = {0};
    zcl_tx_stream parser;
    zcl_tx_stream_facts facts;
    checked.branch_id = branch_id;
    checked.wire_length = (uint32_t)length;
    output_capture capture = {.stream = &parser, .plan = &checked};
    if (!wire || !length || length > ZCL_TX_STREAM_MAX_BYTES ||
        !blue_payment_simulate(wire, length, branch_id,
            checked.screens, &checked.count) ||
        !zcl_tx_stream_begin(&parser, (uint32_t)length) ||
        !zcl_tx_stream_feed(&parser, wire, length, NULL,
            capture_end, &capture) ||
        !zcl_tx_stream_finish(&parser, &facts) ||
        capture.seen != checked.count || facts.outputs != checked.count ||
        !SHA256(wire, length, checked.wire_hash)) return false;
    *plan = checked;
    return true;
}

static bool send_command(blue_payment_live_exchange exchange, void *context,
    uint8_t instruction, const uint8_t *body, uint8_t length,
    const uint8_t *expected, size_t expected_length) {
    uint8_t apdu[5 + 128] = {0xa5, instruction, 0, 0, length};
    uint8_t reply[64];
    size_t reply_length = 0;
    if (length) memcpy(apdu + 5, body, length);
    return exchange(context, apdu, 5 + length, reply, sizeof reply,
                    &reply_length) &&
        reply_length == expected_length + 2 &&
        (!expected_length || !memcmp(reply, expected, expected_length)) &&
        reply[expected_length] == 0x90 && reply[expected_length + 1] == 0;
}

static void put_u32(uint8_t bytes[4], uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
}

static bool feed_range(const uint8_t *wire, size_t start, size_t end,
    uint8_t pass, bool output_at_end,
    blue_payment_live_exchange exchange, void *context) {
    if (end <= start) return false;
    while (start < end) {
        size_t count = end - start < 128 ? end - start : 128;
        uint8_t expected[2] = {pass,
            output_at_end && start + count == end ? 1 : 0};
        if (!send_command(exchange, context, 0x21, wire + start,
                (uint8_t)count, expected, sizeof expected)) return false;
        start += count;
    }
    return true;
}

static bool first_two_passes(const uint8_t *wire, size_t length,
    uint8_t outputs, blue_payment_live_exchange exchange, void *context) {
    for (uint8_t pass = 1; pass <= 2; ++pass) {
        uint8_t expected[2] = {(uint8_t)(pass + 1), outputs};
        if (!feed_range(wire, 0, length, pass, false, exchange, context) ||
            !send_command(exchange, context, 0x22, NULL, 0,
                          expected, sizeof expected)) return false;
    }
    return true;
}

static bool remaining_outputs(const uint8_t *wire, size_t length,
    const blue_payment_live_plan *plan,
    blue_payment_live_exchange exchange,
    blue_payment_live_continue continuation, void *context) {
    size_t position = 0;
    for (uint32_t index = 0; index < plan->count; ++index) {
        size_t end = plan->output_end[index];
        uint8_t expected[6] = {1, 3, 0, 0,
            (uint8_t)plan->count, (uint8_t)(index + 1)};
        if (end <= position || end > length ||
            !feed_range(wire, position, end, 3, true,
                        exchange, context) ||
            !continuation(context, index, &plan->screens[index]) ||
            !send_command(exchange, context, 0x25, NULL, 0,
                          expected, sizeof expected)) return false;
        position = end;
    }
    return position == length ||
        feed_range(wire, position, length, 3, false, exchange, context);
}

bool blue_payment_live_run(const uint8_t *wire, size_t length,
    const blue_payment_live_plan *plan,
    blue_payment_live_exchange exchange,
    blue_payment_live_continue continuation, void *context) {
    static const uint8_t identity[] = {'Z', 'C', 'L', 9, 3};
    if (!wire || !plan || !exchange || !continuation ||
        !plan->count || plan->count > BLUE_PAYMENT_REVIEW_MAX_OUTPUTS ||
        length != plan->wire_length) return false;
    uint8_t actual_hash[32];
    if (!SHA256(wire, length, actual_hash) ||
        memcmp(actual_hash, plan->wire_hash, sizeof actual_hash) ||
        !send_command(exchange, context, 0x01, NULL, 0,
                      identity, sizeof identity)) return false;
    uint8_t begin[12];
    put_u32(begin, (uint32_t)length);
    put_u32(begin + 4, 0);
    put_u32(begin + 8, plan->branch_id);
    if (!send_command(exchange, context, 0x20, begin, sizeof begin,
                      NULL, 0)) {
        (void)send_command(exchange, context, 0x24, NULL, 0, NULL, 0);
        return false;
    }
    bool valid = first_two_passes(wire, length, (uint8_t)plan->count,
                    exchange, context) &&
        remaining_outputs(wire, length, plan,
                          exchange, continuation, context);
    uint8_t count = (uint8_t)plan->count;
    if (valid) valid = send_command(exchange, context, 0x23, NULL, 0,
                                    &count, 1);
    if (!valid) (void)send_command(exchange, context, 0x24, NULL, 0, NULL, 0);
    return valid;
}
