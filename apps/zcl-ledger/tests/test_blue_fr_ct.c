/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_fr_ct.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

typedef void (*field_binary)(struct fr *, const struct fr *, const struct fr *);

static void check_binary(field_binary reference, field_binary candidate,
    const struct fr *a, const struct fr *b) {
    struct fr expected, actual, alias_a = *a, alias_b = *b;
    reference(&expected, a, b);
    candidate(&actual, a, b);
    assert(memcmp(&actual, &expected, sizeof actual) == 0);
    candidate(&alias_a, &alias_a, b);
    assert(memcmp(&alias_a, &expected, sizeof alias_a) == 0);
    candidate(&alias_b, a, &alias_b);
    assert(memcmp(&alias_b, &expected, sizeof alias_b) == 0);
}

static void check_pair(const struct fr *a, const struct fr *b) {
    check_binary(fr_add, blue_fr_add_ct, a, b);
    check_binary(fr_sub, blue_fr_sub_ct, a, b);
    check_binary(fr_mul, blue_fr_mul_ct, a, b);
    struct fr expected, actual, aliased = *a;
    fr_neg(&expected, a);
    blue_fr_neg_ct(&actual, a);
    assert(memcmp(&actual, &expected, sizeof actual) == 0);
    blue_fr_neg_ct(&aliased, &aliased);
    assert(memcmp(&aliased, &expected, sizeof aliased) == 0);
}

static struct fr random_field(uint64_t *state) {
    uint8_t bytes[32];
    for (size_t i = 0; i < sizeof bytes; ++i) {
        *state = *state * 6364136223846793005ULL +
            1442695040888963407ULL;
        bytes[i] = (uint8_t)(*state >> 56);
    }
    bytes[31] &= 0x3fu;
    struct fr value;
    assert(fr_from_bytes(&value, bytes));
    return value;
}

int main(void) {
    struct fr zero, one, minus_one;
    fr_zero(&zero);
    fr_one(&one);
    uint8_t p_minus_one[32] = {
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0xfe, 0x5b, 0xfe, 0xff, 0x02, 0xa4, 0xbd, 0x53,
        0x05, 0xd8, 0xa1, 0x09, 0x08, 0xd8, 0x39, 0x33,
        0x48, 0x7d, 0x9d, 0x29, 0x53, 0xa7, 0xed, 0x73
    };
    assert(fr_from_bytes(&minus_one, p_minus_one));
    const struct fr *edges[] = {&zero, &one, &minus_one};
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j) check_pair(edges[i], edges[j]);
    uint64_t state = 0x23c1a55b7e491dc3ULL;
    for (size_t sample = 0; sample < 4096; ++sample) {
        struct fr a = random_field(&state), b = random_field(&state);
        check_pair(&a, &b);
        check_pair(&a, &a);
    }
    return 0;
}
