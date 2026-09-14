/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_fixture.h"
#include "zcl_keys.h"
#include <mbedtls/ripemd160.h>
#include <mbedtls/sha256.h>
#include <string.h>

static bool public_hash(size_t index, uint8_t *hash)
{
    uint8_t secret[32] = {0}, blinding[32] = {1}, key[33] = {0}, sha[32] = {0};
    if (index >= ZCL_TX_INPUT_MAX) return false;
    secret[31] = (uint8_t)(index + 1);
    const zcl_status status = zcl_public_key(secret, 32, blinding, 32, key, 33);
    zcl_secure_zero(secret, sizeof(secret));
    if (status != ZCL_OK) return false;
    if (mbedtls_sha256(key, sizeof(key), sha, 0) != 0) return false;
    return mbedtls_ripemd160(sha, sizeof(sha), hash) == 0;
}

static bool funding_outputs(signed_review_fixture *fixture, zcl_network network, size_t count)
{
    fixture->funding.input_count = 1; fixture->funding.output_count = count;
    fixture->funding.inputs[0].previous_index = 1;
    fixture->funding.inputs[0].sequence = UINT32_MAX;
    for (size_t i = 0; i < count; ++i) {
        zcl_address destination = {network, ZCL_P2PKH, {0}};
        if (!public_hash(i, destination.hash)) return false;
        memcpy(fixture->hashes[i], destination.hash, 20);
        zcl_tx_output *output = &fixture->funding.outputs[i]; output->value = 10000;
        if (zcl_address_script(&destination, output->script, sizeof(output->script), &output->script_len) != ZCL_OK)
            return false;
    }
    return zcl_transaction_serialize(&fixture->funding, fixture->funding_wire,
        sizeof(fixture->funding_wire), &fixture->funding_length) == ZCL_OK;
}

static bool spending_outputs(signed_review_fixture *fixture, zcl_network network, size_t count)
{
    fixture->spending.output_count = count;
    for (size_t i = 0; i < count; ++i) {
        zcl_address destination = {network, i % 2 == 0 ? ZCL_P2PKH : ZCL_P2SH, {0}};
        memset(destination.hash, (int)(0x60 + i), 20);
        zcl_tx_output *output = &fixture->spending.outputs[i];
        output->value = i == 0 ? fixture->spending.input_count * UINT64_C(10000) - 500 - (count - 1) * 100 : 100;
        if (zcl_address_script(&destination, output->script, sizeof(output->script), &output->script_len) != ZCL_OK)
            return false;
    }
    return true;
}

static bool open_review(signed_review_fixture *fixture, zcl_network network)
{
    zcl_previous_transaction previous[ZCL_TX_INPUT_MAX] = {{0}};
    for (size_t i = 0; i < fixture->spending.input_count; ++i) {
        previous[i].wire = fixture->funding_wire;
        previous[i].length = fixture->funding_length;
    }
    if (zcl_transaction_serialize(&fixture->spending, fixture->unsigned_wire,
        sizeof(fixture->unsigned_wire), &fixture->unsigned_length) != ZCL_OK) return false;
    return zcl_review_open(&fixture->owner, fixture->unsigned_wire, fixture->unsigned_length,
        network, previous, fixture->spending.input_count, 500, 100, &fixture->id) == ZCL_OK;
}

static bool sign_inputs(signed_review_fixture *fixture)
{
    for (size_t i = 0; i < fixture->spending.input_count; ++i) {
        uint8_t secret[32] = {0}; secret[31] = (uint8_t)(i + 1);
        zcl_status status = zcl_review_sighash_context(&fixture->owner, fixture->id, 100,
            i, &fixture->block, fixture->digests[i], 32);
        if (status == ZCL_OK)
            status = zcl_signature_create(secret, 32, fixture->digests[i], 32, &fixture->signatures[i]);
        zcl_secure_zero(secret, sizeof(secret));
        if (status != ZCL_OK) return false;
    }
    return true;
}

bool signed_review_fixture_init(signed_review_fixture *fixture, zcl_network network,
    size_t inputs, size_t outputs)
{
    if (fixture == NULL || inputs == 0 || inputs > ZCL_TX_INPUT_MAX || outputs == 0 || outputs > ZCL_TX_OUTPUT_MAX)
        return false;
    memset(fixture, 0, sizeof(*fixture));
    fixture->block.network = network;
    fixture->block.height = network == ZCL_MAINNET ? 1000000 : 100000;
    fixture->spending.input_count = inputs;
    fixture->spending.expiry_height = fixture->block.height + 10;
    if (!funding_outputs(fixture, network, inputs)) return false;
    for (size_t i = 0; i < inputs; ++i) {
        zcl_tx_input *input = &fixture->spending.inputs[i];
        input->previous_index = (uint32_t)i; input->sequence = UINT32_MAX;
        if (zcl_transaction_id(&fixture->funding, input->previous_txid, 32) != ZCL_OK) return false;
    }
    if (!spending_outputs(fixture, network, outputs) || !open_review(fixture, network)) return false;
    return sign_inputs(fixture);
}
