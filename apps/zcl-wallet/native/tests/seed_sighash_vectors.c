/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Public fixture-only adaptation of pinned original SignatureHash serialization.
 * Original Bitcoin/Zcash/Zclassic source is MIT; see
 * ../../docs/transaction-reference.LICENSE. No wallet or app linkage.
 * Only the bounded shapes in the original v4 vectors are parsed here. */
#include <sodium.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sighash_oracle.h"

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Original hash oracle failed at %d (row %zu)\n", __LINE__, row_number); abort(); } } while (0)
#define ORACLE_WIRE_MAX ((size_t)11000)
#ifdef ZCL_SIGHASH_ORACLE_LIBRARY
static const size_t row_number = 0;
#else
static size_t row_number;
static char line[32768];
static uint8_t wire[ORACLE_WIRE_MAX];
static struct {
    uint8_t wire[1925], script[128];
    size_t wire_length, script_length;
} projections[3];
static unsigned projection_mask;
#endif
typedef struct { const uint8_t *data; size_t length; } span;
typedef struct { const uint8_t *data; size_t length, position; } cursor;
typedef struct {
    span header, lock_expiry, value_balance, join_key;
    span outpoints[8], sequences[8], outputs[16], spends[4], shielded[4], joins[3];
    size_t inputs, output_count, spend_count, shielded_count, join_count, prefix_length;
} transaction_view;

static const uint8_t *take(cursor *reader, size_t length)
{
    CHECK(reader->position <= reader->length && length <= reader->length - reader->position);
    const uint8_t *result = reader->data + reader->position;
    reader->position += length;
    return result;
}

static size_t count(cursor *reader, size_t maximum)
{
    const size_t result = *take(reader, 1);
    CHECK(result <= maximum && result < 253);
    return result;
}

static span read_span(cursor *reader, size_t length)
{
    return (span){take(reader, length), length};
}

static void parse_transparent(cursor *reader, transaction_view *view)
{
    view->header = read_span(reader, 8);
    const uint8_t header[8] = {4, 0, 0, 0x80, 0x85, 0x20, 0x2f, 0x89};
    CHECK(memcmp(view->header.data, header, sizeof(header)) == 0);
    view->inputs = count(reader, 8);
    for (size_t i = 0; i < view->inputs; ++i) {
        view->outpoints[i] = read_span(reader, 36);
        const size_t script_length = count(reader, 128);
        (void)take(reader, script_length);
        view->sequences[i] = read_span(reader, 4);
    }
    view->output_count = count(reader, 16);
    for (size_t i = 0; i < view->output_count; ++i) {
        const size_t begin = reader->position;
        (void)take(reader, 8);
        const size_t script_length = count(reader, 128);
        (void)take(reader, script_length);
        view->outputs[i] = (span){reader->data + begin, reader->position - begin};
    }
    view->lock_expiry = read_span(reader, 8);
    view->prefix_length = reader->position;
}

static void parse_shielded(cursor *reader, transaction_view *view)
{
    view->value_balance = read_span(reader, 8);
    view->spend_count = count(reader, 4);
    for (size_t i = 0; i < view->spend_count; ++i) {
        view->spends[i] = read_span(reader, 320); /* Excludes spendAuthSig. */
        (void)take(reader, 64);
    }
    view->shielded_count = count(reader, 4);
    for (size_t i = 0; i < view->shielded_count; ++i)
        view->shielded[i] = read_span(reader, 948);
    view->join_count = count(reader, 3);
    for (size_t i = 0; i < view->join_count; ++i)
        view->joins[i] = read_span(reader, 1698); /* Groth JoinSplit wire bytes. */
    if (view->join_count != 0) {
        view->join_key = read_span(reader, 32);
        (void)take(reader, 64); /* JoinSplit signature is excluded. */
    }
    if (view->spend_count != 0 || view->shielded_count != 0)
        (void)take(reader, 64); /* Binding signature is excluded. */
    CHECK(reader->position == reader->length);
}

static void start(crypto_generichash_blake2b_state *state, const uint8_t *personal)
{
    CHECK(personal != NULL);
    uint8_t salt[16] = {0};
    CHECK(crypto_generichash_blake2b_init_salt_personal(state, NULL, 0, 32, salt, personal) == 0);
}

static void append(crypto_generichash_blake2b_state *state, span bytes)
{
    CHECK(bytes.data != NULL && bytes.length <= ORACLE_WIRE_MAX);
    CHECK(crypto_generichash_blake2b_update(state, bytes.data, (unsigned long long)bytes.length) == 0);
}

