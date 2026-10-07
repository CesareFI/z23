/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include "uri_text.h"
#include "utf8_reference.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static zcl_status check_request(const uint8_t *data, size_t size, zcl_payment_request *request)
{
    uint8_t original[sizeof(*request)];
    memset(request, 0xa5, sizeof(*request));
    memcpy(original, request, sizeof(original));
    zcl_status status = zcl_payment_parse(data, size, ZCL_MAINNET, request);
    if (status != ZCL_OK) {
        if (memcmp(original, request, sizeof(original)) != 0)
            abort();
        return status;
    }
    if (request->label_len > 200 || request->message_len > 200)
        abort();
    if (utf8_reference_text(request->label, request->label_len) != ZCL_OK ||
        utf8_reference_text(request->message, request->message_len) != ZCL_OK) abort();
    if (request->has_amount && (request->amount == 0 || request->amount > ZCL_MAX_MONEY))
        abort();
    zcl_address restored = {0};
    if (zcl_address_parse(request->address_text, sizeof(request->address_text), ZCL_MAINNET, &restored) != ZCL_OK)
        abort();
    if (restored.kind != request->address.kind || memcmp(restored.hash, request->address.hash, 20) != 0)
        abort();
    return status;
}

static void check_label(const zcl_payment_request *request, const uint8_t *data, size_t size)
{
    if (!request->has_label || request->has_amount || request->has_message || request->label_len != size)
        abort();
    if (size != 0 && memcmp(request->label, data, size) != 0)
        abort();
}

static void encoded_label(const uint8_t *data, size_t size)
{
    if (size > 200)
        return;
    static const uint8_t prefix[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?label=";
    static const uint8_t hex[] = "0123456789ABCDEF";
    uint8_t text[1024] = {0};
    size_t length = sizeof(prefix) - 1;
    memcpy(text, prefix, length);
    /* Bound above guarantees length + size*3 < sizeof(text). */
    for (size_t index = 0; index < size; ++index) {
        text[length++] = (uint8_t)'%';
        text[length++] = hex[data[index] >> 4];
        text[length++] = hex[data[index] & 15];
    }
    zcl_payment_request request;
    const zcl_status status = check_request(text, length, &request);
    /* The fixed address and percent encoding are valid by construction. The
     * independent display oracle decides label admission, including an empty
     * label generated from the NULL/zero-length host fixture. Blanket refusal
     * and accepted-but-changed metadata must not pass this structured path. */
    const bool expected = size == 0 || utf8_reference_text(data, size) == ZCL_OK;
    if ((status == ZCL_OK) != expected) abort();
    if (status == ZCL_OK) check_label(&request, data, size);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (zcl_utf8_visible_text(data, size) != utf8_reference_text(data, size)) abort();
    zcl_payment_request request;
    (void)check_request(data, size, &request);
    encoded_label(data, size);
    static const uint8_t prefix[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?";
    uint8_t text[1024] = {0};
    size_t prefix_len = sizeof(prefix) - 1;
    if (size <= sizeof(text) - prefix_len) {
        memcpy(text, prefix, prefix_len);
        if (size != 0)
            memcpy(text + prefix_len, data, size);
        (void)check_request(text, prefix_len + size, &request);
    }
    return 0;
}
