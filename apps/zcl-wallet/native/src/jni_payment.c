/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"

#include <string.h>

static bool text_metadata_valid(const zcl_payment_request *request)
{
    if (!request->has_label && request->label_len != 0)
        return false;
    if (!request->has_message && request->message_len != 0)
        return false;
    return true;
}

/* UI adapter packet v1: version, presence flags, 35 address bytes, LE amount,
 * two LE 16-bit UTF-8 lengths, then the label and message. No native struct
 * bytes or padding are serialized. This is not a Zclassic wire format. */
jbyteArray zcl_jni_payment_record(JNIEnv *env, const zcl_payment_request *request)
{
    if (request == NULL)
        return NULL;
    if (request->label_len > 200 || request->message_len > 200)
        return NULL;
    if (!text_metadata_valid(request))
        return NULL;
    uint8_t record[449] = {0};
    record[0] = 1;
    record[1] = (uint8_t)((request->has_amount ? 1U : 0U) |
        (request->has_label ? 2U : 0U) | (request->has_message ? 4U : 0U));
    memcpy(record + 2, request->address_text, sizeof(request->address_text));
    for (size_t index = 0; index < 8; ++index)
        record[37 + index] = (uint8_t)((request->amount >> (index * 8)) & UINT64_C(255));
    /* Both lengths are <= 200; the initialized high bytes remain zero. */
    record[45] = (uint8_t)request->label_len;
    record[47] = (uint8_t)request->message_len;
    memcpy(record + 49, request->label, request->label_len);
    memcpy(record + 49 + request->label_len, request->message, request->message_len);
    return zcl_jni_new_bytes(env, record, 49 + request->label_len + request->message_len);
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_parsePayment(JNIEnv *env, jclass type,
                                                     jbyteArray input, jint chain)
{
    (void)type;
    uint8_t text[1024] = {0};
    size_t length = 0;
    zcl_network network;
    zcl_payment_request request = {0};
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, text, sizeof(text), &length) != ZCL_OK)
        return NULL;
    if (zcl_payment_parse(text, length, network, &request) != ZCL_OK)
        return NULL;
    return zcl_jni_payment_record(env, &request);
}
