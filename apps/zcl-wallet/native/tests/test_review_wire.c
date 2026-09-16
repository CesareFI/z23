/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "signed_review_fixture.h"
#include "zcl_keys.h"
#ifdef ZCL_SIGNATURE_ORACLE
#include "signature_oracle.h"
#include "sighash_oracle.h"
#endif
#include <secp256k1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Review wire at %d\n", __LINE__); abort(); } } while (0)
static signed_review_fixture fixture;
static zcl_review_owner saved;
static zcl_transparent_tx parsed;
static uint8_t unsigned_wire[ZCL_TX_WIRE_MAX], stripped_wire[ZCL_TX_WIRE_MAX];
static size_t unsigned_length, signed_length;

static void setup(zcl_network network, size_t inputs, size_t outputs)
{
    CHECK(signed_review_fixture_init(&fixture, network, inputs, outputs));
    memcpy(&saved, &fixture.owner, sizeof(saved));
    memcpy(unsigned_wire, fixture.unsigned_wire, fixture.unsigned_length);
    unsigned_length = fixture.unsigned_length;
}

static void signature_from_script(const zcl_tx_input *input, zcl_signature *signature)
{
    CHECK(input->script_len >= 44 && input->script_len <= 107);
    const size_t first = input->script[0];
    CHECK(first >= 9 && first <= 72 && first < input->script_len);
    CHECK(input->script[first] == 1 && input->script[first + 1] == 33);
    CHECK(input->script_len - first - 2 == 33);
    memset(signature, 0, sizeof(*signature)); signature->der_len = first - 1;
    memcpy(signature->der, input->script + 1, signature->der_len);
    memcpy(signature->public_key, input->script + first + 2, 33);
}

static void verify_input(const uint8_t *wire, size_t length, size_t index)
{
    zcl_signature signature;
    signature_from_script(&parsed.inputs[index], &signature);
    CHECK(signature.der_len == fixture.signatures[index].der_len);
    CHECK(memcmp(signature.der, fixture.signatures[index].der, signature.der_len) == 0);
    CHECK(memcmp(signature.public_key, fixture.signatures[index].public_key, 33) == 0);
    secp256k1_pubkey key = {{0}}; secp256k1_ecdsa_signature decoded = {{0}};
    CHECK(secp256k1_ec_pubkey_parse(secp256k1_context_static, &key, signature.public_key, 33) == 1);
    CHECK(secp256k1_ecdsa_signature_parse_der(secp256k1_context_static, &decoded, signature.der, signature.der_len) == 1);
    CHECK(secp256k1_ecdsa_signature_normalize(secp256k1_context_static, NULL, &decoded) == 0);
    CHECK(secp256k1_ecdsa_verify(secp256k1_context_static, &decoded, fixture.digests[index], &key) == 1);
#ifdef ZCL_SIGNATURE_ORACLE
    uint8_t script[25] = {0x76,0xa9,0x14}, independent[32] = {0};
    memcpy(script + 3, fixture.hashes[index], 20); script[23] = 0x88; script[24] = 0xac;
    /* Hash the COMPLETED signed wire with the separately qualified reader and
     * libsodium, not the production parsed value or prepared digest. */
    zcl_test_sighash_all(wire, length, index, script, sizeof(script), 10000,
        UINT32_C(0x930b540d), independent, sizeof(independent));
    CHECK(memcmp(independent, fixture.digests[index], 32) == 0);
    CHECK(zcl_test_signature_script_oracle(&signature, independent, 32, fixture.hashes[index], 20) == 1);
#else
    (void)wire; (void)length;
#endif
}

static void check_wire(const uint8_t *wire, size_t length)
{
    CHECK(length > unsigned_length && length <= ZCL_TX_WIRE_MAX);
    CHECK(zcl_transaction_parse(wire, length, &parsed) == ZCL_OK);
    size_t expected = unsigned_length;
    for (size_t i = 0; i < parsed.input_count; ++i) {
        verify_input(wire, length, i);
        expected += parsed.inputs[i].script_len;
        parsed.inputs[i].script_len = 0; memset(parsed.inputs[i].script, 0, sizeof(parsed.inputs[i].script));
    }
    CHECK(length == expected);
    size_t stripped_length = 0;
    CHECK(zcl_transaction_serialize(&parsed, stripped_wire, sizeof(stripped_wire), &stripped_length) == ZCL_OK);
    CHECK(stripped_length == unsigned_length && memcmp(stripped_wire, unsigned_wire, unsigned_length) == 0);
    signed_length = length;
}

