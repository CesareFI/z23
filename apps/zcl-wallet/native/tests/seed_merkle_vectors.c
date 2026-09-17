/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "merkle_fixture.h"
#include <stdio.h>
static merkle_fixture fixture;
int main(void)
{
    const uint32_t counts[] = {3, 8, 65, UINT32_MAX};
    const uint32_t indexes[] = {2, 5, 64, UINT32_MAX - 1};
    puts("/* Public synthetic Merkle roots; generated with OpenSSL by seed_merkle_vectors. */");
    puts("static const uint8_t merkle_roots[4][32] = {");
    for (size_t i = 0; i < 4; ++i) {
        const bool ok = i < 3 ? merkle_fixture_tree(counts[i], indexes[i], &fixture)
            : merkle_fixture_path(counts[i], indexes[i], &fixture);
        if (!ok) return 1;
        fputs("    {", stdout);
        for (size_t j = 0; j < 32; ++j) printf("0x%02x,", (unsigned)fixture.root[j]);
        puts("},");
    }
    puts("};");
    return ferror(stdout) != 0 ? 1 : 0;
}
