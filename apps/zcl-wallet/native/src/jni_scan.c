/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_qr.h"
#include "zcl_keys.h"
#include <stdlib.h>

static zcl_status read_layout(JNIEnv *env, jbyteArray input, jint width, jint height,
                               jint row_stride, jint pixel_stride, zcl_qr_image *layout,
                               size_t *length)
{
    if (width < 0 || height < 0 || row_stride < 0 || pixel_stride < 0)
        return ZCL_OUT_OF_RANGE;
    const jsize count = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env) || count < 0)
        return ZCL_INVALID_ARGUMENT;
    *layout = (zcl_qr_image){(size_t)width, (size_t)height, (size_t)row_stride, (size_t)pixel_stride};
    *length = (size_t)count;
    return zcl_scan_image_bounds(*length, layout);
}

static jbyteArray scan_copy(JNIEnv *env, jbyteArray input, size_t length,
                             const zcl_qr_image *layout, zcl_network network)
{
    /* length was validated in C: 441..8 MiB. One owner, one cleanup path.
     * No managed-array pin, retained reference or native handle. */
    uint8_t *image = malloc(length);
    if (image == NULL)
        return NULL;
    size_t copied = 0;
    zcl_payment_request request = {0};
    zcl_status status = zcl_jni_read_bytes(env, input, image, length, &copied);
    if (status == ZCL_OK) status = zcl_scan_qr(image, copied, layout, network, &request);
    /* Decoding has consumed the pixels. Retire this native copy before the
     * public result needs a VM allocation or transfer. */
    zcl_secure_zero(image, length);
    free(image);
    jbyteArray result = status == ZCL_OK ? zcl_jni_payment_record(env, &request) : NULL;
    zcl_secure_zero(&request, sizeof(request));
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_scanQr(JNIEnv *env, jclass type, jbyteArray input,
                                                jint width, jint height, jint row_stride,
                                                jint pixel_stride, jint chain)
{
    (void)type;
    if (env == NULL || input == NULL)
        return NULL;
    if ((*env)->ExceptionCheck(env))
        return NULL;
    zcl_network network;
    if (zcl_jni_network(chain, &network) != ZCL_OK)
        return NULL;
    zcl_qr_image layout = {0};
    size_t length = 0;
    if (read_layout(env, input, width, height, row_stride, pixel_stride, &layout, &length) != ZCL_OK)
        return NULL;
    return scan_copy(env, input, length, &layout, network);
}
