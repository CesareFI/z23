/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void round_trip(const uint8_t *payload, size_t size)
{
    uint8_t text[186], decoded[130];
    memset(text, 0xa5, sizeof(text));
    memset(decoded, 0xa5, sizeof(decoded));
    size_t length = 0, recovered = 0;
    if (zcl_base58check_encode(payload, size, text + 1, 184, &length) != ZCL_OK)
        abort();
    if (length == 0 || length > 184 || text[0] != 0xa5 || text[185] != 0xa5)
        abort();
    if (zcl_base58check_decode(text + 1, length, decoded + 1, 128, &recovered) != ZCL_OK)
        abort();
    if (recovered != size || memcmp(payload, decoded + 1, size) != 0)
        abort();
    if (decoded[0] != 0xa5 || decoded[129] != 0xa5)
        abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t payload[128];
    memset(payload, 0xa5, sizeof(payload));
    size_t length = SIZE_MAX;
    zcl_status status = zcl_base58check_decode(data, size, payload, sizeof(payload), &length);
    if (status == ZCL_OK) {
        if (length == 0 || length > sizeof(payload))
            abort();
        round_trip(payload, length);
    } else {
        if (length != SIZE_MAX)
            abort();
        for (size_t index = 0; index < sizeof(payload); ++index) {
            if (payload[index] != 0xa5)
                abort();
        }
    }
    if (size > 0 && size <= 128)
        round_trip(data, size);
    zcl_address address = {0};
    status = zcl_address_parse(data, size, ZCL_MAINNET, &address);
    if (status == ZCL_OK) {
        uint8_t script[25] = {0};
        size_t script_len = 0;
        if (zcl_address_script(&address, script, sizeof(script), &script_len) != ZCL_OK)
            abort();
        if (script_len != 23 && script_len != 25)
            abort();
    }
    return 0;
}
