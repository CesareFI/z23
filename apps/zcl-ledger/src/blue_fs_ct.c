/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_fs_ct.h"
#include "blue_mod256.h"

static const struct blue_mod256 fs_field = {
    .modulus = {
        0xd6f72cb7u, 0xd0970e5eu, 0xccc81082u, 0xa6682093u,
        0x01343b00u, 0x06673b01u, 0x6533afa9u, 0x0e7db4eau
    },
    .negative_inverse = 0xef788ef9u
};

static const uint64_t fs_r2[4] = {
    0x67719aa495e57731ULL, 0x51b0cef09ce3fc26ULL,
    0x69dab7fac026e9a5ULL, 0x04f6547b8d127688ULL
};

void blue_fs_add_ct(struct fs *result, const struct fs *a,
    const struct fs *b) {
    blue_mod256_add(result->d, a->d, b->d, &fs_field);
}

void blue_fs_sub_ct(struct fs *result, const struct fs *a,
    const struct fs *b) {
    blue_mod256_sub(result->d, a->d, b->d, &fs_field);
}

void blue_fs_neg_ct(struct fs *result, const struct fs *a) {
    const struct fs zero = {0};
    blue_fs_sub_ct(result, &zero, a);
}

void blue_fs_mul_ct(struct fs *result, const struct fs *a,
    const struct fs *b) {
    uint64_t montgomery[4];
    blue_mod256_mont_mul(montgomery, a->d, fs_r2, &fs_field);
    blue_mod256_mont_mul(result->d, montgomery, b->d, &fs_field);
    blue_mod256_wipe(montgomery, sizeof montgomery);
}

bool blue_fs_from_bytes_canonical(struct fs *result,
    const uint8_t bytes[32]) {
    static const uint64_t order[4] = {
        0xd0970e5ed6f72cb7ULL, 0xa6682093ccc81082ULL,
        0x06673b0101343b00ULL, 0x0e7db4ea6533afa9ULL
    };
    if (!result) return false;
    if (!bytes) {
        blue_mod256_wipe(result, sizeof *result);
        return false;
    }
    for (unsigned limb = 0; limb < 4; ++limb) {
        result->d[limb] = 0;
        for (unsigned byte = 0; byte < 8; ++byte)
            result->d[limb] |= (uint64_t)bytes[limb * 8 + byte] << (byte * 8);
    }
    uint64_t borrow = 0;
    for (unsigned limb = 0; limb < 4; ++limb)
        borrow = result->d[limb] < order[limb] + borrow;
    if (!borrow) blue_mod256_wipe(result, sizeof *result);
    return borrow == 1;
}

void blue_fs_to_bytes(uint8_t bytes[32], const struct fs *value) {
    for (unsigned i = 0; i < 32; ++i)
        bytes[i] = (uint8_t)(value->d[i / 8] >> (8 * (i % 8)));
}
