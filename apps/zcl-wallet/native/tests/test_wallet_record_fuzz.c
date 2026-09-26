/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <string.h>

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length);

static int check_case(const uint8_t *bytes, size_t length, bool accepted)
{
    zcl_wallet_info info = {0};
    const zcl_status status = zcl_wallet_header_parse(bytes, length, &info);
    if ((status == ZCL_OK) != accepted) {
        fputs("Wallet header acceptance mismatch\n", stderr);
        return 1;
    }
    /* The shared harness separately checks exact metadata, output guards and
     * unchanged output on refusal, for both raw and fixture-XOR inputs. */
    return LLVMFuzzerTestOneInput(bytes, length);
}

static int check_lengths(const uint8_t *header, size_t capacity)
{
    for (size_t length = 0; length <= capacity; ++length)
        if (check_case(header, length, length == ZCL_WALLET_HEADER_BYTES) != 0) return 1;
    return 0;
}

static int check_corruption(const uint8_t *header, size_t length)
{
    uint8_t changed[80] = {0};
    if (header == NULL || length != sizeof(changed)) return 1;
    memcpy(changed, header, length);
    /* Each single-bit change breaks these generated fixtures' fixed fields,
     * profile, chain identity, address checksum or reserved-byte contract. */
    for (size_t i = 0; i < length; ++i) {
        changed[i] ^= 1;
        if (check_case(changed, length, false) != 0) return 1;
        changed[i] ^= 1;
    }
    return 0;
}

static int check_profile(size_t entropy_len, zcl_network network)
{
    /* Published all-zero entropy and fixed blinding, never a funded wallet. */
    uint8_t entropy[32] = {0}, blinding[32] = {1}, header[82] = {0};
    const zcl_status status = zcl_wallet_header_create(entropy, entropy_len, network,
        blinding, sizeof(blinding), header, sizeof(header));
    zcl_secure_zero(entropy, sizeof(entropy));
    zcl_secure_zero(blinding, sizeof(blinding));
    if (status != ZCL_OK) return 1;
    if (check_lengths(header, sizeof(header)) != 0) return 1;
    return check_corruption(header, ZCL_WALLET_HEADER_BYTES);
}

static int check_seed_lengths(void)
{
    /* Public zero-XOR data preserves the harness's valid header at80 bytes,
     * alongside every truncation and lengths beyond either record bound. */
    const uint8_t input[142] = {0};
    for (size_t length = 0; length <= sizeof(input); ++length)
        if (LLVMFuzzerTestOneInput(input, length) != 0) return 1;
    return 0;
}

int main(void)
{
    int argc = 0;
    char **argv = NULL;
    if (LLVMFuzzerInitialize(&argc, &argv) != 0) return 1;
    if (check_seed_lengths() != 0 || check_case(NULL, 0, false) != 0) return 1;
    const zcl_network networks[] = {ZCL_MAINNET, ZCL_TESTNET};
    for (size_t n = 0; n < sizeof(networks) / sizeof(networks[0]); ++n)
        for (size_t length = 16; length <= 32; length += 4)
            if (check_profile(length, networks[n]) != 0) return 1;
    puts("Wallet header contract: both networks, five entropy lengths, truncation and byte corruption passed");
    return 0;
}
