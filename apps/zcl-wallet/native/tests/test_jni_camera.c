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
JNIEXPORT jint JNICALL API(cameraPlanePacketSize)(JNIEnv *, jclass, jobject, jint, jint, jint, jint, jint, jint);
JNIEXPORT jint JNICALL API(packCameraPlane)(JNIEnv *, jclass, jobject, jint, jint, jint, jint, jint, jint, jbyteArray);
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
static unsigned fault, calls, allocations, new_arrays;
static uint8_t *owned;
static size_t owned_length, allocation_bytes, failure_prefix, read_failure_prefix;
static size_t byte_reads;

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
    CHECK(array == (jarray)&input || array == (jarray)&result);
    return vm_failure() ? 0 : ((const fake_array *)array)->length;
}

static void JNICALL get_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&input && offset == 0 && count >= 0);
    CHECK(count <= input.length && (size_t)count <= input.capacity && bytes != NULL);
    byte_reads += (size_t)count;
    if (vm_failure()) {
        const size_t prefix = (size_t)count < read_failure_prefix ? (size_t)count : read_failure_prefix;
        memcpy(bytes, input.bytes, prefix); /* Partial VM read must still clear. */
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
    /* Decoding has consumed its native image before allocating a public VM
     * result. The allocator observer separately verifies erasure before free. */
    CHECK(owned == NULL);
    ++new_arrays;
    if (vm_failure()) return NULL;
    result.length = count;
    return (jbyteArray)&result;
}

static void JNICALL set_bytes(JNIEnv *env, jbyteArray array, jsize offset, jsize count, const jbyte *bytes)
{
    (void)env;
    CHECK(array == (jbyteArray)&result && offset == 0 && count == result.length);
    CHECK(count > 0 && (size_t)count <= result.capacity && bytes != NULL);
    if (vm_failure()) {
        const size_t prefix = (size_t)count < failure_prefix ? (size_t)count : failure_prefix;
        memcpy(result.bytes, bytes, prefix);
        return;
    }
    memcpy(result.bytes, bytes, (size_t)count);
}

#if defined(__ANDROID__)
typedef struct JNINativeInterface camera_jni_interface;
#else
typedef struct JNINativeInterface_ camera_jni_interface;
#endif
static const camera_jni_interface table = {
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
    fault = selected_fault; calls = 0; allocations = 0; allocation_bytes = 0; new_arrays = 0;
    byte_reads = 0;
    failure_prefix = 6;
    read_failure_prefix = 1;
    input = op == PACKET ? (fake_array){(jsize)packet_length, sizeof(packet), packet}
                        : (fake_array){(jsize)image_length, sizeof(image), image};
    direct_capacity = (jlong)image_length;
    memset(&result_box, 0xa5, sizeof(result_box));
    result = (fake_array){op == PACK ? (jsize)packet_length : 0, sizeof(result_box.bytes), result_box.bytes};
}

static jbyteArray dispatch(operation op, JNIEnv *env, jobject object)
{
    if (op == PACK) {
        const jint written = API(packCameraPlane)(env, NULL, object, 0, (jint)image_length,
            (jint)side, (jint)side, (jint)side, 1, (jbyteArray)&result);
        CHECK(written == 0 || written == result.length);
        return written == 0 ? NULL : (jbyteArray)&result;
    }
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

static void padded_scan(void)
{
    static uint8_t padded[ZCL_SCAN_INPUT_MAX + 1]; /* Public, fixed bounded backing. */
    memset(padded, 0x5a, sizeof(padded));
    memcpy(padded, image, image_length);
    reset(SCAN, 0);
    input = (fake_array){(jsize)ZCL_SCAN_INPUT_MAX, sizeof(padded), padded};
    check_success(SCAN, dispatch(SCAN, &environment, (jobject)&input));
    printf("Padded JNI scan: input=%d allocated=%zu copied=%zu used=%zu\n",
        input.length, allocation_bytes, byte_reads, image_length);
    fflush(stdout);
    CHECK(allocation_bytes == image_length && byte_reads == image_length && owned == NULL);
    CHECK(memcmp(padded, image, image_length) == 0);
    for (size_t i = image_length; i < sizeof(padded); ++i) CHECK(padded[i] == 0x5a);
    const jsize invalid[] = {(jsize)image_length - 1, (jsize)sizeof(padded)};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        reset(SCAN, 0);
        input = (fake_array){invalid[i], sizeof(padded), padded};
        CHECK(dispatch(SCAN, &environment, (jobject)&input) == NULL);
        CHECK(!pending && allocations == 0 && byte_reads == 0 && owned == NULL);
    }
}

