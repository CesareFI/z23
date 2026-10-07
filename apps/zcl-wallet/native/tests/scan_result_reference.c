/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "scan_result_reference.h"
#include <string.h>

static bool same_address(const zcl_payment_request *left, const zcl_payment_request *right)
{
    return left->address.network == right->address.network &&
        left->address.kind == right->address.kind &&
        memcmp(left->address.hash, right->address.hash, sizeof(left->address.hash)) == 0 &&
        memcmp(left->address_text, right->address_text, sizeof(left->address_text)) == 0;
}

static bool same_fields(const zcl_payment_request *expected, const zcl_payment_request *actual)
{
    /* Only lengths from successful parsing select read ranges. Untrusted
     * reported lengths are compared, never used for indexing or copying. */
    return expected->has_amount == actual->has_amount &&
        expected->has_label == actual->has_label && expected->has_message == actual->has_message &&
        expected->amount == actual->amount && expected->label_len == actual->label_len &&
        expected->message_len == actual->message_len &&
        memcmp(expected->label, actual->label, expected->label_len) == 0 &&
        memcmp(expected->message, actual->message, expected->message_len) == 0;
}

bool scan_result_matches(const uint8_t *text, size_t length, zcl_network network,
                         const zcl_payment_request *request)
{
    if (request == NULL) return false;
    zcl_payment_request expected = {0};
    return zcl_payment_parse(text, length, network, &expected) == ZCL_OK &&
        same_address(&expected, request) && same_fields(&expected, request);
}