static void finish(crypto_generichash_blake2b_state *state, uint8_t output[32])
{
    CHECK(crypto_generichash_blake2b_final(state, output, 32) == 0);
    sodium_memzero(state, sizeof(*state));
}

static void component(const char personal[16], const span *items, size_t length,
                       const span *extra, uint8_t output[32])
{
    CHECK(length <= 16);
    crypto_generichash_blake2b_state state;
    start(&state, (const uint8_t *)personal);
    for (size_t i = 0; i < length; ++i) append(&state, items[i]);
    if (extra != NULL) append(&state, *extra);
    finish(&state, output);
}

static void component_hashes(const transaction_view *view, size_t input_index,
                            uint32_t type, uint8_t hashes[6][32])
{
    const uint32_t base = type & 31U;
    if ((type & 0x80U) == 0) {
        component("ZcashPrevoutHash", view->outpoints, view->inputs, NULL, hashes[0]);
        if (base != 2 && base != 3)
            component("ZcashSequencHash", view->sequences, view->inputs, NULL, hashes[1]);
    }
    if (base != 2 && base != 3)
        component("ZcashOutputsHash", view->outputs, view->output_count, NULL, hashes[2]);
    else if (base == 3 && input_index < view->output_count)
        component("ZcashOutputsHash", view->outputs + input_index, 1, NULL, hashes[2]);
    if (view->join_count != 0)
        component("ZcashJSplitsHash", view->joins, view->join_count, &view->join_key, hashes[3]);
    if (view->spend_count != 0)
        component("ZcashSSpendsHash", view->spends, view->spend_count, NULL, hashes[4]);
    if (view->shielded_count != 0)
        component("ZcashSOutputHash", view->shielded, view->shielded_count, NULL, hashes[5]);
}

static void little(uint8_t *output, uint64_t value, size_t length)
{
    CHECK(length == 4 || length == 8);
    for (size_t i = 0; i < length; ++i) output[i] = (uint8_t)(value >> (8 * i));
}

static void signature_hash(const transaction_view *view, size_t input_index, uint32_t type,
                           uint32_t branch, span script, uint64_t amount, uint8_t output[32])
{
    CHECK(input_index < view->inputs && script.length < 253);
    uint8_t hashes[6][32] = {{0}}, personal[16] = {0}, type_bytes[4], amount_bytes[8];
    const uint8_t script_length = (uint8_t)script.length;
    component_hashes(view, input_index, type, hashes);
    memcpy(personal, "ZcashSigHash", 12);
    little(personal + 12, branch, 4);
    little(type_bytes, type, 4);
    little(amount_bytes, amount, 8);
    crypto_generichash_blake2b_state state;
    start(&state, personal);
    append(&state, view->header);
    append(&state, (span){(const uint8_t *)hashes, sizeof(hashes)});
    append(&state, view->lock_expiry);
    append(&state, view->value_balance);
    append(&state, (span){type_bytes, sizeof(type_bytes)});
    append(&state, view->outpoints[input_index]);
    append(&state, (span){&script_length, 1});
    append(&state, script);
    append(&state, (span){amount_bytes, sizeof(amount_bytes)});
    append(&state, view->sequences[input_index]);
    finish(&state, output);
}

void zcl_test_sighash_all(const uint8_t *bytes, size_t wire_length, size_t input_index,
    const uint8_t *script, size_t script_length, uint64_t amount, uint32_t branch,
    uint8_t *digest, size_t capacity)
{
    CHECK(bytes != NULL && script != NULL && digest != NULL);
    CHECK(wire_length <= ORACLE_WIRE_MAX && script_length <= 128 && capacity >= 32);
    CHECK(amount <= UINT64_C(2100000000000000));
    CHECK(sodium_init() >= 0);
    transaction_view view = {0};
    cursor reader = {bytes, wire_length, 0};
    parse_transparent(&reader, &view);
    parse_shielded(&reader, &view);
    signature_hash(&view, input_index, 1, branch, (span){script, script_length}, amount, digest);
}

#ifndef ZCL_SIGHASH_ORACLE_LIBRARY
static uint8_t digit(char value)
{
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    CHECK(value >= 'a' && value <= 'f');
    return (uint8_t)(value - 'a' + 10);
}