static void exception_and_allocation_faults(operation op)
{
    const unsigned vm_calls = op == PACKET ? 5 : 4;
    for (unsigned selected = 0; selected <= vm_calls; ++selected) {
        reset(op, selected);
        const jbyteArray output = invoke(op, &environment, (jobject)&input);
        if (selected == 0) check_success(op, output);
        else CHECK(output == NULL && pending && calls == selected);
        CHECK(owned == NULL && allocations <= 1);
        if (op == PACK) CHECK(new_arrays == 0);
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

static void check_sizing(jint width, jint height, jint length, size_t expected)
{
    CHECK(API(cameraPlanePacketSize)(&environment, NULL, (jobject)&input, 0, length,
        width, height, width, 1) == (jint)expected);
    CHECK(calls == 2 && allocations == 0 && new_arrays == 0 && !pending);
    calls = 0;
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
        check_sizing(width, height, length, expected);
        result.length = (jsize)expected;
        CHECK(API(packCameraPlane)(&environment, NULL, (jobject)&input, 0, length,
            width, height, width, 1, (jbyteArray)&result) == (jint)expected);
        printf("JNI camera %dx%d: allocation=%zu packet=%zu\n", width, height, allocation_bytes, expected);
        CHECK(allocations == 1 && allocation_bytes == expected && owned == NULL);
        CHECK(calls == 4 && new_arrays == 0 && !pending);
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
        21, 1024, 21, 1, (jbyteArray)&result) == 0); /* Unsupported sampled width. */
    CHECK(calls == 0 && allocations == 0 && owned == NULL && !pending);
}

static jint size_packet(JNIEnv *env, jobject buffer)
{
    return API(cameraPlanePacketSize)(env, NULL, buffer, 0, (jint)image_length,
        (jint)side, (jint)side, (jint)side, 1);
}

static void sizing_refusals(void)
{
    reset(PACK, 0);
    CHECK(size_packet(NULL, (jobject)&input) == 0);
    CHECK(size_packet(&environment, NULL) == 0);
    pending = true;
    CHECK(size_packet(&environment, (jobject)&input) == 0);
    CHECK(calls == 0 && allocations == 0 && new_arrays == 0 && pending);
    for (unsigned selected = 1; selected <= 2; ++selected) {
        reset(PACK, selected);
        CHECK(size_packet(&environment, (jobject)&input) == 0);
        CHECK(calls == selected && pending && allocations == 0 && new_arrays == 0);
    }
    reset(PACK, 0);
    direct_capacity = -1;
    CHECK(size_packet(&environment, (jobject)&input) == 0);
    CHECK(calls == 1 && !pending && allocations == 0 && new_arrays == 0);
}

static void destination_refusals(void)
{
    const jsize sizes[] = {-1, 0, 5, (jsize)packet_length - 1, (jsize)packet_length + 1, INT32_MAX};
    reset(PACK, 0);
    CHECK(API(packCameraPlane)(&environment, NULL, (jobject)&input, 0, (jint)image_length,
        (jint)side, (jint)side, (jint)side, 1, NULL) == 0);
    CHECK(calls == 0 && allocations == 0 && !pending);
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        reset(PACK, 0);
        result.length = sizes[i];
        CHECK(invoke(PACK, &environment, (jobject)&input) == NULL);
        CHECK(calls == 3 && allocations == 0 && new_arrays == 0 && !pending);
        for (size_t j = 0; j < result.capacity; ++j) CHECK(result.bytes[j] == 0xa5);
    }
}

/* Single-threaded fuzz fixtures: the advertised direct capacity never exceeds
 * real backing storage. A fake VM must not manufacture an out-of-bounds source
 * by lying about its allocation. No image or reference escapes an invocation. */
typedef struct {
    jint offset, length, width, height, row, pixel;
    size_t backing;
    jlong capacity;
} fuzz_plane;
static uint8_t fuzz_pixels[1024 * 1024 + 32], fuzz_expected[ZCL_CAMERA_PACKET_MAX];

static jint boundary_value(uint8_t selector, jint normal)
{
    CHECK(normal >= 0 && normal < INT32_MAX);
    const jint values[] = {normal, normal, normal, normal, -1, INT32_MIN, INT32_MAX,
        0, normal - 1, normal + 1};
    return values[selector % (sizeof(values) / sizeof(values[0]))];
}

