/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_fr_ct.h"
#include "blue_mod256.h"

static const struct blue_mod256 fr_field = {
    .modulus = {
        0x00000001u, 0xffffffffu, 0xfffe5bfeu, 0x53bda402u,
        0x09a1d805u, 0x3339d808u, 0x299d7d48u, 0x73eda753u
    },
    .negative_inverse = 0xffffffffu
};

void blue_fr_add_ct(struct fr *result, const struct fr *a,
    const struct fr *b) {
    blue_mod256_add(result->d, a->d, b->d, &fr_field);
}

void blue_fr_sub_ct(struct fr *result, const struct fr *a,
    const struct fr *b) {
    blue_mod256_sub(result->d, a->d, b->d, &fr_field);
}

void blue_fr_neg_ct(struct fr *result, const struct fr *a) {
    const struct fr zero = {0};
    blue_fr_sub_ct(result, &zero, a);
}

void blue_fr_mul_ct(struct fr *result, const struct fr *a,
    const struct fr *b) {
    blue_mod256_mont_mul(result->d, a->d, b->d, &fr_field);
}
