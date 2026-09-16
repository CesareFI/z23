/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#include "change_custody_retirement.h"
#include "../src/change_custody_internal.h"
#include <stdio.h>
#include <stdlib.h>

#define REQUIRE(v) do { if (!(v)) { fprintf(stderr, "custody retirement at %d\n", __LINE__); abort(); } } while (0)
static uintptr_t owner_identity;
static bool owner_live;
static unsigned parsed_clears;
_Static_assert(sizeof(zcl_change_custody) != sizeof(zcl_wallet_record), "distinct observed owners");

zcl_status __real_zcl_change_custody_prepare(const uint8_t *, size_t,
    const uint8_t *, size_t, zcl_change_custody *);
zcl_status __wrap_zcl_change_custody_prepare(const uint8_t *, size_t,
    const uint8_t *, size_t, zcl_change_custody *);

zcl_status __wrap_zcl_change_custody_prepare(const uint8_t *record, size_t record_len,
    const uint8_t *entropy, size_t entropy_len, zcl_change_custody *wallet)
{
    REQUIRE(!owner_live && owner_identity == 0);
    if (wallet == NULL)
        return __real_zcl_change_custody_prepare(record, record_len, entropy, entropy_len, wallet);
    owner_identity = (uintptr_t)wallet;
    owner_live = true;
    parsed_clears = 0;
    const bool admitted = record != NULL && entropy != NULL && record_len >= 124 && record_len <= 140;
    const zcl_status status = __real_zcl_change_custody_prepare(record, record_len,
        entropy, entropy_len, wallet);
    REQUIRE(parsed_clears == (admitted ? 1U : 0U));
    return status;
}

bool change_custody_retirement_zero(void *pointer, size_t length)
{
    if (length != sizeof(zcl_change_custody) && length != sizeof(zcl_wallet_record)) return false;
    REQUIRE(pointer != NULL && owner_live);
    if (length == sizeof(zcl_change_custody)) {
        REQUIRE((uintptr_t)pointer == owner_identity);
        owner_identity = 0;
        owner_live = false;
    } else {
        REQUIRE(parsed_clears == 0);
        ++parsed_clears;
    }
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) REQUIRE(bytes[i] == 0);
    return true;
}

void change_custody_retirement_check(void)
{
    REQUIRE(!owner_live && owner_identity == 0);
}