static fuzz_plane prepare_fuzz_plane(const uint8_t data[13])
{
    static const jint layouts[][2] = {{21, 21}, {640, 480}, {384, 384}, {385, 385},
        {480, 640}, {320, 240}, {1024, 1024}, {21, 1024}};
    const size_t selected = data[0] % (sizeof(layouts) / sizeof(layouts[0]));
    const jint width = layouts[selected][0], height = layouts[selected][1];
    const jint length = width * height; /* Fixed table proves <= 1048576. */
    const size_t backing = (size_t)length + 32;
    CHECK(backing <= sizeof(fuzz_pixels));
    const jlong capacities[] = {(jlong)backing, -1, 0, 10, (jlong)length + 10, (jlong)backing - 1};
    const fuzz_plane plane = {boundary_value(data[1], 11), boundary_value(data[2], length),
        boundary_value(data[3], width), boundary_value(data[4], height),
        boundary_value(data[5], width), boundary_value(data[6], 1), backing,
        capacities[data[8] % (sizeof(capacities) / sizeof(capacities[0]))]};
    memset(fuzz_pixels, data[11], backing);
    return plane;
}

static void bind_fuzz_plane(const fuzz_plane *plane)
{
    input = (fake_array){(jsize)plane->backing, plane->backing, fuzz_pixels};
    direct_capacity = plane->capacity;
}

static jint expected_fuzz_packet(const fuzz_plane *plane)
{
    reset(PACK, 0);
    bind_fuzz_plane(plane);
    const jint expected = API(cameraPlanePacketSize)(&environment, NULL, (jobject)&input,
        plane->offset, plane->length, plane->width, plane->height, plane->row, plane->pixel);
    CHECK(calls <= 2 && allocations == 0 && new_arrays == 0 && !pending);
    if (expected == 0) return 0;
    CHECK(expected >= 446 && expected <= (jint)sizeof(fuzz_expected));
    CHECK(plane->offset >= 0 && plane->length >= 0 && plane->width >= 0 && plane->height >= 0);
    CHECK(plane->row >= 0 && plane->pixel >= 0);
    CHECK((size_t)plane->offset <= plane->backing);
    CHECK((size_t)plane->length <= plane->backing - (size_t)plane->offset);
    const zcl_qr_image layout = {(size_t)plane->width, (size_t)plane->height,
        (size_t)plane->row, (size_t)plane->pixel};
    size_t written = 0;
    /* C packing is the JNI transport oracle. Independent pixel/reference
     * checks remain in the existing C camera tests and C camera fuzzer. */
    CHECK(zcl_camera_frame_pack(fuzz_pixels + (size_t)plane->offset, (size_t)plane->length,
        &layout, fuzz_expected, sizeof(fuzz_expected), &written) == ZCL_OK);
    CHECK(written == (size_t)expected);
    return expected;
}

static void check_fuzz_result(const fuzz_plane *plane, jint expected, jint written, uint8_t marker)
{
    CHECK(calls <= 4 && allocations <= 1 && new_arrays == 0 && owned == NULL);
    CHECK(allocations == 0 || allocation_bytes == (size_t)expected);
    size_t touched = 0;
    if (written != 0) {
        CHECK(!pending && written == expected && written == result.length && written > 0);
        touched = (size_t)written;
        CHECK(memcmp(result.bytes, fuzz_expected, touched) == 0);
    } else if (pending && calls == 4) {
        CHECK(result.length > 0);
        touched = failure_prefix < (size_t)result.length ? failure_prefix : (size_t)result.length;
        CHECK(memcmp(result.bytes, fuzz_expected, touched) == 0);
    }
    for (size_t i = touched; i < result.capacity; ++i) CHECK(result.bytes[i] == 0xa5);
    for (size_t i = 0; i < plane->backing; ++i) CHECK(fuzz_pixels[i] == marker);
    CHECK(result_box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && result_box.after == result_box.before);
}

