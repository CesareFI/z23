/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "jni_support.h"
#include "zcl_sync_owners.h"
#include "zcl_keys.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

static jlong open_owner(JNIEnv *env, jbyteArray address_input, jint chain,
    jbyteArray source_input, bool include_history)
{
    uint8_t address[35] = {0}, source[32] = {0};
    size_t address_length = 0, source_length = 0;
    zcl_network network;
    zcl_status status = zcl_jni_network(chain, &network);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, address_input, address, sizeof(address), &address_length);
    if (status == ZCL_OK)
        status = zcl_jni_read_bytes(env, source_input, source, sizeof(source), &source_length);
    uint64_t id = 0;
    bool locked = false;
    if (status == ZCL_OK) {
        locked = pthread_mutex_lock(&registry_lock) == 0;
        if (!locked) status = ZCL_IO_FAILURE;
        else status = include_history
            ? zcl_sync_owners_open_with_history(&registry, address, address_length, network, source, source_length, &id)
            : zcl_sync_owners_open(&registry, address, address_length, network, source, source_length, &id);
    }
    zcl_secure_zero(address, sizeof(address));
    zcl_secure_zero(source, sizeof(source));
    if (locked) status = unlock_registry(status);
    return status == ZCL_OK ? (jlong)id : -(jlong)status;
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_openSyncOwner(JNIEnv *env, jclass type,
    jbyteArray address, jint chain, jbyteArray source)
{
    (void)type;
    return open_owner(env, address, chain, source, false);
}

