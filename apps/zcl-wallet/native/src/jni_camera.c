/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_camera.h"
#include "zcl_keys.h"
#include <stdlib.h>

_Static_assert(ZCL_CAMERA_PACKET_MAX <= INT32_MAX, "Camera packets fit JNI lengths");

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
                                      const zcl_qr_image *layout, size_t *packet_size)
{
    if (buffer == NULL || length < 0)
        return NULL;
    if (zcl_camera_frame_size((size_t)length, layout, packet_size) != ZCL_OK)
        return NULL;
    const jlong capacity = (*env)->GetDirectBufferCapacity(env, buffer);
    if ((*env)->ExceptionCheck(env) || !direct_range(capacity, offset, length))
        return NULL;
    const uint8_t *pixels = (*env)->GetDirectBufferAddress(env, buffer);
    if ((*env)->ExceptionCheck(env) || pixels == NULL)
        return NULL;
    return pixels + (size_t)offset;
}

static jint pack_pixels(JNIEnv *env, const uint8_t *pixels, size_t length,
                         const zcl_qr_image *layout, size_t packet_size, jbyteArray output)
{
    /* C sizing proves 446..147461 bytes. This invocation alone owns and clears
     * exactly that allocation; layout and borrowed pixels stay stable. */
    uint8_t *packet = malloc(packet_size);
    if (packet == NULL)
        return 0;
    jint written = 0;
    size_t packet_len = 0;
    if (zcl_camera_frame_pack(pixels, length, layout, packet, packet_size, &packet_len) == ZCL_OK &&
        packet_len == packet_size) {
        (*env)->SetByteArrayRegion(env, output, 0, (jsize)packet_len, (const jbyte *)packet);
        if (!(*env)->ExceptionCheck(env)) written = (jint)packet_len;
    }
    zcl_secure_zero(packet, packet_size);
    free(packet);
    return written;
}

/* No image bytes are read or copied while sizing. The managed caller allocates
 * the exact destination before packing, and owns its cleanup even if a VM
 * output transfer partially writes before throwing. Packing rechecks all input
 * bounds; no pointer or layout is retained across these separate invocations. */
JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_cameraPlanePacketSize(JNIEnv *env, jclass type, jobject buffer,
    jint offset, jint length, jint width, jint height, jint row, jint pixel)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return 0;
    zcl_qr_image layout = {0};
    if (!camera_layout(width, height, row, pixel, &layout)) return 0;
    size_t packet_size = 0;
    if (direct_pixels(env, buffer, offset, length, &layout, &packet_size) == NULL) return 0;
    return (jint)packet_size;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_packCameraPlane(JNIEnv *env, jclass type, jobject buffer,
                                                        jint offset, jint length, jint width,
                                                        jint height, jint row, jint pixel, jbyteArray output)
{
    (void)type;
    if (env == NULL || output == NULL || (*env)->ExceptionCheck(env))
        return 0;
    zcl_qr_image layout = {0};
    if (!camera_layout(width, height, row, pixel, &layout))
        return 0;
    size_t packet_size = 0;
    const uint8_t *pixels = direct_pixels(env, buffer, offset, length, &layout, &packet_size);
    if (pixels == NULL)
        return 0;
    const jsize capacity = (*env)->GetArrayLength(env, output);
    if ((*env)->ExceptionCheck(env) || capacity != (jsize)packet_size) return 0;
    /* The Android caller keeps the Image and direct ByteBuffer alive until
     * this synchronous copy returns. No pointer or buffer reference escapes. */
    return pack_pixels(env, pixels, (size_t)length, &layout, packet_size, output);
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