static void fuzz_camera_case(const uint8_t data[13])
{
    const fuzz_plane plane = prepare_fuzz_plane(data);
    const jint expected = expected_fuzz_packet(&plane);
    reset(PACK, data[9] % 5U);
    bind_fuzz_plane(&plane);
    result.length = boundary_value(data[7], expected);
    const size_t prefixes[] = {0, 1, 5, 6, (size_t)expected / 2, (size_t)expected};
    failure_prefix = prefixes[data[12] % (sizeof(prefixes) / sizeof(prefixes[0]))];
    allocation_failure = (data[10] & 1U) != 0;
    pending = (data[10] & 2U) != 0;
    const jint written = API(packCameraPlane)((data[10] & 4U) != 0 ? NULL : &environment,
        NULL, (data[10] & 8U) != 0 ? NULL : (jobject)&input,
        plane.offset, plane.length, plane.width, plane.height, plane.row, plane.pixel,
        (data[10] & 16U) != 0 ? NULL : (jbyteArray)&result);
    check_fuzz_result(&plane, expected, written, data[11]);
    if (result.length == expected && fault == 0 && (data[10] & 31U) == 0) CHECK(written == expected);
}

static void fuzz_scan_case(const uint8_t data[13])
{
    const fuzz_plane plane = prepare_fuzz_plane(data);
    reset(SCAN, data[9] % 5U);
    bind_fuzz_plane(&plane);
    allocation_failure = (data[10] & 1U) != 0;
    const zcl_qr_image layout = {(size_t)plane.width, (size_t)plane.height,
        (size_t)plane.row, (size_t)plane.pixel};
    const zcl_status bounds = zcl_scan_image_bounds(plane.backing, &layout);
    size_t needed = 0;
    if (bounds == ZCL_OK)
        needed = (layout.height - 1) * layout.row_stride + (layout.width - 1) * layout.pixel_stride + 1;
    read_failure_prefix = plane.backing / 2;
    const jbyteArray output = API(scanQr)(&environment, NULL, (jbyteArray)&input,
        plane.width, plane.height, plane.row, plane.pixel, 0);
    /* Uniform public pixels contain no QR code, even for an admitted layout. */
    CHECK(output == NULL && new_arrays == 0 && owned == NULL && calls <= 2);
    CHECK(allocations == 0 || (bounds == ZCL_OK && allocation_bytes == needed));
    CHECK(byte_reads == 0 || byte_reads == needed);
    for (size_t i = 0; i < plane.backing; ++i) CHECK(fuzz_pixels[i] == data[11]);
}

/* Four controls followed by actual packet bytes. The fake array advertises
 * only its real backing span. This C decoder is a transport/result oracle;
 * independent pixel/QR behavior remains covered by the C camera/QR fuzzers. */
static uint8_t fuzz_decode_pixels[ZCL_CAMERA_PACKET_MAX];

static size_t transfer_prefix(uint8_t selector, size_t length)
{
    CHECK(length <= ZCL_CAMERA_PACKET_MAX);
    const size_t values[] = {0, 1, 5, length / 2, length, length + 1};
    return values[selector % (sizeof(values) / sizeof(values[0]))];
}

static void check_decode_result(jbyteArray output, zcl_status status, const zcl_scanned_request *expected)
{
    static uint8_t expected_output[ZCL_CAMERA_PACKET_MAX]; /* Single-threaded guard oracle. */
    CHECK(calls <= 5 && allocations <= 1 && new_arrays <= 1 && owned == NULL);
    CHECK(allocations == 0 || allocation_bytes == input.capacity);
    size_t touched = 0;
    if (output != NULL) {
        CHECK(output == (jbyteArray)&result && !pending && status == ZCL_OK);
        CHECK(result.length > 0 && (size_t)result.length == expected->text_len);
        touched = expected->text_len;
    } else if (pending && calls == 5) {
        CHECK(status == ZCL_OK && (size_t)result.length == expected->text_len);
        touched = failure_prefix < expected->text_len ? failure_prefix : expected->text_len;
    }
    CHECK(touched <= ZCL_PAYMENT_TEXT_MAX);
    CHECK(result.capacity == sizeof(expected_output));
    memset(expected_output, 0xa5, sizeof(expected_output));
    memcpy(expected_output, expected->text, touched);
    CHECK(memcmp(result.bytes, expected_output, sizeof(expected_output)) == 0);
    CHECK(result_box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && result_box.after == result_box.before);
}

