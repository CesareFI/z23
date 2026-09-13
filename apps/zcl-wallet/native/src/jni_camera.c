/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_camera.h"
#include "zcl_keys.h"
#include <stdlib.h>

static bool camera_layout(jint width, jint height, jint row, jint pixel, zcl_qr_image *layout)
{
    if (width < 0 || height < 0 || row < 0 || pixel < 0)
        return false;
    *layout = (zcl_qr_image){(size_t)width, (size_t)height, (size_t)row, (size_t)pixel};
    return true;
}

static bool direct_range(jlong capacity, jint offset, jint length)
{
    return capacity >= 0 && offset >= 0 && length >= 0 &&
           (jlong)offset <= capacity && (jlong)length <= capacity - (jlong)offset;
}

static const uint8_t *direct_pixels(JNIEnv *env, jobject buffer, jint offset, jint length,
                                      const zcl_qr_image *layout)
{
    if (buffer == NULL || length < 0)
        return NULL;
    if (zcl_scan_image_bounds((size_t)length, layout) != ZCL_OK)
        return NULL;
    const jlong capacity = (*env)->GetDirectBufferCapacity(env, buffer);
    if ((*env)->ExceptionCheck(env) || !direct_range(capacity, offset, length))
        return NULL;
    const uint8_t *pixels = (*env)->GetDirectBufferAddress(env, buffer);
    if ((*env)->ExceptionCheck(env) || pixels == NULL)
        return NULL;
    return pixels + (size_t)offset;
}

static jbyteArray pack_pixels(JNIEnv *env, const uint8_t *pixels, size_t length,
                                const zcl_qr_image *layout)
{
    /* Fixed, checked allocation. This invocation alone owns and clears it. */
    uint8_t *packet = malloc(ZCL_CAMERA_PACKET_MAX);
    if (packet == NULL)
        return NULL;
    jbyteArray output = NULL;
    size_t packet_len = 0;
    if (zcl_camera_frame_pack(pixels, length, layout, packet, ZCL_CAMERA_PACKET_MAX, &packet_len) == ZCL_OK)
        output = zcl_jni_new_bytes(env, packet, packet_len);
    zcl_secure_zero(packet, ZCL_CAMERA_PACKET_MAX);
    free(packet);
    return output;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_packCameraPlane(JNIEnv *env, jclass type, jobject buffer,
                                                        jint offset, jint length, jint width,
                                                        jint height, jint row, jint pixel)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env))
        return NULL;
    zcl_qr_image layout = {0};
    if (!camera_layout(width, height, row, pixel, &layout))
        return NULL;
    const uint8_t *pixels = direct_pixels(env, buffer, offset, length, &layout);
    if (pixels == NULL)
        return NULL;
    /* The Android caller keeps the Image and direct ByteBuffer alive until
     * this synchronous copy returns. No pointer or buffer reference escapes. */
    return pack_pixels(env, pixels, (size_t)length, &layout);
}

static jbyteArray decode_packet(JNIEnv *env, jbyteArray input, size_t length, zcl_network network)
{
    /* length is already 5..147461. One checked owner, one cleanup path. */
    uint8_t *packet = malloc(length);
    if (packet == NULL)
        return NULL;
    jbyteArray output = NULL;
    size_t copied = 0;
    zcl_scanned_request decoded = {0};
    if (zcl_jni_read_bytes(env, input, packet, length, &copied) == ZCL_OK) {
        if (zcl_camera_packet_scan(packet, copied, network, &decoded) == ZCL_OK)
            output = zcl_jni_new_bytes(env, decoded.text, decoded.text_len);
    }
    zcl_secure_zero(&decoded, sizeof(decoded));
    zcl_secure_zero(packet, length);
    free(packet);
    return output;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_scanCameraPacket(JNIEnv *env, jclass type,
                                                          jbyteArray input, jint chain)
{
    (void)type;
    if (env == NULL || input == NULL)
        return NULL;
    if ((*env)->ExceptionCheck(env))
        return NULL;
    zcl_network network;
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    const jsize length = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env))
        return NULL;
    if (length < 5 || (size_t)length > ZCL_CAMERA_PACKET_MAX)
        return NULL;
    return decode_packet(env, input, (size_t)length, network);
}
