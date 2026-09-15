/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Only the JNI translation units use the fault-injected allocator. */
#undef malloc
#undef free
#include "jni_support.h"
#include "zcl_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "JNI camera check failed at %d\n", __LINE__); abort(); } } while (0)
#define API(name) Java_org_zclassic_wallet_core_NativeCore_##name
JNIEXPORT jbyteArray JNICALL API(packCameraPlane)(JNIEnv *, jclass, jobject, jint, jint, jint, jint, jint, jint);
JNIEXPORT jbyteArray JNICALL API(scanCameraPacket)(JNIEnv *, jclass, jbyteArray, jint);
JNIEXPORT jbyteArray JNICALL API(scanQr)(JNIEnv *, jclass, jbyteArray, jint, jint, jint, jint, jint);

typedef enum { PACK, PACKET, SCAN } operation;
typedef struct { jsize length; size_t capacity; uint8_t *bytes; } fake_array;
/* Single-threaded public fixtures only. Production retains no VM references. */
static uint8_t image[256 * 256], packet[ZCL_CAMERA_PACKET_MAX];
static size_t side, image_length, packet_length;
static const uint8_t address[] = "t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF";
static struct { uint64_t before; uint8_t bytes[ZCL_CAMERA_PACKET_MAX]; uint64_t after; } result_box;
static fake_array input, result;
static jlong direct_capacity;
static bool pending, allocation_failure;
static unsigned fault, calls, allocations;
static uint8_t *owned;
static size_t owned_length, allocation_bytes;

void *zcl_jni_camera_test_malloc(size_t size);
void zcl_jni_camera_test_free(void *pointer);

void *zcl_jni_camera_test_malloc(size_t size)
{
    CHECK(owned == NULL && size > 0 && size <= 8 * 1024 * 1024);
    ++allocations;
    allocation_bytes = size;
    if (allocation_failure) return NULL;
    owned = malloc(size);
    CHECK(owned != NULL);
    owned_length = size;
    memset(owned, 0x5a, size);
    return owned;
}

void zcl_jni_camera_test_free(void *pointer)
{
    CHECK(pointer != NULL && pointer == owned);
    for (size_t i = 0; i < owned_length; ++i) CHECK(owned[i] == 0);
    free(owned);
    owned = NULL;
    owned_length = 0;
}

static bool vm_failure(void)
{
    CHECK(!pending);
    ++calls;
    if (calls != fault) return false;
    pending = true;
    return true;
}

static jboolean JNICALL exception_check(JNIEnv *env)
{
    (void)env;
    return pending ? JNI_TRUE : JNI_FALSE;
}

static jsize JNICALL array_length(JNIEnv *env, jarray array)
{
    (void)env;
    CHECK(array == (jarray)&input);
    return vm_failure() ? 0 : input.length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&input && offset == 0 && count >= 0);
    CHECK((size_t)count <= input.capacity && bytes != NULL);
    if (vm_failure()) {
        if (count > 0) bytes[0] = 42; /* Partial VM read must still clear. */
        return;
    }
    memcpy(bytes, input.bytes, (size_t)count);
}

static jlong JNICALL buffer_capacity(JNIEnv *env, jobject buffer)
{
    (void)env;
    CHECK(buffer == (jobject)&input);
    return vm_failure() ? -1 : direct_capacity;
}

static void *JNICALL buffer_address(JNIEnv *env, jobject buffer)
{
    (void)env;
    CHECK(buffer == (jobject)&input);
    return vm_failure() ? NULL : input.bytes;
}

static jbyteArray JNICALL new_bytes(JNIEnv *env, jsize count)
{
    (void)env;
    CHECK(count > 0 && (size_t)count <= result.capacity);
    if (vm_failure()) return NULL;
    result.length = count;
    return (jbyteArray)&result;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&result && offset == 0 && count == result.length);
    CHECK(count > 0 && (size_t)count <= result.capacity && bytes != NULL);
    if (vm_failure()) { result.bytes[0] = (uint8_t)bytes[0]; return; }
    memcpy(result.bytes, bytes, (size_t)count);
}

