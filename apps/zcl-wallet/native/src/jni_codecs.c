/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_keys.h"

#include <string.h>

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_encodeBase58(JNIEnv *env, jclass type,
                                                     jbyteArray input)
{
    (void)type;
    uint8_t payload[128] = {0}, text[184] = {0};
    size_t size = 0, length = 0;
    zcl_status status = zcl_jni_read_bytes(env, input, payload, sizeof(payload), &size);
    if (status == ZCL_OK)
        status = zcl_base58check_encode(payload, size, text, sizeof(text), &length);
    zcl_secure_zero(payload, sizeof(payload));
    jbyteArray output = status == ZCL_OK ? zcl_jni_new_bytes(env, text, length) : NULL;
    zcl_secure_zero(text, sizeof(text));
    return output;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_decodeBase58(JNIEnv *env, jclass type,
                                                     jbyteArray input)
{
    (void)type;
    uint8_t text[184] = {0}, payload[128] = {0};
    size_t length = 0, size = 0;
    zcl_status status = zcl_jni_read_bytes(env, input, text, sizeof(text), &length);
    if (status == ZCL_OK)
        status = zcl_base58check_decode(text, length, payload, sizeof(payload), &size);
    zcl_secure_zero(text, sizeof(text));
    jbyteArray output = status == ZCL_OK ? zcl_jni_new_bytes(env, payload, size) : NULL;
    zcl_secure_zero(payload, sizeof(payload));
    return output;
}

/* Public adapter record: type byte and 20 hash bytes. Not chain serialization.
 * No native pointers or handles. Consumers validate every record again. */
JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_parseAddress(JNIEnv *env, jclass type,
                                                     jbyteArray input, jint chain)
{
    (void)type;
    uint8_t text[35] = {0}, record[21] = {0};
    size_t length = 0;
    zcl_network network;
    zcl_address address = {0};
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, text, sizeof(text), &length) != ZCL_OK)
        return NULL;
    if (zcl_address_parse(text, length, network, &address) != ZCL_OK)
        return NULL;
    record[0] = (uint8_t)address.kind;
    memcpy(record + 1, address.hash, sizeof(address.hash));
    return zcl_jni_new_bytes(env, record, sizeof(record));
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_addressScript(JNIEnv *env, jclass type,
                                                      jbyteArray input, jint chain)
{
    (void)type;
    uint8_t record[21] = {0}, script[25] = {0};
    size_t size = 0, length = 0;
    zcl_address address = {0};
    if (zcl_jni_network(chain, &address.network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, record, sizeof(record), &size) != ZCL_OK || size != sizeof(record))
        return NULL;
    address.kind = (zcl_address_kind)record[0];
    memcpy(address.hash, record + 1, sizeof(address.hash));
    if (zcl_address_script(&address, script, sizeof(script), &length) != ZCL_OK)
        return NULL;
    return zcl_jni_new_bytes(env, script, length);
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_addressFromHash(JNIEnv *env, jclass type,
                                                        jbyteArray input, jint chain)
{
    (void)type;
    uint8_t hash[20] = {0}, text[35] = {0};
    size_t size = 0, length = 0;
    zcl_network network;
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, hash, sizeof(hash), &size) != ZCL_OK)
        return NULL;
    if (zcl_address_from_hash(hash, size, network, text, sizeof(text), &length) != ZCL_OK)
        return NULL;
    return zcl_jni_new_bytes(env, text, length);
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_encodeAddress(JNIEnv *env, jclass type,
                                                      jbyteArray input, jint chain)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
    uint8_t record[21] = {0}, text[35] = {0};
    size_t size = 0, length = 0;
    zcl_address address = {0};
    if (zcl_jni_network(chain, &address.network) != ZCL_OK)
        return NULL;
    if (zcl_jni_read_bytes(env, input, record, sizeof(record), &size) != ZCL_OK || size != sizeof(record))
        return NULL;
    if (record[0] != (uint8_t)ZCL_P2PKH && record[0] != (uint8_t)ZCL_P2SH) return NULL;
    address.kind = (zcl_address_kind)record[0];
    memcpy(address.hash, record + 1, sizeof(address.hash));
    if (zcl_address_encode(&address, text, sizeof(text), &length) != ZCL_OK)
        return NULL;
    return zcl_jni_new_bytes(env, text, length);
}