static size_t unhex(const char *text, uint8_t *output, size_t capacity)
{
    const size_t length = strlen(text);
    CHECK(length % 2 == 0 && length / 2 <= capacity);
    for (size_t i = 0; i < length / 2; ++i)
        output[i] = (uint8_t)((unsigned)digit(text[2 * i]) * 16U + digit(text[2 * i + 1]));
    return length / 2;
}

static int64_t number(const char *text, int64_t minimum, int64_t maximum)
{
    errno = 0;
    char *end = NULL;
    const intmax_t result = strtoimax(text, &end, 10);
    CHECK(errno == 0 && end != text && *end == '\0');
    CHECK(result >= minimum && result <= maximum);
    return (int64_t)result;
}

static void capture_projection(const transaction_view *view, span script)
{
    const size_t rows[3] = {203, 208, 296};
    for (size_t i = 0; i < 3; ++i) {
        if (row_number != rows[i]) continue;
        CHECK(view->prefix_length <= sizeof(projections[i].wire) - 11);
        CHECK(script.length <= sizeof(projections[i].script));
        CHECK(memcmp(view->lock_expiry.data + 4, "\0\0\0\0", 4) == 0);
        CHECK((projection_mask & (1U << i)) == 0);
        memcpy(projections[i].wire, wire, view->prefix_length);
        memset(projections[i].wire + view->prefix_length, 0, 11);
        memcpy(projections[i].script, script.data, script.length);
        projections[i].wire_length = view->prefix_length + 11;
        projections[i].script_length = script.length;
        projection_mask |= 1U << i;
    }
}

static void split_fields(char *text, char **fields, size_t field_count)
{
    CHECK(field_count == 7 || field_count == 8);
    fields[0] = text;
    for (size_t i = 1; i < field_count; ++i) {
        char *separator = strchr(fields[i - 1], '\t');
        CHECK(separator != NULL);
        *separator = '\0'; fields[i] = separator + 1;
    }
    CHECK(strchr(fields[field_count - 1], '\t') == NULL);
}

static void check_row(char *text)
{
    char *fields[7] = {0};
    split_fields(text, fields, 7);
    const size_t original_row = (size_t)number(fields[0], 1, 503);
    CHECK(original_row > row_number);
    row_number = original_row;
    const size_t input_index = (size_t)number(fields[1], 0, 7);
    const uint32_t type = (uint32_t)number(fields[2], INT32_MIN, INT32_MAX);
    const uint32_t branch = (uint32_t)number(fields[3], 0, UINT32_MAX);
    const size_t wire_length = unhex(fields[4], wire, sizeof(wire));
    uint8_t script[128] = {0}, expected[32] = {0}, digest[32] = {0};
    const size_t script_length = unhex(fields[5], script, sizeof(script));
    CHECK(unhex(fields[6], expected, sizeof(expected)) == sizeof(expected));
    transaction_view view = {0};
    cursor reader = {wire, wire_length, 0};
    parse_transparent(&reader, &view);
    parse_shielded(&reader, &view);
    signature_hash(&view, input_index, type, branch, (span){script, script_length}, 0, digest);
    for (size_t i = 0; i < 32; ++i) CHECK(digest[i] == expected[31 - i]);
    capture_projection(&view, (span){script, script_length});
}

static void check_zip_row(char *text)
{
    char *fields[8] = {0};
    split_fields(text, fields, 8);
    const size_t next_row = (size_t)number(fields[0], 1, 14);
    CHECK(next_row > row_number);
    row_number = next_row;
    const size_t input_index = (size_t)number(fields[1], 0, 7);
    const uint32_t type = (uint32_t)number(fields[2], 1, 0x83);
    const uint64_t amount = (uint64_t)number(fields[3], 1, INT64_C(2100000000000000));
    const uint32_t branch = (uint32_t)number(fields[4], 0, UINT32_MAX);
    const size_t length = unhex(fields[5], wire, sizeof(wire));
    uint8_t script[128] = {0}, expected[32] = {0}, digest[32] = {0};
    const size_t script_length = unhex(fields[6], script, sizeof(script));
    CHECK(unhex(fields[7], expected, sizeof(expected)) == sizeof(expected));
    transaction_view view = {0};
    cursor reader = {wire, length, 0};
    parse_transparent(&reader, &view);
    parse_shielded(&reader, &view);
    signature_hash(&view, input_index, type, branch, (span){script, script_length}, amount, digest);
    CHECK(memcmp(digest, expected, sizeof(digest)) == 0); /* ZIP results are raw digest bytes. */
}

