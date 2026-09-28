/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_sapling_ock.h"

#include <string.h>

static bool complete_inputs(const uint8_t ovk[32], const uint8_t cv[32],
    const uint8_t cm[32], const uint8_t epk[32],
    const zcl_zip243_hasher *hasher) {
    return ovk && cv && cm && epk && hasher && hasher->context &&
        hasher->init && hasher->update && hasher->final;
}

bool blue_sapling_ock(uint8_t key[32], const uint8_t ovk[32],
    const uint8_t cv[32], const uint8_t cm[32], const uint8_t epk[32],
    const zcl_zip243_hasher *hasher) {
    static const uint8_t personal[16] = {
        'Z','c','a','s','h','_','D','e','r','i','v','e','_','o','c','k'
    };
    if (!key) return false;
    if (!complete_inputs(ovk, cv, cm, epk, hasher)) {
        memset(key, 0, 32);
        return false;
    }
    bool valid = hasher->init(hasher->context, personal) &&
        hasher->update(hasher->context, ovk, 32) &&
        hasher->update(hasher->context, cv, 32) &&
        hasher->update(hasher->context, cm, 32) &&
        hasher->update(hasher->context, epk, 32) &&
        hasher->final(hasher->context, key);
    if (!valid) memset(key, 0, 32);
    return valid;
}
