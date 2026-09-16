/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#include "zcl_wallet_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "wallet record check failed at line %d\n", __LINE__); return 1; } } while (0)

static unsigned info_clears, record_clears;
void zcl_record_test_zero(void *pointer, size_t length);
void zcl_record_test_zero(void *pointer, size_t length)
{
    if (pointer == NULL) abort();
    if (length == sizeof(zcl_wallet_info)) ++info_clears;
    else if (length == sizeof(zcl_wallet_record)) ++record_clears;
    else abort();
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != 0) abort();
    }
}

static int supported_records(void)
{
    uint8_t entropy[32] = {0}, blinding[32] = {1}, iv[12] = {0}, ciphertext[48] = {0};
    for (size_t size = 16; size <= 32; size += 4) {
        for (unsigned chain = 0; chain <= 1; ++chain) {
            uint8_t header[82] = {0}, record[142] = {0}, address[37] = {0};
            zcl_wallet_info info = {0};
            zcl_wallet_record parsed = {0};
            size_t record_len = 0;
            memset(header, 0xa5, sizeof(header));
            memset(record, 0xa5, sizeof(record));
            memset(address, 0xa5, sizeof(address));
            CHECK(zcl_wallet_header_create(entropy, size, (zcl_network)chain, blinding, sizeof(blinding), header + 1, 80) == ZCL_OK);
            CHECK(header[0] == 0xa5 && header[81] == 0xa5);
            CHECK(zcl_wallet_header_parse(header + 1, 80, &info) == ZCL_OK);
            CHECK(info.network == (zcl_network)chain && info.entropy_len == size);
            info_clears = record_clears = 0;
            CHECK(zcl_wallet_record_pack(header + 1, 80, iv, sizeof(iv), ciphertext, size + 16,
                                          record + 1, 140, &record_len) == ZCL_OK);
            CHECK(record_len == 108 + size && record[0] == 0xa5 && record[141] == 0xa5);
            CHECK(info_clears == 1 && record_clears == 0);
            CHECK(zcl_wallet_record_parse(record + 1, record_len, &parsed) == ZCL_OK);
            CHECK(info_clears == 1 && record_clears == 1);
            CHECK(memcmp(parsed.header, header + 1, 80) == 0 && parsed.ciphertext_len == size + 16);
            CHECK(memcmp(parsed.iv, iv, sizeof(iv)) == 0);
            CHECK(memcmp(parsed.ciphertext, ciphertext, size + 16) == 0);
            CHECK(zcl_wallet_recovered_address(parsed.header, sizeof(parsed.header), entropy, size,
                blinding, sizeof(blinding), address + 1, 35) == ZCL_OK);
            CHECK(address[0] == 0xa5 && address[36] == 0xa5 && memcmp(address + 1, info.address, 35) == 0);
        }
    }
    return 0;
}

static int mutated_headers(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, header[80] = {0}, changed[80] = {0};
    uint8_t address[35] = {0};
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, sizeof(blinding), header, sizeof(header)) == ZCL_OK);
    for (size_t offset = 0; offset < sizeof(header); ++offset) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            zcl_wallet_info info = {0};
            memcpy(changed, header, sizeof(changed));
            changed[offset] ^= (uint8_t)(1U << bit);
            zcl_status status = zcl_wallet_header_parse(changed, sizeof(changed), &info);
            if (status == ZCL_OK) {
                /* A structurally valid entropy-size edit still fails against
                 * the recovered secret. Header parsing is not authentication. */
                CHECK(zcl_wallet_recovered_address(changed, sizeof(changed), entropy, sizeof(entropy),
                    blinding, sizeof(blinding), address, sizeof(address)) != ZCL_OK);
            }
        }
    }
    entropy[0] = 1;
    memset(address, 0xa5, sizeof(address));
    CHECK(zcl_wallet_recovered_address(header, sizeof(header), entropy, sizeof(entropy),
        blinding, sizeof(blinding), address, sizeof(address)) == ZCL_INVALID_ENCODING);
    for (size_t i = 0; i < sizeof(address); ++i)
        CHECK(address[i] == 0xa5);
    return 0;
}

