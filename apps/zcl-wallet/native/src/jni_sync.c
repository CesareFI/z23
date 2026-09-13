/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_sync_owners.h"
#include "zcl_keys.h"
#include <pthread.h>
#include <stdlib.h>

/* The JNI adapter explicitly owns this one bounded process registry. Its four
 * slots contain PUBLIC sync data only, no keys, sockets or Java references.
 * The mutex covers lookup, every borrowed-watch use and close. No C pointer is
 * exposed as a Java long; only never-reused IDs leave this translation unit.
 * An enclosing Java owner must close explicitly; exhaustion fails closed.
 */
static zcl_sync_owners registry = {0};
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
_Static_assert(ZCL_SYNC_OWNER_ID_MAX <= INT64_MAX, "Owner IDs fit jlong");
_Static_assert(ZCL_TLS_FAILURE <= UINT8_MAX, "JNI statuses fit a byte");

static zcl_status unlock_registry(zcl_status status)
{
    return pthread_mutex_unlock(&registry_lock) == 0 ? status : ZCL_IO_UNCERTAIN;
}

/* Success lends a watch with the lock HELD. All other paths leave no borrow. */
static zcl_status enter_owner(jlong id, zcl_sync_watch **watch)
{
    if (id <= 0) return ZCL_CANCELLED;
    if (pthread_mutex_lock(&registry_lock) != 0) return ZCL_IO_FAILURE;
    const zcl_status status = zcl_sync_owners_get(&registry, (uint64_t)id, watch);
    return status == ZCL_OK ? ZCL_OK : unlock_registry(status);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_openSyncOwner(JNIEnv *env, jclass type,
    jbyteArray address_input, jint chain, jbyteArray source_input)
{
    (void)type;
    uint8_t address[35] = {0}, source[32] = {0};
    size_t address_length = 0, source_length = 0;
    zcl_network network;
    zcl_status status = zcl_jni_network(chain, &network);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, address_input, address, sizeof(address), &address_length);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, source_input, source, sizeof(source), &source_length);
    if (status != ZCL_OK) return -(jlong)status;
    if (pthread_mutex_lock(&registry_lock) != 0) return -(jlong)ZCL_IO_FAILURE;
    uint64_t id = 0;
    status = zcl_sync_owners_open(&registry, address, address_length, network, source, source_length, &id);
    status = unlock_registry(status);
    return status == ZCL_OK ? (jlong)id : -(jlong)status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_closeSyncOwner(JNIEnv *env, jclass type, jlong id)
{
    (void)env; (void)type;
    if (id <= 0) return (jint)ZCL_CANCELLED;
    if (pthread_mutex_lock(&registry_lock) != 0) return (jint)ZCL_IO_FAILURE;
    return (jint)unlock_registry(zcl_sync_owners_close(&registry, (uint64_t)id));
}

