/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_tx_stream.h"
#include "zcl_tx_review.h"
#include "zcl_tx_script_facts.h"

#include <stdlib.h>

typedef struct {
    uint32_t inputs, outputs;
    uint64_t output_zat;
} observations;

static bool see_input(void *context, uint32_t index,
                      const uint8_t outpoint[36], uint32_t sequence) {
    observations *seen = context;
    (void)outpoint;
    (void)sequence;
    if (index != seen->inputs++) abort();
    return true;
}

static bool see_output(void *context, uint32_t index, uint64_t amount,
                       zcl_tx_stream_output_type type,
                       const uint8_t hash160[20]) {
    observations *seen = context;
    (void)hash160;
    if (index != seen->outputs++ ||
        (type != ZCL_TX_STREAM_P2PKH && type != ZCL_TX_STREAM_P2SH)) abort();
    seen->output_zat += amount;
    return true;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > ZCL_TX_STREAM_MAX_BYTES) return 0;
    zcl_tx_stream stream;
    if (!zcl_tx_stream_begin(&stream, (uint32_t)size)) return 0;
    observations seen = {0};
    for (size_t offset = 0; offset < size;) {
        size_t chunk = 1 + data[offset] % 64;
        if (chunk > size - offset) chunk = size - offset;
        if (!zcl_tx_stream_feed(&stream, data + offset, chunk,
                                see_input, see_output, &seen)) return 0;
        offset += chunk;
    }
    zcl_tx_stream_facts facts;
    if (!zcl_tx_stream_finish(&stream, &facts)) return 0;
    zcl_tx_review full;
    zcl_tx_script_facts scripts;
    if (zcl_tx_review_parse(data, size, &full) < 0 ||
        zcl_tx_script_facts_parse(data, size, &scripts) < 0 ||
        facts.inputs != full.transparent_inputs ||
        facts.outputs != full.transparent_outputs ||
        facts.output_zat != full.transparent_output_zat ||
        facts.lock_time != full.lock_time ||
        facts.expiry_height != full.expiry_height ||
        facts.inputs != seen.inputs || facts.outputs != seen.outputs ||
        facts.output_zat != seen.output_zat ||
        scripts.p2pkh_outputs + scripts.p2sh_outputs != facts.outputs ||
        full.sapling_spends || full.sapling_outputs ||
        full.sprout_joinsplits || full.value_balance_zat) abort();
    return 0;
}
