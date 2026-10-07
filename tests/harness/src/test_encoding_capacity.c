/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Text encoders must stay inside the caller's advertised output capacity. */
#include "test/test_core.h"
#include "encoding/utilstrencodings.h"

typedef size_t (*capacity_encoder)(const unsigned char *, size_t,
                                  char *, size_t);

/* The output starts at storage[1]. Both surrounding regions remain canaries;
 * a zero-capacity call owns no byte, including no terminator byte. */
static bool capacity_output_matches(const unsigned char storage[32],
                                    size_t capacity, const char *expected,
                                    size_t written)
{
    size_t count = strlen(expected);
    if (capacity == 0)
        count = 0;
    else if (count >= capacity)
        count = capacity - 1;
    if (written != count || storage[0] != 0x5a)
        return false;
    if (memcmp(storage + 1, expected, count) != 0)
        return false;
    size_t untouched = 1;
    if (capacity != 0) {
        if (storage[1 + count] != '\0')
            return false;
        untouched = 2 + count;
    }
    for (size_t i = untouched; i < 32; i++) {
        if (storage[i] != 0x5a)
            return false;
    }
    return true;
}

static bool capacity_encoded_case(capacity_encoder encode, const char *input,
                                  const char *expected)
{
    unsigned char storage[32];
    size_t length = strlen(expected);
    if (length > sizeof(storage) - 4)
        return false;
    for (size_t capacity = 0; capacity <= length + 2; capacity++) {
        memset(storage, 0x5a, sizeof(storage));
        size_t written = encode((const unsigned char *)input, strlen(input),
                                (char *)storage + 1, capacity);
        if (!capacity_output_matches(storage, capacity, expected, written))
            return false;
    }
    return true;
}

static int capacity_encoder_test(const char *label, capacity_encoder encode,
                                 const char *single, const char *hello)
{
    int failures = 0;
    TEST(label) {
        ASSERT(capacity_encoded_case(encode, "", ""));
        ASSERT(capacity_encoded_case(encode, "f", single));
        ASSERT(capacity_encoded_case(encode, "Hello", hello));
        ASSERT(encode((const unsigned char *)"Hello", 5, NULL, 0) == 0);
        PASS();
    } _test_next:;
    return failures;
}

static bool capacity_sanitized_case(int rule, const char *expected)
{
    unsigned char storage[32];
    size_t length = strlen(expected);
    if (length > sizeof(storage) - 4)
        return false;
    for (size_t capacity = 0; capacity <= length + 2; capacity++) {
        memset(storage, 0x5a, sizeof(storage));
        SanitizeString("f<>o/o", rule, (char *)storage + 1, capacity);
        size_t written = strnlen((char *)storage + 1, capacity);
        if (!capacity_output_matches(storage, capacity, expected, written))
            return false;
    }
    return true;
}

static int capacity_sanitizer_test(void)
{
    int failures = 0;
    TEST("SanitizeString: output capacity and canaries") {
        ASSERT(capacity_sanitized_case(SAFE_CHARS_DEFAULT, "fo/o"));
        ASSERT(capacity_sanitized_case(SAFE_CHARS_UA_COMMENT, "foo"));
        SanitizeString("Hello", SAFE_CHARS_DEFAULT, NULL, 0);
        PASS();
    } _test_next:;
    return failures;
}

int test_encoding_capacity(void);

int test_encoding_capacity(void)
{
    int failures = capacity_encoder_test("EncodeBase64: output capacity and canaries",
                                         EncodeBase64, "Zg==", "SGVsbG8=");
    failures += capacity_encoder_test("EncodeBase32: output capacity and canaries",
                                      EncodeBase32, "my======", "jbswy3dp");
    failures += capacity_sanitizer_test();
    return failures;
}
