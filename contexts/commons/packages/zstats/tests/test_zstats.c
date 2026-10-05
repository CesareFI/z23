/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * Exercise streaming statistics, count refusal and the pinned merge contract. */
#include "zstats/zstats.h"
#include <float.h>
#include <math.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    double _d = (a) - (b); \
    if (_d < 0) _d = -_d; \
    if (!(_d <= (eps))) { \
        fprintf(stderr, "FAIL %s:%d: %f !~ %f (eps %g)\n", \
                __FILE__, __LINE__, (double)(a), (double)(b), (double)(eps)); \
        exit(1); \
    } \
} while (0)

static void test_empty_and_single(void)
{
    zstats s;
    zstats_init(&s);
    CHECK(zstats_count(&s) == 0);
    CHECK(zstats_mean(&s) == 0.0);
    CHECK(zstats_variance(&s) == 0.0);
    CHECK(zstats_sample_variance(&s) == 0.0);
    CHECK(zstats_stddev(&s) == 0.0);
    CHECK(zstats_min(&s) == 0.0);
    CHECK(zstats_max(&s) == 0.0);
    CHECK(zstats_total(&s) == 0.0L);

    zstats_add(&s, 42.0);
    CHECK(zstats_count(&s) == 1);
    CHECK(zstats_mean(&s) == 42.0);
    CHECK(zstats_variance(&s) == 0.0);
    CHECK(zstats_min(&s) == 42.0 && zstats_max(&s) == 42.0);
    CHECK(zstats_total(&s) == 42.0L);

    /* NULL tolerance. */
    zstats_init(NULL);
    zstats_add(NULL, 1.0);
    zstats_add_repeated(NULL, 1.0, 3);
    zstats_merge(NULL, &s);
    zstats_merge(&s, NULL);
    CHECK(zstats_count(NULL) == 0);
    CHECK(zstats_mean(NULL) == 0.0);
}

static void test_known_dataset(void)
{
    /* Classic: 2 4 4 4 5 5 7 9 -> mean 5, pop var 4, sample var 32/7. */
    const double data[] = {2, 4, 4, 4, 5, 5, 7, 9};
    zstats s;
    zstats_init(&s);
    for (size_t i = 0; i < 8; i++) zstats_add(&s, data[i]);
    CHECK(zstats_count(&s) == 8);
    CHECK_NEAR(zstats_mean(&s), 5.0, 1e-12);
    CHECK_NEAR(zstats_variance(&s), 4.0, 1e-12);
    CHECK_NEAR(zstats_sample_variance(&s), 32.0 / 7.0, 1e-12);
    CHECK_NEAR(zstats_stddev(&s), 2.0, 1e-12);
    CHECK(zstats_min(&s) == 2.0 && zstats_max(&s) == 9.0);
    CHECK(zstats_total(&s) == 40.0L);
}

static void test_repeated_and_merge(void)
{
    /* add_repeated must equal individual adds. */
    zstats a, b;
    zstats_init(&a);
    zstats_init(&b);
    for (int i = 0; i < 100; i++) zstats_add(&a, 7.5);
    zstats_add_repeated(&b, 7.5, 100);
    CHECK(zstats_count(&b) == 100);
    CHECK_NEAR(zstats_mean(&a), zstats_mean(&b), 1e-9);
    CHECK_NEAR(zstats_variance(&a), zstats_variance(&b), 1e-12);

    /* Mixed: 3 then repeated 97. */
    zstats c;
    zstats_init(&c);
    zstats_add(&c, 7.5);
    zstats_add_repeated(&c, 7.5, 99);
    CHECK(zstats_count(&c) == 100);
    CHECK_NEAR(zstats_mean(&c), 7.5, 1e-12);

    /* Merge two halves equals the whole. */
    zstats left, right, whole;
    zstats_init(&left);
    zstats_init(&right);
    zstats_init(&whole);
    for (int i = 0; i < 1000; i++) {
        double x = (double)((i * 7919) % 1000) / 10.0;
        zstats_add(&whole, x);
        if (i % 2 == 0) zstats_add(&left, x);
        else zstats_add(&right, x);
    }
    zstats_merge(&left, &right);
    CHECK(zstats_count(&left) == 1000);
    CHECK_NEAR(zstats_mean(&left), zstats_mean(&whole), 1e-9);
    CHECK_NEAR(zstats_variance(&left), zstats_variance(&whole), 1e-6);
    CHECK(zstats_min(&left) == zstats_min(&whole));
    CHECK(zstats_max(&left) == zstats_max(&whole));

    /* Merge into empty; merge empty. */
    zstats e, full;
    zstats_init(&e);
    zstats_init(&full);
    zstats_add(&full, 3.0);
    zstats_merge(&e, &full);
    CHECK(zstats_count(&e) == 1 && zstats_mean(&e) == 3.0);
    zstats empty;
    zstats_init(&empty);
    zstats_merge(&e, &empty);
    CHECK(zstats_count(&e) == 1);
}

