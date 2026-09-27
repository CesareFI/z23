/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_tx_prevout.h"
#include "zcl_tx_script_facts.h"
#include "zcl_zip243_host.h"
#include "crypto/blake2b.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
    abort(); \
} } while (0)

typedef struct { uint8_t bytes[256]; size_t length; } transaction;

static void put_u32(transaction *tx, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        tx->bytes[tx->length++] = (uint8_t)(value >> (8 * i));
}

static void put_u64(transaction *tx, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        tx->bytes[tx->length++] = (uint8_t)(value >> (8 * i));
}

static void put_bytes(transaction *tx, const uint8_t *bytes, size_t length) {
    CHECK(tx->length + length <= sizeof tx->bytes);
    memcpy(tx->bytes + tx->length, bytes, length);
    tx->length += length;
}

static void put_header(transaction *tx) {
    put_u32(tx, 0x80000004);
    put_u32(tx, 0x892f2085);
}

static void put_tail(transaction *tx) {
    put_u32(tx, 0);
    put_u32(tx, 0);
    put_u64(tx, 0);
    tx->bytes[tx->length++] = 0;
    tx->bytes[tx->length++] = 0;
    tx->bytes[tx->length++] = 0;
}

static void put_output(transaction *tx, uint64_t value, uint8_t type,
                       uint8_t fill) {
    put_u64(tx, value);
    tx->bytes[tx->length++] = type == 0 ? 25 : 23;
    tx->bytes[tx->length++] = type == 0 ? 0x76 : 0xa9;
    if (type == 0) tx->bytes[tx->length++] = 0xa9;
    tx->bytes[tx->length++] = 0x14;
    for (unsigned i = 0; i < 20; ++i) tx->bytes[tx->length++] = fill;
    if (type == 0) tx->bytes[tx->length++] = 0x88;
    tx->bytes[tx->length++] = type == 0 ? 0xac : 0x87;
}

static transaction previous_transaction(void) {
    transaction tx = {0};
    put_header(&tx);
    tx.bytes[tx.length++] = 1;
    uint8_t coinbase[36] = {0};
    memset(coinbase + 32, 0xff, 4);
    put_bytes(&tx, coinbase, sizeof coinbase);
    tx.bytes[tx.length++] = 2;
    tx.bytes[tx.length++] = 1;
    tx.bytes[tx.length++] = 1;
    put_u32(&tx, UINT32_MAX);
    tx.bytes[tx.length++] = 1;
    put_output(&tx, 50000000, 0, 0x11);
    put_tail(&tx);
    return tx;
}

static bool sha256_bytes(const uint8_t *bytes, size_t length,
                         uint8_t digest[32]) {
    return SHA256(bytes, length, digest) != NULL;
}

static transaction spending_transaction(const uint8_t txid[32],
                                        unsigned input_count) {
    transaction tx = {0};
    put_header(&tx);
    tx.bytes[tx.length++] = (uint8_t)input_count;
    for (unsigned i = 0; i < input_count; ++i) {
        put_bytes(&tx, txid, 32);
        put_u32(&tx, 0);
        tx.bytes[tx.length++] = 0;
        put_u32(&tx, UINT32_MAX - 1);
    }
    tx.bytes[tx.length++] = 2;
    put_output(&tx, 30000000, 0, 0x22);
    put_output(&tx, 19000000, 1, 0x33);
    put_tail(&tx);
    return tx;
}

static bool capture_input(void *context, const zcl_tx_input *input) {
    unsigned *count = context;
    CHECK(input->index == (*count)++);
    CHECK(input->previous_output_index == 0);
    CHECK(input->script_length == 0);
    CHECK(input->sequence == UINT32_MAX - 1);
    return true;
}

static bool first_output(void *context, const zcl_tx_output *output) {
    if (!output->index) *(uint8_t **)context = (uint8_t *)output->script;
    return true;
}