static bool begin_numbers(jlong now, jlong timeout, jlong first_id)
{
    if (now < 0 || timeout <= 0 || timeout > (jlong)ZCL_SYNC_TIMEOUT_MAX_MS) return false;
    if (now > INT64_MAX - timeout) return false;
    return first_id > 0 && first_id <= (jlong)(UINT32_MAX - 5);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_beginSyncAttempt(JNIEnv *env, jclass type,
    jlong id, jlong now, jlong timeout, jlong first_id)
{
    (void)env; (void)type;
    if (!begin_numbers(now, timeout, first_id)) return -(jlong)ZCL_OUT_OF_RANGE;
    zcl_sync_watch *watch = NULL;
    zcl_status status = enter_owner(id, &watch);
    if (status != ZCL_OK) return -(jlong)status;
    uint64_t token = 0;
    if (watch->sequence >= ZCL_SYNC_OWNER_ID_MAX) status = ZCL_RESOURCE_EXHAUSTED;
    else status = zcl_sync_watch_begin(watch, (uint64_t)now, (uint64_t)timeout, (uint32_t)first_id, &token);
    status = unlock_registry(status);
    return status == ZCL_OK ? (jlong)token : -(jlong)status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_failSyncAttempt(JNIEnv *env, jclass type,
    jlong id, jlong token, jint reason)
{
    (void)env; (void)type;
    if (token <= 0) return (jint)ZCL_CANCELLED;
    if (reason <= (jint)ZCL_OK || reason > (jint)ZCL_TLS_FAILURE) return (jint)ZCL_INVALID_ARGUMENT;
    zcl_sync_watch *watch = NULL;
    zcl_status status = enter_owner(id, &watch);
    if (status != ZCL_OK) return (jint)status;
    status = zcl_sync_watch_fail(watch, (uint64_t)token, (zcl_status)reason);
    return (jint)unlock_registry(status);
}

static jbyteArray request_packet(JNIEnv *env, zcl_sync_watch *watch, uint64_t token, uint64_t now)
{
    uint8_t packet[ZCL_ELECTRUM_REQUEST_MAX + 1] = {0};
    size_t length = 0;
    /* Publish the new waiting/clock state only after Java accepts the packet. */
    zcl_sync_watch next = *watch;
    const zcl_status status = zcl_sync_watch_request(&next, token, now,
        packet + 1, sizeof(packet) - 1, &length);
    packet[0] = (uint8_t)status;
    jbyteArray result = zcl_jni_new_bytes(env, packet, status == ZCL_OK ? length + 1 : 1);
    if (result != NULL) *watch = next;
    else (void)zcl_sync_watch_fail(watch, token, ZCL_RESOURCE_EXHAUSTED);
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncRequest(JNIEnv *env, jclass type,
    jlong id, jlong token, jlong now)
{
    (void)type;
    zcl_status status = ZCL_OUT_OF_RANGE;
    zcl_sync_watch *watch = NULL;
    if (token > 0 && now >= 0) status = enter_owner(id, &watch);
    if (status != ZCL_OK) {
        const uint8_t error = (uint8_t)status;
        return zcl_jni_new_bytes(env, &error, 1);
    }
    jbyteArray packet = request_packet(env, watch, (uint64_t)token, (uint64_t)now);
    return unlock_registry(ZCL_OK) == ZCL_OK ? packet : NULL;
}

static zcl_status reply_frame(JNIEnv *env, zcl_sync_watch *watch, uint64_t token,
    uint64_t now, jbyteArray input)
{
    /* One checked, fixed-capacity owner; never place a network frame on stack. */
    uint8_t *frame = malloc(ZCL_ELECTRUM_FRAME_MAX);
    if (frame == NULL) return zcl_sync_watch_fail(watch, token, ZCL_RESOURCE_EXHAUSTED);
    size_t length = 0;
    zcl_status status = zcl_jni_read_bytes(env, input, frame, ZCL_ELECTRUM_FRAME_MAX, &length);
    if (status == ZCL_OK) status = zcl_sync_watch_reply(watch, token, now, frame, length);
    else status = zcl_sync_watch_fail(watch, token, status);
    zcl_secure_zero(frame, ZCL_ELECTRUM_FRAME_MAX);
    free(frame);
    return status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncReply(JNIEnv *env, jclass type,
    jlong id, jlong token, jlong now, jbyteArray input)
{
    (void)type;
    if (token <= 0) return (jint)ZCL_CANCELLED;
    if (now < 0) return (jint)ZCL_OUT_OF_RANGE;
    zcl_sync_watch *watch = NULL;
    zcl_status status = enter_owner(id, &watch);
    if (status != ZCL_OK) return (jint)status;
    status = reply_frame(env, watch, (uint64_t)token, (uint64_t)now, input);
    return (jint)unlock_registry(status);
}

static zcl_status snapshot_numbers(zcl_sync_watch *watch, uint64_t now, jlong values[9])
{
    zcl_sync_snapshot snapshot = {0};
    const zcl_status status = zcl_sync_watch_snapshot(watch, now, &snapshot);
    if (status != ZCL_OK) return status;
    if (snapshot.age_ms > INT64_MAX || snapshot.report.balance.confirmed > INT64_MAX ||
        snapshot.report.balance.total > INT64_MAX) return ZCL_OUT_OF_RANGE;
    values[1] = (jlong)snapshot.freshness;
    values[2] = snapshot.refreshing ? 1 : 0;
    values[3] = (jlong)snapshot.last_fault;
    values[4] = (jlong)snapshot.age_ms;
    values[5] = (jlong)snapshot.report.balance.confirmed;
    values[6] = (jlong)snapshot.report.balance.pending_delta;
    values[7] = (jlong)snapshot.report.balance.total;
    values[8] = (jlong)snapshot.report.tip.height;
    return ZCL_OK;
}

JNIEXPORT jlongArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncSnapshot(JNIEnv *env, jclass type, jlong id, jlong now)
{
    (void)type;
    jlong values[9] = {0};
    zcl_sync_watch *watch = NULL;
    zcl_status status = now < 0 ? ZCL_OUT_OF_RANGE : enter_owner(id, &watch);
    if (status == ZCL_OK) status = unlock_registry(snapshot_numbers(watch, (uint64_t)now, values));
    values[0] = (jlong)status;
    jlongArray result = (*env)->NewLongArray(env, 9);
    if (result == NULL || (*env)->ExceptionCheck(env)) return NULL;
    (*env)->SetLongArrayRegion(env, result, 0, 9, values);
    return (*env)->ExceptionCheck(env) ? NULL : result;
}
