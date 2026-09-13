/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void encoding(const zcl_address *address, size_t capacity)
{
    uint8_t box[37], original[37];
    memset(box, 0xa5, sizeof(box));
    memcpy(original, box, sizeof(original));
    size_t length = SIZE_MAX;
    const zcl_status status = zcl_address_encode(address, box + 1, capacity, &length);
    if (box[0] != 0xa5 || box[36] != 0xa5) abort();
    if (status != ZCL_OK) {
        if (length != SIZE_MAX || memcmp(box, original, sizeof(box)) != 0) abort();
        return;
    }
    if (length != 35 || capacity < length) abort();
    zcl_address parsed;
    if (zcl_address_parse(box + 1, length, address->network, &parsed) != ZCL_OK) abort();
    if (parsed.kind != address->kind || parsed.network != address->network) abort();
    if (memcmp(parsed.hash, address->hash, sizeof(parsed.hash)) != 0) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 184) return 0;
    zcl_address address = {(zcl_network)(data[0] % 2), (zcl_address_kind)(1 + data[0] % 2), {0}};
    for (size_t i = 0; i < sizeof(address.hash); ++i) address.hash[i] = data[i % size];
    encoding(&address, 35);
    encoding(&address, data[0] % 36);
    address.kind = (zcl_address_kind)(data[0] % 4);
    encoding(&address, 35);
    address.network = (zcl_network)(data[0] % 4);
    encoding(&address, 35);
    for (int network = 0; network < 2; ++network) {
        if (zcl_address_parse(data, size, (zcl_network)network, &address) == ZCL_OK) {
            uint8_t text[35];
            size_t length = 0;
            if (zcl_address_encode(&address, text, sizeof(text), &length) != ZCL_OK) abort();
            if (length != size || memcmp(text, data, length) != 0) abort();
        }
    }
    return 0;
}