JNIEXPORT jlong JNICALL
Java_org_zclassic_wallet_core_NativeCore_openHistorySyncOwner(JNIEnv *env, jclass type,
    jbyteArray address, jint chain, jbyteArray source)
{
    (void)type;
    return open_owner(env, address, chain, source, true);
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
    (void)type;
    /* Refusal must not start an attempt or consume its sequence. */
    if (env == NULL || (*env)->ExceptionCheck(env)) return -(jlong)ZCL_INVALID_ARGUMENT;
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
    zcl_secure_zero(&next, sizeof(next));
    zcl_secure_zero(packet, sizeof(packet));
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncRequest(JNIEnv *env, jclass type,
    jlong id, jlong token, jlong now)
{
    (void)type;
    /* Refusal must not consume a request or poison this owner. */
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
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

static zcl_status reply_length(JNIEnv *env, jbyteArray input, size_t *length)
{
    if (input == NULL) return ZCL_INVALID_ARGUMENT;
    const jsize count = (*env)->GetArrayLength(env, input);
    if ((*env)->ExceptionCheck(env)) return ZCL_INVALID_ARGUMENT;
    if (count < 0 || (size_t)count > ZCL_ELECTRUM_FRAME_MAX) return ZCL_OUT_OF_RANGE;
    *length = (size_t)count;
    return ZCL_OK;
}

static zcl_status reply_frame(JNIEnv *env, zcl_sync_watch *watch, uint64_t token,
    uint64_t now, jbyteArray input, size_t length)
{
    /* Java array lengths are immutable. Own and retire only the admitted frame
     * span; an empty frame needs no heap owner but follows the same parser. */
    uint8_t empty = 0;
    uint8_t *frame = length == 0 ? &empty : malloc(length);
    if (frame == NULL) return zcl_sync_watch_fail(watch, token, ZCL_RESOURCE_EXHAUSTED);
    size_t copied = 0;
    zcl_status status = zcl_jni_read_bytes(env, input, frame, length, &copied);
    if (status == ZCL_OK) status = zcl_sync_watch_reply(watch, token, now, frame, copied);
    else status = zcl_sync_watch_fail(watch, token, status);
    if (length != 0) {
        zcl_secure_zero(frame, length);
        free(frame);
    }
    return status;
}

JNIEXPORT jint JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncReply(JNIEnv *env, jclass type,
    jlong id, jlong token, jlong now, jbyteArray input)
{
    (void)type;
    /* Refusal must not allocate/copy a frame or fail this attempt. */
    if (env == NULL || (*env)->ExceptionCheck(env)) return (jint)ZCL_INVALID_ARGUMENT;
    if (token <= 0) return (jint)ZCL_CANCELLED;
    if (now < 0) return (jint)ZCL_OUT_OF_RANGE;
    zcl_sync_watch *watch = NULL;
    zcl_status status = enter_owner(id, &watch);
    if (status != ZCL_OK) return (jint)status;
    /* The registry lock keeps this token decision stable until reply_frame.
     * Reuse C's check before allocating/copying an already retired reply. */
    status = zcl_sync_watch_check_attempt(watch, (uint64_t)token);
    size_t length = 0;
    if (status == ZCL_OK) {
        status = reply_length(env, input, &length);
        status = status == ZCL_OK
            ? reply_frame(env, watch, (uint64_t)token, (uint64_t)now, input, length)
            : zcl_sync_watch_fail(watch, (uint64_t)token, status);
    }
    return (jint)unlock_registry(status);
}

static zcl_status snapshot_values(const zcl_sync_snapshot *snapshot, jlong values[10])
{
    if (snapshot->age_ms > INT64_MAX || snapshot->report.balance.confirmed > INT64_MAX ||
        snapshot->report.balance.total > INT64_MAX || snapshot->next_change_ms > INT64_MAX)
        return ZCL_OUT_OF_RANGE;
    values[1] = (jlong)snapshot->freshness;
    values[2] = snapshot->refreshing ? 1 : 0;
    values[3] = (jlong)snapshot->last_fault;
    values[4] = (jlong)snapshot->age_ms;
    values[5] = (jlong)snapshot->report.balance.confirmed;
    values[6] = (jlong)snapshot->report.balance.pending_delta;
    values[7] = (jlong)snapshot->report.balance.total;
    values[8] = (jlong)snapshot->report.tip.height;
    values[9] = (jlong)snapshot->next_change_ms;
    return ZCL_OK;
}

static zcl_status snapshot_numbers(zcl_sync_watch *watch, uint64_t now, jlong values[10])
{
    zcl_sync_snapshot snapshot = {0};
    zcl_status status = zcl_sync_watch_snapshot(watch, now, &snapshot);
    if (status == ZCL_OK) status = snapshot_values(&snapshot, values);
    zcl_secure_zero(&snapshot, sizeof(snapshot));
    return status;
}

#define HISTORY_PACKET_MAX ((size_t)12 + 9 * ZCL_ELECTRUM_HISTORY_MAX)
_Static_assert(HISTORY_PACKET_MAX <= INT32_MAX, "History packets fit jsize");

/* Eight big-endian uint32 words avoid implementation-defined uint64-to-signed
 * conversions. Each Java long carries exactly 0..UINT32_MAX, followed by height.
 * Caller supplies nine elements for one validated C entry. */
static void history_entry_values(const zcl_reported_history_entry *entry, jlong values[9])
{
    for (size_t word = 0; word < 8; ++word) {
        uint32_t value = 0;
        for (size_t byte = 0; byte < 4; ++byte)
            value = (value << 8) | entry->txid[word * 4 + byte];
        values[word] = (jlong)value;
    }
    values[8] = (jlong)entry->reported_height;
}

static zcl_status history_values(const zcl_sync_snapshot *snapshot,
    jlong values[HISTORY_PACKET_MAX], size_t *length)
{
    const zcl_status status = snapshot_values(snapshot, values);
    if (status != ZCL_OK) return status;
    const zcl_reported_history *history = &snapshot->report.history;
    if (history->count > ZCL_ELECTRUM_HISTORY_MAX) return ZCL_OUT_OF_RANGE;
    if (!snapshot->report.has_history && history->count != 0) return ZCL_INVALID_ENCODING;
    values[10] = snapshot->report.has_history ? 1 : 0;
    values[11] = (jlong)history->count;
    for (size_t i = 0; i < history->count; ++i)
        history_entry_values(&history->entries[i], &values[12 + i * 9]);
    *length = 12 + history->count * 9; /* count <=16 proves length <=156. */
    return ZCL_OK;
}

static zcl_status history_snapshot_numbers(zcl_sync_watch *watch, uint64_t now,
    jlong values[HISTORY_PACKET_MAX], size_t *length)
{
    zcl_sync_snapshot snapshot = {0};
    zcl_status status = zcl_sync_watch_snapshot(watch, now, &snapshot);
    if (status == ZCL_OK) status = history_values(&snapshot, values, length);
    zcl_secure_zero(&snapshot, sizeof(snapshot));
    return status;
}

static jlongArray new_snapshot_numbers(JNIEnv *env, const jlong *values, size_t length)
{
    if (length > HISTORY_PACKET_MAX) return NULL;
    jlongArray result = (*env)->NewLongArray(env, (jsize)length);
    if (result == NULL || (*env)->ExceptionCheck(env)) return NULL;
    (*env)->SetLongArrayRegion(env, result, 0, (jsize)length, values);
    return (*env)->ExceptionCheck(env) ? NULL : result;
}

JNIEXPORT jlongArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncHistorySnapshot(JNIEnv *env, jclass type,
    jlong id, jlong now)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
    jlong values[HISTORY_PACKET_MAX] = {0};
    size_t length = 12;
    zcl_sync_watch *watch = NULL;
    zcl_status status = now < 0 ? ZCL_OUT_OF_RANGE : enter_owner(id, &watch);
    if (status == ZCL_OK)
        status = unlock_registry(history_snapshot_numbers(watch, (uint64_t)now, values, &length));
    if (status != ZCL_OK) {
        memset(values, 0, sizeof(values));
        length = 12;
    }
    values[0] = (jlong)status;
    jlongArray result = new_snapshot_numbers(env, values, length);
    zcl_secure_zero(values, sizeof(values));
    return result;
}

JNIEXPORT jlongArray JNICALL
Java_org_zclassic_wallet_core_NativeCore_syncSnapshot(JNIEnv *env, jclass type, jlong id, jlong now)
{
    (void)type;
    if (env == NULL || (*env)->ExceptionCheck(env)) return NULL;
    jlong values[10] = {0};
    zcl_sync_watch *watch = NULL;
    zcl_status status = now < 0 ? ZCL_OUT_OF_RANGE : enter_owner(id, &watch);
    if (status == ZCL_OK) status = unlock_registry(snapshot_numbers(watch, (uint64_t)now, values));
    if (status != ZCL_OK) memset(values, 0, sizeof(values));
    values[0] = (jlong)status;
    jlongArray result = new_snapshot_numbers(env, values, 10);
    zcl_secure_zero(values, sizeof(values));
    return result;
}