static void call(uint64_t id, uint64_t now, size_t count, size_t capacity, zcl_status status)
{
    struct { uint8_t before[8], wire[ZCL_TX_WIRE_MAX], after[8]; } box;
    memset(&box, 0xa5, sizeof(box)); size_t length = SIZE_MAX;
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, id, now, &fixture.block, fixture.signatures,
        count, box.wire, capacity, &length) == status);
    for (size_t i = 0; i < 8; ++i) CHECK(box.before[i] == 0xa5 && box.after[i] == 0xa5);
    if (status == ZCL_OK) check_wire(box.wire, length);
    else CHECK(length == SIZE_MAX);
    for (size_t i = status == ZCL_OK ? length : 0; i < sizeof(box.wire); ++i) CHECK(box.wire[i] == 0xa5);
}

static void destroy_sources(void)
{
    memset(&fixture.funding, 0xff, sizeof(fixture.funding));
    memset(&fixture.spending, 0xff, sizeof(fixture.spending));
    memset(fixture.funding_wire, 0xff, sizeof(fixture.funding_wire));
    memset(fixture.unsigned_wire, 0xff, sizeof(fixture.unsigned_wire));
}

static void profiles(void)
{
    for (size_t network = 0; network < 2; ++network) {
        for (size_t count = 1; count <= 8; ++count) {
            for (size_t outputs = 1; outputs <= 16; outputs += 15) {
                setup((zcl_network)network, count, outputs); destroy_sources();
                call(fixture.id, 100, count, ZCL_TX_WIRE_MAX, ZCL_OK);
                const size_t needed = signed_length;
                call(fixture.id, 100, count, needed - 1, ZCL_BUFFER_TOO_SMALL);
                call(fixture.id, 100, count, needed, ZCL_OK);
                call(fixture.id, 100, count, needed + 1, ZCL_OK);
                call(fixture.id, 100, count, SIZE_MAX, ZCL_OK);
                CHECK(memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
            }
        }
    }
    setup(ZCL_MAINNET, 1, 1); destroy_sources();
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_OK);
    const size_t needed = signed_length;
    for (size_t capacity = 0; capacity <= needed + 1; ++capacity)
        call(fixture.id, 100, 1, capacity, capacity < needed ? ZCL_BUFFER_TOO_SMALL : ZCL_OK);
}

static void rejected_signature(void)
{
    uint8_t wire[ZCL_TX_WIRE_MAX]; size_t length = SIZE_MAX;
    memset(wire, 0xa5, sizeof(wire));
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 100, &fixture.block, fixture.signatures,
        8, wire, sizeof(wire), &length) != ZCL_OK);
    CHECK(length == SIZE_MAX);
    for (size_t i = 0; i < sizeof(wire); ++i) CHECK(wire[i] == 0xa5);
    CHECK(memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
}

static void signatures(void)
{
    setup(ZCL_MAINNET, 8, 16);
    for (size_t i = 0; i < 8; ++i) {
        const zcl_signature original = fixture.signatures[i];
        fixture.signatures[i] = fixture.signatures[(i + 1) % 8]; rejected_signature();
        fixture.signatures[i] = original;
        fixture.signatures[i].der[0] ^= 1; rejected_signature(); fixture.signatures[i] = original;
        fixture.signatures[i].der_len = SIZE_MAX; rejected_signature(); fixture.signatures[i] = original;
        uint8_t secret[32] = {0}, digest[32]; secret[31] = (uint8_t)(i + 1);
        memcpy(digest, fixture.digests[i], 32); digest[0] ^= 1;
        CHECK(zcl_signature_create(secret, 32, digest, 32, &fixture.signatures[i]) == ZCL_OK);
        zcl_secure_zero(secret, sizeof(secret)); rejected_signature(); fixture.signatures[i] = original;
    }
    call(fixture.id, 100, 8, ZCL_TX_WIRE_MAX, ZCL_OK);
}

