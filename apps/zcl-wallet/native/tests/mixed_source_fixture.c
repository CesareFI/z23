/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "mixed_source_fixture.h"
#include <string.h>
#ifdef ZCL_MIXED_SOURCE_ORACLE
#include <openssl/evp.h>
#endif

static bool identity(const uint8_t *wire, size_t length, uint8_t id[32])
{
#ifdef ZCL_MIXED_SOURCE_ORACLE
    uint8_t first[32], second[32]; unsigned count = 0;
    if (EVP_Digest(wire, length, first, &count, EVP_sha256(), NULL) != 1 || count != 32) return false;
    if (EVP_Digest(first, sizeof(first), second, &count, EVP_sha256(), NULL) != 1 || count != 32) return false;
    for (size_t i = 0; i < 32; ++i) id[i] = second[31 - i];
    return true;
#else
    return source_assessment_hash(wire, length, id);
#endif
}

static bool joinsplit(uint8_t wire[4096], size_t *used)
{
    if (*used > 4096 - 1899) return false;
    wire[(*used)++] = 1;
    memset(wire + *used, 0x5c, 1802);
    static const size_t prefixes[] = {304,337,370,435,468,501,534,567};
    for (size_t i = 0; i < 8; ++i) wire[*used + prefixes[i]] = i == 2 ? 10 : 2;
    *used += 1802;
    memset(wire + *used, 0xa3, 96); *used += 96;
    return true;
}

static bool legacy(source_assessment_fixture *fixture, size_t index, unsigned profile)
{
    if (!assessment_fixture_rebind(&fixture->base, index)) return false;
    const size_t length = fixture->base.sources[index].length;
    if (length < 23 || length > ZCL_TX_WIRE_MAX) return false;
    uint8_t *wire = fixture->wire[index];
    size_t used = 0;
    if (profile >= 3) {
        used = length - 11; memcpy(wire, fixture->base.wire[index], used);
        wire[0] = 3; wire[4] = 0x70; wire[5] = 0x82; wire[6] = 0xc4; wire[7] = 3;
    } else {
        memset(wire, 0, 4); wire[0] = profile == 0 ? 1 : 2;
        memcpy(wire + 4, fixture->base.wire[index] + 8, length - 23); used = length - 19;
    }
    if (profile == 2 || profile == 4) { if (!joinsplit(wire, &used)) return false; }
    else if (profile != 0) wire[used++] = 0;
    fixture->base.sources[index].wire = wire; fixture->base.sources[index].length = used;
    return identity(wire, used, fixture->base.spending.inputs[index].previous_txid);
}

bool mixed_source_extend(source_assessment_fixture *fixture, size_t index, unsigned profile)
{
    if (fixture == NULL || index >= 2 || profile > 12) return false;
    if (profile < 5) return legacy(fixture, index, profile);
    if (!source_assessment_extend(fixture, index, profile - 5)) return false;
    return identity(fixture->wire[index], fixture->base.sources[index].length,
        fixture->base.spending.inputs[index].previous_txid);
}

bool mixed_source_init(source_assessment_fixture *fixture, unsigned first, unsigned second)
{
    if (!source_assessment_init(fixture, 0)) return false;
    return mixed_source_extend(fixture, 0, first) && mixed_source_extend(fixture, 1, second);
}