static const struct JNINativeInterface_ table = {
    .ExceptionCheck = exception_check, .GetArrayLength = array_length,
    .GetByteArrayRegion = get_bytes, .NewByteArray = new_bytes, .SetByteArrayRegion = set_bytes,
    .GetDirectBufferCapacity = buffer_capacity, .GetDirectBufferAddress = buffer_address
};
static JNIEnv environment = &table;

static void initialize(void)
{
    uint8_t modules[ZCL_RECEIVE_QR_MODULES_MAX];
    size_t symbol = 0;
    CHECK(zcl_receive_qr(address, 35, ZCL_MAINNET, modules, sizeof(modules), &symbol) == ZCL_OK);
    CHECK(symbol <= 41);
    side = (symbol + 8) * 3;
    image_length = side * side;
    CHECK(image_length <= sizeof(image));
    memset(image, 255, sizeof(image));
    for (size_t y = 0; y < symbol; ++y) for (size_t x = 0; x < symbol; ++x) {
        for (size_t dy = 0; dy < 3; ++dy) for (size_t dx = 0; dx < 3; ++dx)
            image[((y + 4) * 3 + dy) * side + (x + 4) * 3 + dx] = modules[y * symbol + x] != 0 ? 0 : 255;
    }
    const zcl_qr_image layout = {side, side, side, 1};
    CHECK(zcl_camera_frame_pack(image, image_length, &layout, packet, sizeof(packet), &packet_length) == ZCL_OK);
}

static void reset(operation op, unsigned selected_fault)
{
    CHECK(owned == NULL);
    pending = false; allocation_failure = false;
    fault = selected_fault; calls = 0; allocations = 0; allocation_bytes = 0;
    input = op == PACKET ? (fake_array){(jsize)packet_length, sizeof(packet), packet}
                        : (fake_array){(jsize)image_length, sizeof(image), image};
    direct_capacity = (jlong)image_length;
    memset(&result_box, 0xa5, sizeof(result_box));
    result = (fake_array){0, sizeof(result_box.bytes), result_box.bytes};
}

static jbyteArray dispatch(operation op, JNIEnv *env, jobject object)
{
    if (op == PACK)
        return API(packCameraPlane)(env, NULL, object, 0, (jint)image_length,
            (jint)side, (jint)side, (jint)side, 1);
    if (op == PACKET) return API(scanCameraPacket)(env, NULL, (jbyteArray)object, 0);
    return API(scanQr)(env, NULL, (jbyteArray)object, (jint)side, (jint)side, (jint)side, 1, 0);
}

static jbyteArray invoke(operation op, JNIEnv *env, jobject object)
{
    static uint8_t before[ZCL_CAMERA_PACKET_MAX]; /* Public single-threaded scratch. */
    CHECK(input.capacity <= sizeof(before));
    memcpy(before, input.bytes, input.capacity);
    const jbyteArray output = dispatch(op, env, object);
    CHECK(memcmp(before, input.bytes, input.capacity) == 0);
    return output;
}

static void check_success(operation op, jbyteArray output)
{
    CHECK(output == (jbyteArray)&result && !pending);
    if (op == PACK) {
        CHECK((size_t)result.length == packet_length);
        CHECK(memcmp(result.bytes, packet, packet_length) == 0);
    } else if (op == PACKET) {
        CHECK(result.length == 35 && memcmp(result.bytes, address, 35) == 0);
    } else {
        CHECK(result.length == 49 && result.bytes[0] == 1 && result.bytes[1] == 0);
        CHECK(memcmp(result.bytes + 2, address, 35) == 0);
        for (size_t i = 37; i < 49; ++i) CHECK(result.bytes[i] == 0);
    }
    for (size_t i = (size_t)result.length; i < result.capacity; ++i) CHECK(result.bytes[i] == 0xa5);
}

static void exception_and_allocation_faults(operation op)
{
    const unsigned vm_calls = op == PACK ? 4 : 5;
    for (unsigned selected = 0; selected <= vm_calls; ++selected) {
        reset(op, selected);
        const jbyteArray output = invoke(op, &environment, (jobject)&input);
        if (selected == 0) check_success(op, output);
        else CHECK(output == NULL && pending && calls == selected);
        CHECK(owned == NULL && allocations <= 1);
        CHECK(result_box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && result_box.after == result_box.before);
    }
    reset(op, 0);
    allocation_failure = true;
    CHECK(invoke(op, &environment, (jobject)&input) == NULL);
    CHECK(allocations == 1 && owned == NULL && !pending);
}

