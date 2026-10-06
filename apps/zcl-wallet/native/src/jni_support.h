/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_JNI_SUPPORT_H
#define ZCL_JNI_SUPPORT_H
#include "zcl_wallet.h"
#include <jni.h>
/* No pins, retained references, native heap allocation or handles. Caller
 * owns output; Java references are local to the JNI invocation. Secret-bearing
 * callers must clear native scratch and arrange managed-array cleanup.
 * Pending exceptions refuse before any array access/allocation and remain
 * pending. A failed region read can leave partial bytes in caller scratch;
 * length publishes only on success. Clear secret scratch on every exit. */
zcl_status zcl_jni_read_bytes(JNIEnv *env, jbyteArray input, uint8_t *bytes,
                              size_t capacity, size_t *length);
zcl_status zcl_jni_read_exact_bytes(JNIEnv *env, jbyteArray input, uint8_t *bytes,
                                    size_t length);
/* Public results only. Secret outputs use managed-owned destinations whose
 * finally cleanup is established before JNI, including a failed region copy. */
jbyteArray zcl_jni_new_bytes(JNIEnv *env, const uint8_t *bytes, size_t length);
zcl_status zcl_jni_network(jint value, zcl_network *network);
jbyteArray zcl_jni_payment_record(JNIEnv *env, const zcl_payment_request *request);
#endif