static void test_count_overflow(unsigned row) {
    zstats s, other;
    zstats_init(&s); zstats_init(&other);
    zstats_add_repeated(&s, 1.0, row == 1 ? UINT64_MAX - 1 : UINT64_MAX);
    zstats before = s;
    if (row == 0) zstats_add(&s, 2.0);
    else if (row == 1) zstats_add_repeated(&s, 2.0, 2);
    else { zstats_add(&other, 2.0); zstats_merge(&s, &other); }
    CHECK(s.n == before.n && s.mean == before.mean && s.m2 == before.m2 &&
          s.min == before.min && s.max == before.max && s.sum == before.sum);
}
static void test_count_overflow_rows(void) {
    const char *row = getenv("ZQA_ROW");
    if (row) {
        unsigned r = (unsigned)strtoul(row, NULL, 10);
        if (r > 2) exit(2);
        test_count_overflow(r);
    } else {
        for (unsigned r = 0; r < 3; r++) test_count_overflow(r);
    }
}

static uint64_t rng_state = 0x0123456789abcdefull;
static uint64_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static void test_fuzz_vs_reference(void)
{
    /* Random datasets: streaming result must match a naive two-pass
     * reference within tight tolerance. */
    for (int trial = 0; trial < 200; trial++) {
        size_t n = (size_t)(rng_next() % 500) + 1;
        double data[500];
        for (size_t i = 0; i < n; i++)
            data[i] = (double)(rng_next() % 10000) / 8.0 - 500.0;

        zstats s;
        zstats_init(&s);
        for (size_t i = 0; i < n; i++) zstats_add(&s, data[i]);

        long double sum = 0.0L;
        double mn = data[0], mx = data[0];
        for (size_t i = 0; i < n; i++) {
            sum += data[i];
            if (data[i] < mn) mn = data[i];
            if (data[i] > mx) mx = data[i];
        }
        double mean = (double)(sum / (long double)n);
        double var = 0.0;
        for (size_t i = 0; i < n; i++) {
            double d = data[i] - mean;
            var += d * d;
        }
        var /= (double)n;

        CHECK(zstats_count(&s) == n);
        CHECK_NEAR(zstats_mean(&s), mean, 1e-9);
        CHECK_NEAR(zstats_variance(&s), var, 1e-6);
        CHECK(zstats_min(&s) == mn);
        CHECK(zstats_max(&s) == mx);

        /* Random split-merge equals the whole. */
        zstats l, r;
        zstats_init(&l);
        zstats_init(&r);
        for (size_t i = 0; i < n; i++)
            if (rng_next() & 1) zstats_add(&l, data[i]);
            else zstats_add(&r, data[i]);
        zstats_merge(&l, &r);
        CHECK(zstats_count(&l) == n);
        CHECK_NEAR(zstats_mean(&l), mean, 1e-9);
        CHECK_NEAR(zstats_variance(&l), var, 1e-5);
    }
}

static void test_merge_weight(unsigned row)
{
    zstats a, b;
    zstats_init(&a);
    zstats_init(&b);
    if (row == 0) {
        zstats_add(&a, 1.0);
        zstats_add_repeated(&b, 0.0, UINT64_C(1) << 54);
        zstats reverse = b;
        zstats_merge(&reverse, &a);
        zstats_merge(&a, &b);
        CHECK(a.mean == 0x1p-54 && reverse.mean == 0x1p-54);
        CHECK(a.n == (UINT64_C(1) << 54) + 1 && a.sum == 1.0L);
    } else {
        zstats_add(&a, DBL_MAX);
        zstats_add_repeated(&b, DBL_MAX, 2);
        zstats_merge(&a, &b);
        CHECK(a.mean == DBL_MAX && a.n == 3 && a.m2 == 0.0);
    }
}