static void context_and_lifetime(void)
{
    setup(ZCL_MAINNET, 1, 1);
    fixture.block.network = ZCL_TESTNET; call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_UNSUPPORTED);
    fixture.block.network = ZCL_MAINNET;
    fixture.block.height = 476969; call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_CRYPTO_FAILURE);
    fixture.block.height = 1000011; call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    fixture.block.height = 1000000; fixture.block.lock_time_cutoff = UINT64_MAX;
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE); fixture.block.lock_time_cutoff = 0;
    CHECK(memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
    call(fixture.id + 1, UINT64_MAX, 1, ZCL_TX_WIRE_MAX, ZCL_CANCELLED);
    CHECK(memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
    call(fixture.id, 101, 1, ZCL_TX_WIRE_MAX, ZCL_OK);
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_CANCELLED);
    CHECK(fixture.owner.data.id == 0 && fixture.owner.issued == 1);
    setup(ZCL_MAINNET, 1, 1);
    call(fixture.id, 100 + ZCL_REVIEW_LIFETIME_MS, 1, ZCL_TX_WIRE_MAX, ZCL_TIMED_OUT);
    CHECK(fixture.owner.data.id == 0 && fixture.owner.issued == 1);
    setup(ZCL_MAINNET, 1, 1);
    CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_CANCELLED);
}

static void replacement_and_p2sh(void)
{
    setup(ZCL_MAINNET, 1, 1);
    const uint64_t old = fixture.id;
    CHECK(zcl_review_cancel(&fixture.owner, old) == ZCL_OK);
    zcl_previous_transaction previous = {fixture.funding_wire, fixture.funding_length};
    CHECK(zcl_review_open(&fixture.owner, fixture.unsigned_wire, fixture.unsigned_length, ZCL_MAINNET,
        &previous, 1, 500, 100, &fixture.id) == ZCL_OK);
    memcpy(&saved, &fixture.owner, sizeof(saved));
    call(old, UINT64_MAX, 1, ZCL_TX_WIRE_MAX, ZCL_CANCELLED);
    CHECK(memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_OK);
    CHECK(zcl_review_cancel(&fixture.owner, fixture.id) == ZCL_OK);
    zcl_address destination = {ZCL_MAINNET, ZCL_P2SH, {0}};
    memcpy(destination.hash, fixture.hashes[0], 20);
    zcl_tx_output *output = &fixture.funding.outputs[0];
    CHECK(zcl_address_script(&destination, output->script, sizeof(output->script), &output->script_len) == ZCL_OK);
    CHECK(zcl_transaction_serialize(&fixture.funding, fixture.funding_wire, sizeof(fixture.funding_wire), &fixture.funding_length) == ZCL_OK);
    CHECK(zcl_transaction_id(&fixture.funding, fixture.spending.inputs[0].previous_txid, 32) == ZCL_OK);
    CHECK(zcl_transaction_serialize(&fixture.spending, fixture.unsigned_wire, sizeof(fixture.unsigned_wire), &fixture.unsigned_length) == ZCL_OK);
    previous.length = fixture.funding_length;
    CHECK(zcl_review_open(&fixture.owner, fixture.unsigned_wire, fixture.unsigned_length, ZCL_MAINNET,
        &previous, 1, 500, 100, &fixture.id) == ZCL_OK);
    call(fixture.id, 100, 1, ZCL_TX_WIRE_MAX, ZCL_UNSUPPORTED);
}

static void arguments(void)
{
    setup(ZCL_MAINNET, 1, 1);
    uint8_t wire[ZCL_TX_WIRE_MAX]; size_t length = SIZE_MAX;
    memset(wire, 0xa5, sizeof(wire));
    CHECK(zcl_review_p2pkh_wire(NULL, fixture.id, 101, &fixture.block, fixture.signatures, 1, wire, sizeof(wire), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 101, NULL, fixture.signatures, 1, wire, sizeof(wire), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 101, &fixture.block, NULL, 1, wire, sizeof(wire), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 101, &fixture.block, fixture.signatures, 1, NULL, sizeof(wire), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_review_p2pkh_wire(&fixture.owner, fixture.id, 101, &fixture.block, fixture.signatures, 1, wire, sizeof(wire), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(length == SIZE_MAX && memcmp(&saved, &fixture.owner, sizeof(saved)) == 0);
    for (size_t i = 0; i < sizeof(wire); ++i) CHECK(wire[i] == 0xa5);
    call(fixture.id, 100, 0, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    call(fixture.id, 100, 2, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    call(fixture.id, 100, 9, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    call(fixture.id, 100, SIZE_MAX, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
}

int main(void)
{
    profiles(); signatures(); context_and_lifetime(); replacement_and_p2sh(); arguments();
    CHECK(puts("Signed wire verifies every owned review input, preserves exact fields, and refuses stale or altered signatures") >= 0);
    return 0;
}
