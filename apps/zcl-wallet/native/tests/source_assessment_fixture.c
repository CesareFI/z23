/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "source_assessment_fixture.h"
#include <mbedtls/sha256.h>
#include <string.h>

bool source_assessment_hash(const uint8_t *wire, size_t length, uint8_t id[32])
{
    uint8_t first[32], second[32];
    if (mbedtls_sha256(wire, length, first, 0) != 0) return false;
    if (mbedtls_sha256(first, sizeof(first), second, 0) != 0) return false;
    for (size_t i = 0; i < 32; ++i) id[i] = second[31 - i];
    return true;
}

static bool append(uint8_t *wire, size_t *used, bool present, size_t width)
{
    const size_t length = present ? width : 0;
    if (*used >= 4096 || length > 4096 - *used - 1) return false;
    wire[(*used)++] = present ? 1 : 0;
    memset(wire + *used, 0x5c, length); *used += length;
    return true;
}

bool source_assessment_extend(source_assessment_fixture *fixture, size_t index, unsigned tail)
{
    if (fixture == NULL || index >= 2 || tail > 7) return false;
    if (!assessment_fixture_rebind(&fixture->base, index)) return false;
    size_t used = fixture->base.sources[index].length - 3;
    uint8_t *wire = fixture->wire[index];
    memcpy(wire, fixture->base.wire[index], used);
    if (!append(wire, &used, (tail & 1U) != 0, 384)) return false;
    if (!append(wire, &used, (tail & 2U) != 0, 948)) return false;
    if (!append(wire, &used, (tail & 4U) != 0, 1698)) return false;
    const size_t signatures = ((tail & 4U) != 0 ? 96u : 0u) + ((tail & 3U) != 0 ? 64u : 0u);
    if (signatures > 4096 - used) return false;
    memset(wire + used, 0xa3, signatures); used += signatures;
    if (!source_assessment_hash(wire, used, fixture->base.spending.inputs[index].previous_txid)) return false;
    fixture->base.sources[index].wire = wire;
    fixture->base.sources[index].length = used;
    return true;
}

bool source_assessment_init(source_assessment_fixture *fixture, unsigned tail)
{
    if (fixture == NULL || !assessment_fixture_init(&fixture->base)) return false;
    return source_assessment_extend(fixture, 0, tail) && source_assessment_extend(fixture, 1, tail);
}