static void fuzz_decode_case(const uint8_t *data, size_t size)
{
    CHECK(size >= 4 && size <= sizeof(fuzz_decode_pixels) + 4);
    const size_t length = size - 4;
    static const jint chains[] = {0, 1, -1, INT32_MIN, INT32_MAX};
    const jint chain = chains[data[0] % (sizeof(chains) / sizeof(chains[0]))];
    memcpy(fuzz_decode_pixels, data + 4, length);
    zcl_scanned_request expected = {0};
    zcl_status status = ZCL_UNSUPPORTED;
    if (chain == 0 || chain == 1)
        status = zcl_camera_packet_scan(fuzz_decode_pixels, length,
            chain == 0 ? ZCL_MAINNET : ZCL_TESTNET, &expected);
    reset(PACKET, data[1] % 6U);
    input = (fake_array){(jsize)length, length, fuzz_decode_pixels};
    allocation_failure = (data[2] & 1U) != 0;
    pending = (data[2] & 2U) != 0;
    read_failure_prefix = transfer_prefix(data[3], length);
    failure_prefix = transfer_prefix(data[3], expected.text_len);
    const jbyteArray output = API(scanCameraPacket)((data[2] & 4U) != 0 ? NULL : &environment,
        NULL, (data[2] & 8U) != 0 ? NULL : (jbyteArray)&input, chain);
    CHECK(memcmp(fuzz_decode_pixels, data + 4, length) == 0);
    check_decode_result(output, status, &expected);
    if (status != ZCL_OK || fault != 0 || (data[2] & 15U) != 0) CHECK(output == NULL);
    else CHECK(output != NULL);
}

#if !defined(ZCL_JNI_CAMERA_FUZZ)
/* Keep the fixed matrix in the registered test, with its suite deadline.
 * Fuzzing exercises each packet/fault combination as a separate timed input;
 * do not charge an entire new regression matrix to the first empty input. */
static void decoder_regressions(void)
{
    static uint8_t seed[ZCL_CAMERA_PACKET_MAX + 4];
    static const uint8_t values[] = {0, 1, 2, 3, 4, 5, 7, 15, 31, 255};
    CHECK(packet_length >= 5 && packet_length <= sizeof(seed) - 4);
    memcpy(seed + 4, packet, packet_length);
    for (size_t field = 0; field < 4; ++field) for (size_t i = 0; i < sizeof(values); ++i) {
        memset(seed, 0, 4);
        seed[field] = values[i];
        fuzz_decode_case(seed, packet_length + 4);
    }
    for (unsigned selected = 3; selected <= 5; selected += 2) {
        for (uint8_t prefix = 0; prefix < 6; ++prefix) {
            memset(seed, 0, 4);
            seed[1] = (uint8_t)selected;
            seed[3] = prefix;
            fuzz_decode_case(seed, packet_length + 4);
        }
    }
    memset(seed, 0, 4);
    const size_t lengths[] = {0, 1, 4, 5, packet_length - 1, packet_length + 1, ZCL_CAMERA_PACKET_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
        fuzz_decode_case(seed, lengths[i] + 4);
    for (size_t i = 0; i < 5; ++i) {
        const uint8_t original = seed[i + 4];
        seed[i + 4] = 0xff;
        fuzz_decode_case(seed, packet_length + 4);
        seed[i + 4] = original;
    }
}
#endif

static void regressions(void)
{
    initialize();
    padded_scan();
    sizing_refusals();
    destination_refusals();
    for (operation op = PACK; op <= SCAN; ++op) {
        invalid_entries(op);
        exception_and_allocation_faults(op);
    }
    invalid_lengths();
    exact_pack_allocations();
}

#if defined(ZCL_JNI_CAMERA_FUZZ)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static bool initialized;
    if (!initialized) { regressions(); initialized = true; }
    if (size == 13) { fuzz_camera_case(data); fuzz_scan_case(data); }
    else if (size >= 4 && size <= ZCL_CAMERA_PACKET_MAX + 4) fuzz_decode_case(data, size);
    return 0;
}
#else
int main(void)
{
    regressions();
    decoder_regressions();
    static const uint8_t values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 15, 16, 31, 32, 63, 64, 127, 128, 255};
    uint8_t data[13] = {0};
    for (size_t field = 0; field < sizeof(data); ++field) {
        for (size_t value = 0; value < sizeof(values); ++value) {
            memset(data, 0, sizeof(data));
            data[field] = values[value];
            fuzz_camera_case(data);
            fuzz_scan_case(data);
        }
    }
    puts("JNI camera/scan pending exceptions, exact public results and cleared allocations passed");
    return 0;
}
#endif