static unsigned hex_digit(char value) {
    if (value >= '0' && value <= '9') return (unsigned)(value - '0');
    if (value >= 'a' && value <= 'f') return (unsigned)(value - 'a' + 10);
    abort();
}

static void test_published_txid(const char *path) {
    FILE *file = fopen(path, "r");
    CHECK(file);
    char line[512];
    do { CHECK(fgets(line, sizeof line, file)); } while (line[0] == '#');
    CHECK(fclose(file) == 0);
    CHECK(strcspn(line, "\r\n") == 490);
    uint8_t wire[245];
    for (size_t i = 0; i < sizeof wire; ++i)
        wire[i] = (uint8_t)((hex_digit(line[2 * i]) << 4) |
                             hex_digit(line[2 * i + 1]));
    uint8_t first[32], raw_txid[32], published[32];
    CHECK(sha256_bytes(wire, sizeof wire, first));
    CHECK(sha256_bytes(first, sizeof first, raw_txid));
    static const char display[] =
        "97d8814886d07fc12bbac90c089a10f90906cbb53402ee26e576ef99276c492d";
    for (size_t i = 0; i < 32; ++i)
        published[31 - i] = (uint8_t)((hex_digit(display[2 * i]) << 4) |
                                       hex_digit(display[2 * i + 1]));
    CHECK(memcmp(raw_txid, published, sizeof raw_txid) == 0);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    test_published_txid(argv[1]);
    transaction prev = previous_transaction();
    uint8_t first[32], txid[32];
    CHECK(sha256_bytes(prev.bytes, prev.length, first));
    CHECK(sha256_bytes(first, sizeof first, txid));
    transaction spend = spending_transaction(txid, 1);
    zcl_tx_previous_transaction source = {prev.bytes, prev.length};
    zcl_tx_transparent_facts facts = {.fee_zat = UINT64_MAX};
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
                                       &source, 1, sha256_bytes, &facts) == 0);
    CHECK(facts.transparent_inputs == 1 && facts.transparent_outputs == 2);
    CHECK(facts.input_zat == 50000000 && facts.output_zat == 49000000);
    CHECK(facts.fee_zat == 1000000);
    unsigned count = 0;
    CHECK(zcl_tx_inputs_visit(spend.bytes, spend.length,
                              capture_input, &count) == 0 && count == 1);

    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    uint8_t direct[32], bound[32];
    CHECK(zcl_zip243_transparent_digest(spend.bytes, spend.length, 0,
        prev.bytes + 8 + 1 + 36 + 1 + 2 + 4 + 1 + 8 + 1, 25,
        50000000, 0x76b809bb, &hasher, direct) == 0);
    CHECK(zcl_tx_hash_bound_digest(spend.bytes, spend.length, 0, source,
        0x76b809bb, sha256_bytes, &hasher, bound) == 0);
    CHECK(memcmp(direct, bound, 32) == 0);
    zcl_tx_transparent_facts bound_facts = {.fee_zat = UINT64_MAX};
    uint8_t all_digests[2][32];
    memset(all_digests, 0x5a, sizeof all_digests);
    CHECK(zcl_tx_transparent_bound_digests(spend.bytes, spend.length,
        &source, 1, 0x76b809bb, sha256_bytes, &hasher,
        &bound_facts, all_digests, 2) == 0);
    CHECK(bound_facts.fee_zat == 1000000);
    CHECK(memcmp(all_digests[0], direct, 32) == 0);
    for (size_t i = 0; i < 32; ++i) CHECK(all_digests[1][i] == 0x5a);
    bound_facts.fee_zat = UINT64_MAX;
    memset(all_digests, 0x5a, sizeof all_digests);
    CHECK(zcl_tx_transparent_bound_digests(spend.bytes, spend.length,
        &source, 1, 0x76b809bb, sha256_bytes, &hasher,
        &bound_facts, all_digests, 0) < 0);
    CHECK(bound_facts.fee_zat == UINT64_MAX);
    for (size_t i = 0; i < sizeof all_digests; ++i)
        CHECK(((uint8_t *)all_digests)[i] == 0x5a);

    prev.bytes[10] ^= 1;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    CHECK(facts.fee_zat == 1000000);
    CHECK(zcl_tx_hash_bound_digest(spend.bytes, spend.length, 0, source,
        0x76b809bb, sha256_bytes, &hasher, bound) < 0);
    CHECK(zcl_tx_transparent_bound_digests(spend.bytes, spend.length,
        &source, 1, 0x76b809bb, sha256_bytes, &hasher,
        &bound_facts, all_digests, 2) < 0);
    CHECK(bound_facts.fee_zat == UINT64_MAX);
    for (size_t i = 0; i < sizeof all_digests; ++i)
        CHECK(((uint8_t *)all_digests)[i] == 0x5a);
    prev.bytes[10] ^= 1;
    source.length--;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    source.length++;

    transaction duplicate = spending_transaction(txid, 2);
    zcl_tx_previous_transaction two[2] = {source, source};
    CHECK(zcl_tx_transparent_preflight(duplicate.bytes, duplicate.length,
        two, 2, sha256_bytes, &facts) < 0);
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 0, sha256_bytes, &facts) < 0);
    CHECK(zcl_tx_hash_bound_digest(spend.bytes, spend.length, 1, source,
        0x76b809bb, sha256_bytes, &hasher, bound) < 0);
    uint8_t *script = NULL;
    CHECK(zcl_tx_outputs_visit(spend.bytes, spend.length,
                               first_output, &script) == 0 && script);
    script[0] = 0x6a;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    script[0] = 0x76;
    uint8_t amount_backup[8];
    memcpy(amount_backup, script - 9, sizeof amount_backup);
    (script - 9)[3] = 4;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    memcpy(script - 9, amount_backup, sizeof amount_backup);
    spend.bytes[8 + 1 + 32] = 1;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    spend.bytes[8 + 1 + 32] = 0;
    spend.bytes[spend.length - 11] = 1;
    CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
        &source, 1, sha256_bytes, &facts) < 0);
    spend.bytes[spend.length - 11] = 0;
    for (size_t cut = 0; cut < spend.length; ++cut)
        CHECK(zcl_tx_transparent_preflight(spend.bytes, cut,
            &source, 1, sha256_bytes, &facts) < 0);
    for (size_t cut = 0; cut < prev.length; ++cut) {
        source.length = cut;
        CHECK(zcl_tx_transparent_preflight(spend.bytes, spend.length,
            &source, 1, sha256_bytes, &facts) < 0);
    }
    CHECK(facts.fee_zat == 1000000);
    transaction other_prev = previous_transaction();
    other_prev.bytes[10] ^= 1;
    uint8_t other_first[32], other_txid[32];
    CHECK(sha256_bytes(other_prev.bytes, other_prev.length, other_first));
    CHECK(sha256_bytes(other_first, sizeof other_first, other_txid));
    memcpy(duplicate.bytes + 8 + 1 + 41, other_txid, sizeof other_txid);
    two[1] = (zcl_tx_previous_transaction){other_prev.bytes, other_prev.length};
    CHECK(zcl_tx_transparent_preflight(duplicate.bytes, duplicate.length,
        two, 2, sha256_bytes, &facts) == 0);
    CHECK(facts.transparent_inputs == 2 && facts.input_zat == 100000000);
    CHECK(facts.output_zat == 49000000 && facts.fee_zat == 51000000);
    CHECK(zcl_tx_hash_bound_digest(duplicate.bytes, duplicate.length, 1,
        two[1], 0x76b809bb, sha256_bytes, &hasher, bound) == 0);
    CHECK(zcl_tx_transparent_bound_digests(duplicate.bytes, duplicate.length,
        two, 2, 0x76b809bb, sha256_bytes, &hasher,
        &bound_facts, all_digests, 2) == 0);
    CHECK(bound_facts.fee_zat == 51000000);
    CHECK(memcmp(all_digests[1], bound, 32) == 0);
    return 0;
}
