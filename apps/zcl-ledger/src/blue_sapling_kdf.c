/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_sapling_kdf.h"

#include <string.h>

bool blue_sapling_kdf(uint8_t key[32], const uint8_t dh[32],
    const uint8_t epk[32], const zcl_zip243_hasher *hasher) {
    static const uint8_t personal[16] = {
        'Z','c','a','s','h','_','S','a','p','l','i','n','g','K','D','F'
    };
    if (!key) return false;
    if (!dh || !epk || !hasher || !hasher->context || !hasher->init ||
        !hasher->update || !hasher->final) {
        memset(key, 0, 32);
        return false;
    }
    bool valid = hasher->init(hasher->context, personal) &&
        hasher->update(hasher->context, dh, 32) &&
        hasher->update(hasher->context, epk, 32) &&
        hasher->final(hasher->context, key);
    if (!valid) memset(key, 0, 32);
    return valid;
}