static void test_opposite_merge_weight(void)
{
    static const double minority[] = {1.0, -1.0};
    static const double majority[] = {-1.0, 1.0};
    static const double expected[] = {-0x1.fffffffffffffp-1,
                                      0x1.fffffffffffffp-1};
    for (unsigned row = 0; row < 2; row++) {
        zstats a, b;
        zstats_init(&a);
        zstats_init(&b);
        zstats_add(&a, minority[row]);
        zstats_add_repeated(&b, majority[row], UINT64_C(1) << 54);
        zstats reverse = b;
        zstats_merge(&reverse, &a);
        zstats_merge(&a, &b);
        CHECK(a.mean == expected[row] && reverse.mean == expected[row]);
        CHECK(a.n == (UINT64_C(1) << 54) + 1 && reverse.n == a.n);
    }
    /* The product fallback still handles an unrepresentable difference. */
    zstats a, b;
    zstats_init(&a);
    zstats_init(&b);
    zstats_add(&a, DBL_MAX);
    zstats_add(&b, -DBL_MAX);
    zstats reverse = b;
    zstats_merge(&reverse, &a);
    zstats_merge(&a, &b);
    CHECK(a.mean == 0.0 && reverse.mean == 0.0);
}

static void test_nonfinite_merge_row(double minority, double majority,
                                     double expected)
{
    const uint64_t many = UINT64_C(1) << 54;
    zstats a, b;
    zstats_init(&a);
    zstats_init(&b);
    zstats_add(&a, minority);
    zstats_add_repeated(&b, majority, many);
    zstats reverse = b;
    zstats_merge(&reverse, &a);
    zstats_merge(&a, &b);
    CHECK(a.n == many + 1 && reverse.n == a.n);
    CHECK(b.n == many);
    if (isnan(expected)) {
        CHECK(isnan(a.mean) && isnan(reverse.mean));
    } else {
        CHECK(a.mean == expected && reverse.mean == expected);
    }
}

static void test_nonfinite_merge(const char *selected)
{
    static const struct {
        const char *name;
        double minority, majority, expected;
    } rows[] = {
        {"positive_inf", INFINITY, 0.0, INFINITY},
        {"negative_inf", -INFINITY, 0.0, -INFINITY},
        {"positive_inf_negative_finite", INFINITY, -2.0, INFINITY},
        {"negative_inf_positive_finite", -INFINITY, 2.0, -INFINITY},
        {"positive_infinities", INFINITY, INFINITY, INFINITY},
        {"negative_infinities", -INFINITY, -INFINITY, -INFINITY},
        {"minority_nan", NAN, 0.0, NAN},
        {"majority_nan", 0.0, NAN, NAN},
        {"nan_with_inf", NAN, INFINITY, NAN},
        {"inf_with_nan", INFINITY, NAN, NAN},
        {"opposite_inf", INFINITY, -INFINITY, NAN},
        {"reverse_opposite_inf", -INFINITY, INFINITY, NAN},
    };
    bool found = false;
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (selected && strcmp(selected, rows[i].name) != 0) continue;
        found = true;
        test_nonfinite_merge_row(rows[i].minority, rows[i].majority,
                                 rows[i].expected);
    }
    if (!found) exit(2);
}

/* STATS-CONTRACT rows, numbered in specification order. Row 26 exercises
 * both forms of invalid M2. The void API signals refusal by no mutation. */
static void check_contract_value(double actual, double expected)
{
    if (isnan(expected)) { CHECK(isnan(actual)); return; }
    CHECK(actual == expected);
    CHECK(!!signbit(actual) == !!signbit(expected));
}

