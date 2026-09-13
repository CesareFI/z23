/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Prevout check failed at %d\n", __LINE__); abort(); } } while (0)

static size_t decode(const char *hex, uint8_t *bytes, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    const size_t size = strlen(hex);
    CHECK(size % 2 == 0 && size / 2 <= capacity);
    for (size_t i = 0; i < size / 2; ++i) {
        const char *a = strchr(digits, hex[2 * i]), *b = strchr(digits, hex[2 * i + 1]);
        CHECK(a != NULL && b != NULL);
        bytes[i] = (uint8_t)((a - digits) * 16 + (b - digits));
    }
    return size / 2;
}

static void script_refused(const uint8_t *script, size_t size, zcl_network network, zcl_status expected)
{
    struct { uint64_t before; zcl_address address; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_address old;
    memcpy(&old, &box.address, sizeof(old));
    CHECK(zcl_address_from_script(script, size, network, &box.address) == expected);
    CHECK(memcmp(&old, &box.address, sizeof(old)) == 0);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
}

static void script_templates(void)
{
    static const char *scripts[] = {
        "76a914000102030405060708090a0b0c0d0e0f1011121388ac",
        "a914000102030405060708090a0b0c0d0e0f1011121387"
    };
    for (size_t kind = 0; kind < 2; ++kind) {
        uint8_t script[27] = {0}, encoded[25];
        const size_t length = decode(scripts[kind], script, sizeof(script));
        const size_t offset = kind == 0 ? 3 : 2;
        for (int chain = 0; chain < 2; ++chain) {
            zcl_address address;
            CHECK(zcl_address_from_script(script, length, (zcl_network)chain, &address) == ZCL_OK);
            CHECK(address.network == (zcl_network)chain && address.kind == (kind == 0 ? ZCL_P2PKH : ZCL_P2SH));
            for (size_t i = 0; i < 20; ++i) CHECK(address.hash[i] == i);
            size_t written = 0;
            CHECK(zcl_address_script(&address, encoded, sizeof(encoded), &written) == ZCL_OK);
            CHECK(written == length && memcmp(script, encoded, length) == 0);
            for (size_t size = 0; size < sizeof(script); ++size) {
                if (size != length) script_refused(script, size, (zcl_network)chain, ZCL_UNSUPPORTED);
            }
            /* Every non-hash opcode byte must match exactly, including push20.
             * Hash bytes accept all values without treating them as opcodes. */
            for (size_t i = 0; i < length; ++i) {
                const uint8_t original = script[i];
                for (unsigned value = 0; value <= 255; ++value) {
                    script[i] = (uint8_t)value;
                    if (i >= offset && i < offset + 20) {
                        CHECK(zcl_address_from_script(script, length, (zcl_network)chain, &address) == ZCL_OK);
                        CHECK(address.hash[i - offset] == value);
                    } else if (value != original) script_refused(script, length, (zcl_network)chain, ZCL_UNSUPPORTED);
                }
                script[i] = original;
            }
        }
        script_refused(script, length, (zcl_network)2, ZCL_UNSUPPORTED);
        script_refused(script, SIZE_MAX, ZCL_MAINNET, ZCL_UNSUPPORTED);
    }
    const uint8_t empty = 0;
    script_refused(NULL, 0, ZCL_MAINNET, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_address_from_script(&empty, 0, ZCL_MAINNET, NULL) == ZCL_INVALID_ARGUMENT);
    /* Equivalent pushdata encodings and an appended opcode are not templates. */
    uint8_t alternate[27];
    size_t size = decode("76a94c14000102030405060708090a0b0c0d0e0f1011121388ac", alternate, sizeof(alternate));
    script_refused(alternate, size, ZCL_MAINNET, ZCL_UNSUPPORTED);
}

static void prevout_refused(const zcl_tx_input *input, const uint8_t *wire, size_t size, zcl_status expected)
{
    struct { uint64_t before; zcl_tx_output output; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_tx_output old;
    memcpy(&old, &box.output, sizeof(old));
    const zcl_status status = zcl_transaction_prevout(input, wire, size, &box.output);
    CHECK(status != ZCL_OK && (expected == ZCL_OK || status == expected));
    CHECK(memcmp(&old, &box.output, sizeof(old)) == 0);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
}

static void independent_previous_outputs(void)
{
    uint8_t wire[256];
    const size_t size = decode(transaction_vectors[0].hex, wire, sizeof(wire));
    zcl_tx_input input = {0};
    CHECK(decode(transaction_vectors[0].txid, input.previous_txid, sizeof(input.previous_txid)) == 32);
    zcl_tx_output output;
    CHECK(zcl_transaction_prevout(&input, wire, size, &output) == ZCL_OK);
    CHECK(output.value == UINT64_C(80581513) && output.script_len == 6);
    const uint8_t script0[] = {0x53, 0x52, 0x00, 0x53, 0x00, 0x6a};
    CHECK(memcmp(output.script, script0, sizeof(script0)) == 0);
    for (size_t i = sizeof(script0); i < sizeof(output.script); ++i) CHECK(output.script[i] == 0);
    input.previous_index = 1;
    CHECK(zcl_transaction_prevout(&input, wire, size, &output) == ZCL_OK);
    CHECK(output.value == UINT64_C(83547780) && output.script_len == 5);
    const uint8_t script1[] = {0x63, 0x53, 0x6a, 0x6a, 0x53};
    CHECK(memcmp(output.script, script1, sizeof(script1)) == 0);
    /* Hash matching does not silently classify an arbitrary script as an address. */
    script_refused(output.script, output.script_len, ZCL_MAINNET, ZCL_UNSUPPORTED);
    input.previous_index = 2; prevout_refused(&input, wire, size, ZCL_OUT_OF_RANGE);
    input.previous_index = UINT32_MAX; prevout_refused(&input, wire, size, ZCL_OUT_OF_RANGE);
    input.previous_index = 0;
    for (size_t cut = 0; cut < size; ++cut) prevout_refused(&input, wire, cut, ZCL_OK);
    for (size_t i = 0; i < size; ++i) {
        wire[i] ^= 1;
        prevout_refused(&input, wire, size, ZCL_OK);
        wire[i] ^= 1;
    }
    for (size_t i = 0; i < sizeof(input.previous_txid); ++i) {
        input.previous_txid[i] ^= 1;
        prevout_refused(&input, wire, size, ZCL_INVALID_ENCODING);
        input.previous_txid[i] ^= 1;
    }
    prevout_refused(NULL, wire, size, ZCL_INVALID_ARGUMENT);
    prevout_refused(&input, NULL, 0, ZCL_INVALID_ARGUMENT);
    prevout_refused(&input, wire, SIZE_MAX, ZCL_RESOURCE_EXHAUSTED);
    CHECK(zcl_transaction_prevout(&input, wire, size, NULL) == ZCL_INVALID_ARGUMENT);
}

static void synthetic_standard_outputs(void)
{
    zcl_transparent_tx previous = {0};
    previous.input_count = 1;
    previous.output_count = 2;
    previous.inputs[0].previous_index = 1;
    previous.inputs[0].sequence = UINT32_MAX;
    previous.outputs[0].value = 10000;
    previous.outputs[1].value = 5000;
    zcl_address destination = {ZCL_TESTNET, ZCL_P2PKH, {0}};
    memset(destination.hash, 0xff, sizeof(destination.hash));
    for (size_t i = 0; i < 2; ++i) {
        destination.kind = i == 0 ? ZCL_P2PKH : ZCL_P2SH;
        CHECK(zcl_address_script(&destination, previous.outputs[i].script,
            sizeof(previous.outputs[i].script), &previous.outputs[i].script_len) == ZCL_OK);
    }
    uint8_t wire[ZCL_TX_WIRE_MAX];
    size_t size = 0;
    CHECK(zcl_transaction_serialize(&previous, wire, sizeof(wire), &size) == ZCL_OK);
    zcl_tx_input input = {0};
    CHECK(zcl_transaction_id(&previous, input.previous_txid, sizeof(input.previous_txid)) == ZCL_OK);
    for (uint32_t i = 0; i < 2; ++i) {
        input.previous_index = i;
        zcl_tx_output output;
        CHECK(zcl_transaction_prevout(&input, wire, size, &output) == ZCL_OK);
        CHECK(output.value == previous.outputs[i].value);
        zcl_address extracted;
        CHECK(zcl_address_from_script(output.script, output.script_len, ZCL_TESTNET, &extracted) == ZCL_OK);
        CHECK(extracted.network == ZCL_TESTNET && extracted.kind == (i == 0 ? ZCL_P2PKH : ZCL_P2SH));
        CHECK(memcmp(extracted.hash, destination.hash, sizeof(extracted.hash)) == 0);
    }
}

int main(void)
{
    script_templates(); independent_previous_outputs(); synthetic_standard_outputs();
    puts("Script templates and hash-matched previous outputs passed");
    return 0;
}
