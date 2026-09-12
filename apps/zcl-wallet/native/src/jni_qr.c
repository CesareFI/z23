/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_qr.h"

/* Public adapter record: width byte, followed by width*width module bytes.
 * No native handles, array pins, secret input or retained memory.
 */
JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_receiveQr(JNIEnv *env, jclass type,
                                                  jbyteArray input, jint chain)
{
    (void)type;
    uint8_t text[35] = {0}, record[1 + ZCL_RECEIVE_QR_MODULES_MAX] = {0};
    size_t length = 0, side = 0;
    zcl_network network;
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, text, sizeof(text), &length) != ZCL_OK)
        return NULL;
    if (zcl_receive_qr(text, length, network, record + 1, sizeof(record) - 1, &side) != ZCL_OK)
        return NULL;
    record[0] = (uint8_t)side; /* C core proves side <= 41. */
    return zcl_jni_new_bytes(env, record, 1 + side * side);
}