static void test_merge_contract(unsigned selected)
{
    static const struct {
        uint64_t na, nb, n;
        double a, b, ma, mb, mean, m2;
        bool refused;
    } rows[] = {
        {0, 2, 2, NAN, 3, NAN, 4, 3, 4, false},
        {0, 0, 0, NAN, INFINITY, NAN, -INFINITY, 0, 0, false},
        {2, 3, 5, 7, 7, 3, 4, 7, 7, false},
        {2, 2, 4, 1, 3, 0, 0, 2, 4, false},
        {1, 1, 2, DBL_MAX, DBL_MAX, 0, 0, DBL_MAX, 0, false},
        {1, 1, 2, -DBL_MAX, -DBL_MAX, 0, 0, -DBL_MAX, 0, false},
        {2, 2, 4, DBL_MAX, -DBL_MAX, 0, 0, 0, INFINITY, false},
        {3, 1, 4, DBL_MAX, -DBL_MAX, 0, 0, DBL_MAX/2, INFINITY, false},
        {UINT64_C(1)<<63, (UINT64_C(1)<<63)-1, UINT64_MAX,
         DBL_MAX, -DBL_MAX, 0, 0, 0x1.fffffffffffffp959, INFINITY, false},
        {1, 1, 2, 0, 0x1p512, 0, 0, 0x1p511, 0x1p1023, false},
        {UINT64_C(1)<<62, UINT64_C(1)<<62, UINT64_C(1)<<63,
         0, 0x1p-540, 0, 0, 0x1p-541, 0x1p-1019, false},
        {1, 1, 2, 1, 1, DBL_MAX, DBL_MAX, 1, INFINITY, false},
        {1, 1, 2, 1, 1, INFINITY, 0, 1, INFINITY, false},
        {1, 1, 2, 1, 3, NAN, 0, 2, NAN, false},
        {1, 1, 2, 1, 1, NAN, INFINITY, 1, NAN, false},
        {1, 1, 2, NAN, 2, 0, 0, NAN, NAN, false},
        {1, 1, 2, NAN, INFINITY, 0, 0, NAN, NAN, false},
        {1, 1, 2, INFINITY, 2, 0, 0, INFINITY, NAN, false},
        {1, 1, 2, -INFINITY, 2, 0, 0, -INFINITY, NAN, false},
        {1, 1, 2, INFINITY, INFINITY, 0, 0, INFINITY, NAN, false},
        {1, 1, 2, -INFINITY, -INFINITY, 0, 0, -INFINITY, NAN, false},
        {1, 1, 2, INFINITY, -INFINITY, 0, 0, NAN, NAN, false},
        {2, 2, 4, 4, 4, 6, 6, 4, 12, false},
        {1, 1, 2, -0.0, 0.0, 0, 0, 0.0, 0, false},
        {1, 1, 2, -0.0, -0.0, 0, 0, -0.0, 0, false},
        {1, 1, 0, 0, 0, -INFINITY, 0, 0, 0, true},
        {UINT64_MAX, 1, 0, 0, 0, 0, 0, 0, 0, true},
    };
    CHECK(selected >= 1 && selected <= sizeof rows / sizeof rows[0]);
    size_t i = selected - 1;
    for (unsigned variant = 0; variant < (selected == 26 ? 2u : 1u); variant++) {
        for (unsigned order = 0; order < 2; order++) {
            zstats a = {0}, b = {0};
            a.n = rows[i].na; b.n = rows[i].nb;
            a.mean = rows[i].a; b.mean = rows[i].b;
            a.m2 = variant ? -1.0 : rows[i].ma; b.m2 = rows[i].mb;
            a.min = a.max = a.mean; b.min = b.max = b.mean;
            a.sum = 11; b.sum = 13;
            if (order) { zstats tmp = a; a = b; b = tmp; }
            unsigned char before[sizeof a], source[sizeof b];
            memcpy(before, &a, sizeof a); memcpy(source, &b, sizeof b);
            zstats_merge(&a, &b);
            CHECK(memcmp(source, &b, sizeof b) == 0);
            if (rows[i].refused) {
                CHECK(memcmp(before, &a, sizeof a) == 0);
            } else {
                CHECK(a.n == rows[i].n);
                check_contract_value(a.mean, rows[i].mean);
                check_contract_value(a.m2, rows[i].m2);
                if (selected == 2) {
                    CHECK(a.min == 0 && a.max == 0 && a.sum == 0);
                }
            }
            printf("contract row %u variant %u order %u passed\n",
                   selected, variant, order);
        }
    }
}

int main(void)
{
    const char *contract_row = getenv("ZSTATS_CONTRACT_ROW");
    if (contract_row) {
        char *end;
        unsigned long row = strtoul(contract_row, &end, 10);
        if (*end || row < 1 || row > 27) return 2;
        test_merge_contract((unsigned)row);
        return 0;
    }
    const char *nonfinite_row = getenv("ZSTATS_NONFINITE_ROW");
    if (nonfinite_row) {
        test_nonfinite_merge(nonfinite_row);
        return 0;
    }
    const char *fix_row = getenv("ZSTATS_FIX_ROW");
    if (fix_row) {
        if (strcmp(fix_row, "0") == 0) test_merge_weight(0);
        else if (strcmp(fix_row, "1") == 0) test_merge_weight(1);
        else if (strcmp(fix_row, "2") == 0) test_opposite_merge_weight();
        else return 2;
        return 0;
    }
    for (unsigned row = 1; row <= 27; row++) test_merge_contract(row);
    test_nonfinite_merge(NULL);
    test_merge_weight(0);
    test_merge_weight(1);
    test_opposite_merge_weight();
    test_empty_and_single();
    test_known_dataset();
    test_repeated_and_merge();
    test_count_overflow_rows();
    test_fuzz_vs_reference();
    puts("test_zstats: all groups passed (empty known repeated merge fuzz)");
    return 0;
}