static void emit_bytes(const uint8_t *bytes, size_t length)
{
    CHECK(length <= 32 && printf("{") >= 0);
    for (size_t i = 0; i < length; ++i)
        CHECK(printf("%s0x%02x", i == 0 ? "" : ",", (unsigned)bytes[i]) >= 0);
    CHECK(printf("}") >= 0);
}

static void emit_profile(const transaction_view *view, size_t projection, span script, unsigned profile)
{
    const uint32_t branches[4] = {0, UINT32_C(0x5ba81b19), UINT32_C(0x76b809bb), UINT32_MAX};
    const uint64_t amounts[3] = {0, 1, UINT64_C(2100000000000000)};
    for (size_t input_index = 0; input_index < view->inputs; ++input_index)
        for (size_t branch = 0; branch < 4; ++branch)
            for (size_t amount = 0; amount < 3; ++amount) {
                uint8_t digest[32];
                signature_hash(view, input_index, 1, branches[branch], script, amounts[amount], digest);
                CHECK(printf("    {%zu, %zu, %u, UINT32_C(0x%08" PRIx32 "), UINT64_C(%" PRIu64 "), ",
                    projection, input_index, profile, branches[branch], amounts[amount]) >= 0);
                emit_bytes(digest, sizeof(digest));
                CHECK(printf("},\n") >= 0);
            }
}

static void emit_vectors(void)
{
    CHECK(projection_mask == 7);
    CHECK(printf("/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.\n"
        " * Public projected v4 SIGHASH_ALL fixtures, not original transactions.\n"
        " * Original 14a83d510ffd109d3fa09bf74ebf8c28854a263f rows 203/208/296.\n"
        " * Generated only after matching all 130 untouched original v4 hashes.\n"
        " * Also matches both published ZIP 243 transparent-input hashes with nonzero amounts.\n"
        " * Uses libsodium %s; no wallet C parser, serializer or hash provider.\n"
        " * Branches are explicit test values, not a current-network assertion.\n"
        " * Script profile 0: original row script; profile 1: P2PKH hash bytes 0..19.\n"
        " * Digests are raw bytes, opposite original uint256 display order. */\n",
        sodium_version_string()) >= 0);
    CHECK(printf("static const struct { size_t length; uint8_t bytes[128]; } sighash_scripts[] = {\n") >= 0);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(printf("    {%zu, ", projections[i].script_length) >= 0);
        CHECK(projections[i].script_length > 0);
        emit_bytes(projections[i].script, projections[i].script_length);
        CHECK(printf("},\n") >= 0);
    }
    CHECK(printf("};\nstatic const struct { size_t projection, input_index; unsigned script_profile;\n"
        "    uint32_t branch; uint64_t amount; uint8_t digest[32]; } sighash_vectors[] = {\n") >= 0);
    uint8_t p2pkh[25] = {0x76, 0xa9, 0x14};
    for (size_t i = 0; i < 20; ++i) p2pkh[i + 3] = (uint8_t)i;
    p2pkh[23] = 0x88; p2pkh[24] = 0xac;
    for (size_t i = 0; i < 3; ++i) {
        transaction_view view = {0};
        cursor reader = {projections[i].wire, projections[i].wire_length, 0};
        parse_transparent(&reader, &view);
        parse_shielded(&reader, &view);
        emit_profile(&view, i, (span){projections[i].script, projections[i].script_length}, 0);
        emit_profile(&view, i, (span){p2pkh, sizeof(p2pkh)}, 1);
    }
    CHECK(printf("};\n") >= 0 && fflush(stdout) == 0);
}

static void check_file(const char *path, size_t expected_rows, void (*check)(char *))
{
    CHECK(path != NULL && check != NULL && (expected_rows == 130 || expected_rows == 2));
    row_number = 0;
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    size_t rows = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        const size_t length = strlen(line);
        CHECK(length > 0 && line[length - 1] == '\n' && rows < expected_rows);
        line[length - 1] = '\0';
        check(line); ++rows;
    }
    CHECK(ferror(input) == 0);
    CHECK(fclose(input) == 0 && rows == expected_rows);
}

int main(int argc, char **argv)
{
    CHECK(argc == 3 && sodium_init() >= 0);
    check_file(argv[1], 130, check_row);
    CHECK(fputs("130 untouched original v4 signature-hash vectors matched with libsodium\n", stderr) >= 0);
    check_file(argv[2], 2, check_zip_row);
    CHECK(fputs("Both published ZIP 243 transparent-input hashes matched, including nonzero 64-bit amounts\n", stderr) >= 0);
    emit_vectors();
    return 0;
}
#endif