static void invalid_entries(operation op)
{
    reset(op, 0);
    pending = true;
    CHECK(invoke(op, &environment, (jobject)&input) == NULL);
    CHECK(pending && calls == 0 && allocations == 0);
    pending = false;
    CHECK(invoke(op, NULL, (jobject)&input) == NULL);
    CHECK(invoke(op, &environment, NULL) == NULL);
    CHECK(calls == 0 && allocations == 0);
}

static void invalid_lengths(void)
{
    const jsize lengths[] = {-1, 0, 4, INT32_MAX};
    for (operation op = PACKET; op <= SCAN; ++op) {
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            reset(op, 0);
            input.length = lengths[i];
            CHECK(invoke(op, &environment, (jobject)&input) == NULL);
            CHECK(allocations == 0 && calls == 1 && !pending);
        }
    }
    const jlong capacities[] = {-1, 0, 440};
    for (size_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i) {
        reset(PACK, 0);
        direct_capacity = capacities[i];
        CHECK(invoke(PACK, &environment, (jobject)&input) == NULL);
        CHECK(allocations == 0 && calls == 1 && !pending);
    }
}

static void exact_pack_allocations(void)
{
    static uint8_t pixels[1024 * 1024]; /* Public fixture pixels, never an application buffer. */
    static const struct { jint width, height; size_t sampled_width, sampled_height; } layouts[] = {
        {640, 480, 320, 240}, {480, 640, 240, 320}, {320, 240, 320, 240},
        {240, 240, 240, 240}, {384, 384, 384, 384}, {385, 385, 193, 193},
        {1024, 1024, 342, 342}, {21, 21, 21, 21}
    };
    memset(pixels, 93, sizeof(pixels));
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        reset(PACK, 0);
        const jint width = layouts[i].width, height = layouts[i].height;
        const jint length = width * height; /* Both are positive and at most 1024. */
        const size_t expected = 5 + layouts[i].sampled_width * layouts[i].sampled_height;
        input = (fake_array){length, sizeof(pixels), pixels};
        direct_capacity = sizeof(pixels);
        CHECK(API(packCameraPlane)(&environment, NULL, (jobject)&input, 0, length,
            width, height, width, 1) == (jbyteArray)&result);
        printf("JNI camera %dx%d: allocation=%zu packet=%zu\n", width, height, allocation_bytes, expected);
        CHECK(allocations == 1 && allocation_bytes == expected && owned == NULL);
        CHECK(calls == 4 && !pending);
        CHECK((size_t)result.length == expected && result.bytes[0] == 1);
        CHECK((size_t)result.bytes[1] + (size_t)result.bytes[2] * 256 == layouts[i].sampled_width);
        CHECK((size_t)result.bytes[3] + (size_t)result.bytes[4] * 256 == layouts[i].sampled_height);
        for (size_t j = 5; j < result.capacity; ++j) CHECK(result.bytes[j] == (j < expected ? 93 : 0xa5));
    }
    for (size_t i = 0; i < sizeof(pixels); ++i) CHECK(pixels[i] == 93);
    reset(PACK, 0);
    input = (fake_array){21 * 1024, sizeof(pixels), pixels};
    direct_capacity = sizeof(pixels);
    CHECK(API(packCameraPlane)(&environment, NULL, (jobject)&input, 0, input.length,
        21, 1024, 21, 1) == NULL); /* Valid input dimensions, unsupported sampled width. */
    CHECK(calls == 0 && allocations == 0 && owned == NULL && !pending);
}

int main(void)
{
    initialize();
    for (operation op = PACK; op <= SCAN; ++op) {
        invalid_entries(op);
        exception_and_allocation_faults(op);
    }
    invalid_lengths();
    exact_pack_allocations();
    puts("JNI camera/scan pending exceptions, exact public results and cleared allocations passed");
    return 0;
}
