/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "domain/encoding/base58.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Independent long division reference: divide the original base-256 payload
 * repeatedly, rather than multiplying a base-58 accumulator like production. */
static size_t reference_encode(const unsigned char *input, size_t length, char *text)
{
    static const char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    unsigned char work[1024];
    char reverse[1414];
    memcpy(work, input, length);
    size_t zeroes = 0, count = 0;
    while (zeroes < length && work[zeroes] == 0) ++zeroes;
    size_t start = zeroes;
    while (start < length) {
        unsigned carry = 0;
        for (size_t i = start; i < length; ++i) {
            carry = carry * 256 + work[i];
            work[i] = (unsigned char)(carry / 58);
            carry %= 58;
        }
        reverse[count++] = alphabet[carry];
        while (start < length && work[start] == 0) ++start;
    }
    memset(text, '1', zeroes);
    for (size_t i = 0; i < count; ++i) text[zeroes + i] = reverse[count - 1 - i];
    text[zeroes + count] = '\0';
    return zeroes + count;
}

static bool encode_case(const unsigned char *input, size_t length, const char *expected, size_t size)
{
    char text[1415];
    memset(text, '?', sizeof(text));
    size_t written = 777;
    if (!domain_encoding_base58_encode(input, length, text, size + 1, &written) ||
        written != size || strcmp(text, expected) != 0 || text[size + 1] != '?') return false;
    memset(text, '?', sizeof(text));
    if (domain_encoding_base58_encode(input, length, text, size, &written) || written != size) return false;
    for (size_t i = 0; i < sizeof(text); ++i) if (text[i] != '?') return false;
    return true;
}

static bool decode_case(const unsigned char *input, size_t length, const char *text)
{
    unsigned char decoded[1025];
    memset(decoded, 0xa5, sizeof(decoded));
    size_t written = 777;
    if (!domain_encoding_base58_decode(text, decoded, length, &written) ||
        written != length || memcmp(decoded, input, length) != 0 || decoded[length] != 0xa5) return false;
    if (length == 0) return true;
    memset(decoded, 0xa5, sizeof(decoded));
    if (domain_encoding_base58_decode(text, decoded, length - 1, &written) || written != length) return false;
    for (size_t i = 0; i < sizeof(decoded); ++i) if (decoded[i] != 0xa5) return false;
    return true;
}

static bool span_case(size_t length, size_t zeroes, unsigned char fill)
{
    unsigned char input[1024], saved[1024];
    char expected[1415];
    memset(input, fill, sizeof(input));
    memset(input, 0, zeroes);
    memcpy(saved, input, sizeof(saved));
    size_t size = reference_encode(input, length, expected);
    bool okay = encode_case(input, length, expected, size);
    /* Decoder's existing 1023-char cap applies after leading ones. */
    if (size - zeroes <= 1023) okay = decode_case(input, length, expected) && okay;
    return okay && memcmp(input, saved, sizeof(input)) == 0;
}

int base58_span_cases(void);
int base58_span_cases(void)
{
    bool okay = true;
    for (size_t length = 0; length <= 128; ++length) {
        for (size_t zeroes = 0; zeroes <= length; ++zeroes) {
            okay = span_case(length, zeroes, 1) && okay;
            okay = span_case(length, zeroes, 255) && okay;
        }
    }
    const size_t boundaries[] = {256, 512, 747, 748, 1023, 1024};
    for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
        okay = span_case(boundaries[i], 0, 255) && okay;
        okay = span_case(boundaries[i], boundaries[i] - 1, 1) && okay;
        okay = span_case(boundaries[i], boundaries[i], 0) && okay;
    }
    printf("domain_encoding_base58: significant span reference/capacity corpus... %s\n", okay ? "OK" : "FAIL");
    return okay ? 0 : 1;
}
