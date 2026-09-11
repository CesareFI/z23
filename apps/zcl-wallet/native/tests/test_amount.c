/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Fixed diagnostic text: never print an input buffer or key material. */
#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed at line %d\n", __LINE__); return false; \
} } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1

static bool valid_amounts(void)
{
    uint64_t out = UINT64_MAX;
    CHECK(zcl_amount_parse(TEXT("0"), &out) == ZCL_OK && out == 0);
    CHECK(zcl_amount_parse(TEXT("0.00000001"), &out) == ZCL_OK && out == 1);
    CHECK(zcl_amount_parse(TEXT("1"), &out) == ZCL_OK && out == ZCL_ZATOSHIS_PER_COIN);
    CHECK(zcl_amount_parse(TEXT("12.34567890"), &out) == ZCL_OK && out == UINT64_C(1234567890));
    CHECK(zcl_amount_parse(TEXT("21000000"), &out) == ZCL_OK && out == ZCL_MAX_MONEY);
    CHECK(zcl_amount_parse(TEXT("21000000.00000000"), &out) == ZCL_OK && out == ZCL_MAX_MONEY);
    CHECK(zcl_amount_parse(TEXT("20999999.99999999"), &out) == ZCL_OK && out == ZCL_MAX_MONEY - 1);
    return true;
}

static bool invalid_amounts(void)
{
    static const struct { const char *text; size_t size; } cases[] = {
        {"", 0}, {" 1", 2}, {"1 ", 2}, {"+1", 2}, {"-1", 2}, {"01", 2},
        {"1.", 2}, {".1", 2}, {"1e2", 3}, {"1,2", 3}, {"NaN", 3},
        {"1.000000001", 11}, {"21000000.00000001", 17}, {"999999999999999999", 18},
        {"1\n", 2}, {"1\0x", 3}, {"\xff", 1}, {"0x10", 4}
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint64_t out = UINT64_C(42);
        CHECK(zcl_amount_parse((const uint8_t *)cases[index].text, cases[index].size, &out) != ZCL_OK);
        CHECK(out == UINT64_C(42));
    }
    return true;
}

static bool null_and_arithmetic(void)
{
    uint64_t out = UINT64_C(42);
    size_t length = 7;
    uint8_t buffer[17] = {0};
    CHECK(zcl_amount_parse(NULL, 0, &out) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_parse(TEXT("1"), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_parse(TEXT("1"), &out) == ZCL_OK);
    CHECK(zcl_amount_format(0, NULL, 17, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_format(0, buffer, 17, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_add(ZCL_MAX_MONEY, 1, &out) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_amount_add(UINT64_MAX, 1, &out) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_amount_add(1, UINT64_MAX, &out) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_amount_subtract(0, 1, &out) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_amount_subtract(UINT64_MAX, 0, &out) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_amount_add(0, 0, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_subtract(0, 0, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_amount_add(1, 2, &out) == ZCL_OK && out == 3);
    CHECK(zcl_amount_subtract(3, 2, &out) == ZCL_OK && out == 1);
    return true;
}

static bool bounded_formatting(void)
{
    static const uint64_t cases[] = {0, 1, 10, ZCL_ZATOSHIS_PER_COIN,
        UINT64_C(123456789), ZCL_MAX_MONEY - 1, ZCL_MAX_MONEY};
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t buffer[19];
        memset(buffer, 0xa5, sizeof(buffer));
        size_t length = SIZE_MAX;
        CHECK(zcl_amount_format(cases[index], buffer + 1, 17, &length) == ZCL_OK);
        CHECK(length > 0 && length <= 17);
        CHECK(buffer[0] == 0xa5 && buffer[18] == 0xa5);
        uint64_t recovered = UINT64_MAX;
        CHECK(zcl_amount_parse(buffer + 1, length, &recovered) == ZCL_OK);
        CHECK(recovered == cases[index]);
        for (size_t capacity = 0; capacity < length; ++capacity) {
            uint8_t short_buffer[17];
            memset(short_buffer, 0xa5, sizeof(short_buffer));
            size_t unchanged = SIZE_MAX;
            CHECK(zcl_amount_format(cases[index], short_buffer, capacity, &unchanged) == ZCL_BUFFER_TOO_SMALL);
            CHECK(unchanged == SIZE_MAX);
            for (size_t byte = 0; byte < sizeof(short_buffer); ++byte)
                CHECK(short_buffer[byte] == 0xa5);
        }
    }
    return true;
}

int main(void)
{
    if (!valid_amounts() || !invalid_amounts() || !null_and_arithmetic() || !bounded_formatting())
        return 1;
    if (puts("wallet-core: 4 amount test groups passed") == EOF)
        return 1;
    return 0;
}
