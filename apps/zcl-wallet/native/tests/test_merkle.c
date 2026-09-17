/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "merkle_fixture.h"
#include "merkle_vectors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Merkle branch at %d\n", __LINE__); abort(); } } while (0)
static merkle_fixture fixture, saved;

static void verify(zcl_status expected)
{
    memcpy(&saved, &fixture, sizeof(saved));
    CHECK(zcl_merkle_branch_check(fixture.leaf, &fixture.branch, fixture.root) == expected);
    CHECK(memcmp(&saved, &fixture, sizeof(saved)) == 0);
    CHECK(merkle_fixture_check(&fixture) == (expected == ZCL_OK));
}

static void full_trees(void)
{
    for (uint32_t count = 1; count <= 65; ++count) for (uint32_t index = 0; index < count; ++index) {
        CHECK(merkle_fixture_tree(count, index, &fixture)); verify(ZCL_OK);
    }
    const uint32_t counts[] = {3, 8, 65, UINT32_MAX};
    const uint32_t indexes[] = {2, 5, 64, UINT32_MAX - 1};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(i < 3 ? merkle_fixture_tree(counts[i], indexes[i], &fixture)
            : merkle_fixture_path(counts[i], indexes[i], &fixture));
        CHECK(memcmp(fixture.root, merkle_roots[i], 32) == 0); verify(ZCL_OK);
    }
}

static void boundary_widths(void)
{
    for (unsigned bits = 0; bits < 32; ++bits) {
        const uint32_t middle = UINT32_C(1) << bits;
        const uint32_t counts[] = {middle, middle + 1, middle == 1 ? 1 : middle - 1};
        for (size_t i = 0; i < 3; ++i) {
            CHECK(merkle_fixture_path(counts[i], 0, &fixture)); verify(ZCL_OK);
            CHECK(merkle_fixture_path(counts[i], counts[i] - 1, &fixture)); verify(ZCL_OK);
        }
    }
    CHECK(merkle_fixture_path(UINT32_MAX, UINT32_MAX - 1, &fixture)); verify(ZCL_OK);
    CHECK(fixture.branch.sibling_count == 32);
}

static void corruptions(void)
{
    for (size_t level = 0; level < 32; ++level) {
        CHECK(merkle_fixture_path(UINT32_MAX, UINT32_MAX - 1, &fixture));
        fixture.branch.siblings[level][level] ^= 1; verify(ZCL_INVALID_ENCODING);
    }
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    fixture.branch.transaction_count = 4; verify(ZCL_INVALID_ENCODING); /* Self-copy is no real sibling. */
    CHECK(merkle_fixture_tree(4, 3, &fixture));
    fixture.branch.transaction_index = 2; verify(ZCL_INVALID_ENCODING);
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    fixture.leaf[0] ^= 1; verify(ZCL_INVALID_ENCODING);
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    fixture.root[31] ^= 1; verify(ZCL_INVALID_ENCODING);
}

static void arguments(void)
{
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    CHECK(zcl_merkle_branch_check(NULL, &fixture.branch, fixture.root) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_merkle_branch_check(fixture.leaf, NULL, fixture.root) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_merkle_branch_check(fixture.leaf, &fixture.branch, NULL) == ZCL_INVALID_ARGUMENT);
    fixture.branch.transaction_count = 0; verify(ZCL_OUT_OF_RANGE);
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    fixture.branch.transaction_index = 3; verify(ZCL_OUT_OF_RANGE);
    fixture.branch.transaction_index = UINT32_MAX; verify(ZCL_OUT_OF_RANGE);
    CHECK(merkle_fixture_tree(3, 2, &fixture));
    for (size_t depth = 0; depth <= 33; ++depth) {
        fixture.branch.sibling_count = depth;
        if (depth == 2) verify(ZCL_OK);
        else verify(depth > 32 ? ZCL_RESOURCE_EXHAUSTED : ZCL_INVALID_ENCODING);
    }
    fixture.branch.sibling_count = SIZE_MAX; verify(ZCL_RESOURCE_EXHAUSTED);
}

int main(void)
{
    full_trees(); boundary_widths(); corruptions(); arguments();
    puts("Bounded Merkle branch byte order, exact shape and reference roots passed");
    return 0;
}
