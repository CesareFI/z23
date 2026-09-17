/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "merkle_fixture.h"
#include <stdlib.h>
#include <string.h>
static merkle_fixture fixture, saved;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static uint8_t byte_at(const uint8_t *data, size_t size, size_t index)
{
    return index < size ? data[index] : 0;
}
static uint32_t word(const uint8_t *data, size_t size, size_t offset)
{
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) value |= (uint32_t)byte_at(data, size, offset + i) << (8 * i);
    return value;
}
static void mutate(const uint8_t *data, size_t size)
{
    const unsigned mode = byte_at(data, size, 0) % 6;
    if (mode == 0) fixture.branch.transaction_count = word(data, size, 1);
    if (mode == 1) fixture.branch.transaction_index = word(data, size, 1);
    if (mode == 2) fixture.branch.sibling_count = word(data, size, 1);
    if (mode < 3) return;
    uint8_t *bytes = mode == 3 ? fixture.leaf : (mode == 4 ? fixture.root : (uint8_t *)&fixture.branch.siblings);
    const size_t capacity = mode == 5 ? sizeof(fixture.branch.siblings) : 32;
    const size_t length = size - 1 < capacity ? size - 1 : capacity;
    for (size_t i = 0; i < length; ++i) bytes[i] ^= data[i + 1];
}
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 1025) return 0;
    uint32_t count = word(data, size, 5);
    if (count == 0) count = 1;
    const uint32_t index = word(data, size, 9) % count;
    if (!merkle_fixture_path(count, index, &fixture)) abort();
    mutate(data, size);
    memcpy(&saved, &fixture, sizeof(saved));
    const bool expected = merkle_fixture_check(&fixture);
    const zcl_status status = zcl_merkle_branch_check(fixture.leaf, &fixture.branch, fixture.root);
    if ((status == ZCL_OK) != expected || memcmp(&saved, &fixture, sizeof(saved)) != 0) abort();
    return 0;
}