static int bounds(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, header[80] = {0}, iv[12] = {0}, cipher[32] = {0};
    uint8_t record[141] = {0}, before[141] = {0};
    zcl_wallet_record parsed = {0}, prior = {0};
    zcl_wallet_info info = {0};
    size_t length = 777;
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, sizeof(blinding), header, sizeof(header)) == ZCL_OK);
    memset(record, 0xa5, sizeof(record));
    memcpy(before, record, sizeof(before));
    for (size_t capacity = 0; capacity < 124; ++capacity) {
        CHECK(zcl_wallet_record_pack(header, 80, iv, 12, cipher, 32, record, capacity, &length) == ZCL_BUFFER_TOO_SMALL);
        CHECK(length == 777 && memcmp(record, before, sizeof(record)) == 0);
    }
    CHECK(zcl_wallet_header_parse(NULL, 80, &info) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_header_parse(header, SIZE_MAX, &info) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_record_pack(header, 80, iv, SIZE_MAX, cipher, 32, record, sizeof(record), &length) != ZCL_OK);
    CHECK(zcl_wallet_record_pack(header, 80, iv, 12, cipher, SIZE_MAX, record, sizeof(record), &length) != ZCL_OK);
    CHECK(zcl_wallet_record_parse(NULL, 0, &parsed) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_record_parse(record, SIZE_MAX, &parsed) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_wallet_record_pack(header, 80, iv, 12, cipher, 32, record, sizeof(record), &length) == ZCL_OK);
    CHECK(length == 124);
    memset(&parsed, 0xa5, sizeof(parsed));
    memcpy(&prior, &parsed, sizeof(prior));
    for (size_t size = 0; size <= sizeof(record); ++size) {
        if (size == 124)
            continue;
        CHECK(zcl_wallet_record_parse(record, size, &parsed) != ZCL_OK);
        CHECK(memcmp(&parsed, &prior, sizeof(parsed)) == 0);
    }
    record[80] ^= 1;
    record[123] ^= 1;
    CHECK(zcl_wallet_record_parse(record, 124, &parsed) == ZCL_OK);
    /* IV/tag corruption cannot be detected here; platform GCM must reject it. */
    return 0;
}

static int refused_record_retirement(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, header[80] = {0};
    uint8_t iv[12] = {0}, ciphertext[32] = {0}, record[140] = {0};
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET,
        blinding, sizeof(blinding), header, sizeof(header)) == ZCL_OK);
    for (unsigned fault = 0; fault < 3; ++fault) {
        size_t length = 999;
        memset(record, 0xa5, sizeof(record));
        if (fault == 2) header[4] = 0xff;
        info_clears = record_clears = 0;
        CHECK(zcl_wallet_record_pack(header, 80, iv, 12, ciphertext,
            fault == 0 ? 31 : 32, record, fault == 1 ? 123 : 140, &length) != ZCL_OK);
        CHECK(info_clears == 1 && record_clears == 0 && length == 999);
        for (size_t i = 0; i < sizeof(record); ++i) CHECK(record[i] == 0xa5);
    }
    for (unsigned fault = 0; fault < 2; ++fault) {
        zcl_wallet_record output, before;
        memset(&output, 0xa5, sizeof(output));
        memcpy(&before, &output, sizeof(before));
        header[4] = fault == 0 ? 1 : 0xff;
        memcpy(record, header, sizeof(header));
        record_clears = 0;
        CHECK(zcl_wallet_record_parse(record, fault == 0 ? 125 : 124, &output) != ZCL_OK);
        CHECK(record_clears == 1 && memcmp(&output, &before, sizeof(output)) == 0);
    }
    return 0;
}

int main(void)
{
    if (supported_records() || mutated_headers() || bounds() || refused_record_retirement())
        return 1;
    puts("wallet record: all entropy/network formats, header edits and bounds passed");
    return 0;
}
