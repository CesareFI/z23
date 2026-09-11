/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "mnemonic_words.h"

#include <string.h>

typedef struct { uint8_t length; uint8_t text[8]; } mnemonic_word;
static const mnemonic_word english[2048] = {
#include "english.inc"
};

bool zcl_entropy_length_valid(size_t length)
{
    return length >= 16 && length <= 32 && length % 4 == 0;
}

/* All private helpers receive validated non-NULL spans. */
static zcl_status word_index(const uint8_t *bits, size_t bits_len,
                             size_t offset, uint16_t *index)
{
    if (bits_len != 33 || offset > 253)
        return ZCL_OUT_OF_RANGE;
    uint16_t value = 0;
    for (size_t bit = offset; bit < offset + 11; ++bit) {
        unsigned shift = 7U - (unsigned)(bit % 8);
        unsigned next = ((unsigned)bits[bit / 8] >> shift) & 1U;
        value = (uint16_t)(((unsigned)value << 1) | next);
    }
    *index = value;
    return ZCL_OK;
}

static zcl_status append_word(uint16_t index, uint8_t *text, size_t capacity,
                              size_t *used)
{
    if (index >= 2048 || *used > capacity)
        return ZCL_OUT_OF_RANGE;
    const mnemonic_word *word = &english[index];
    size_t separator = *used == 0 ? 0 : 1;
    size_t needed = (size_t)word->length + separator;
    if (needed > capacity - *used)
        return ZCL_BUFFER_TOO_SMALL;
    if (separator != 0)
        text[(*used)++] = ' ';
    memcpy(text + *used, word->text, word->length);
    *used += word->length;
    return ZCL_OK;
}

static zcl_status validate_word_output(const uint8_t *bits, size_t word_count,
                                       const uint8_t *text, const size_t *text_len)
{
    if (bits == NULL || text == NULL || text_len == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (word_count < 12 || word_count > 24 || word_count % 3 != 0)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

zcl_status zcl_words_write(const uint8_t *bits, size_t bits_len, size_t word_count,
                           uint8_t *text, size_t text_capacity, size_t *text_len)
{
    /* Internal scratch-writing API: caller owns and clears partially written
     * text on error. Exported mnemonic APIs stage before publishing outputs. */
    zcl_status status = validate_word_output(bits, word_count, text, text_len);
    if (status != ZCL_OK)
        return status;
    size_t used = 0;
    uint16_t index = 0;
    for (size_t word = 0; word < word_count; ++word) {
        status = word_index(bits, bits_len, word * 11, &index);
        if (status != ZCL_OK)
            break;
        status = append_word(index, text, text_capacity, &used);
        if (status != ZCL_OK)
            break;
    }
    if (status == ZCL_OK)
        *text_len = used;
    zcl_secure_zero(&index, sizeof(index));
    return status;
}

static int compare_word(const uint8_t *text, size_t length, const mnemonic_word *word)
{
    size_t common = length < word->length ? length : word->length;
    int compared = memcmp(text, word->text, common);
    if (compared != 0)
        return compared;
    if (length == word->length)
        return 0;
    return length < word->length ? -1 : 1;
}

static zcl_status find_word(const uint8_t *text, size_t length, uint16_t *index)
{
    if (length < 3 || length > 8)
        return ZCL_INVALID_ENCODING;
    size_t low = 0, high = 2048;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int compared = compare_word(text, length, &english[middle]);
        if (compared == 0) {
            *index = (uint16_t)middle;
            return ZCL_OK;
        }
        if (compared < 0)
            high = middle;
        else
            low = middle + 1;
    }
    return ZCL_INVALID_ENCODING;
}

static zcl_status insert_word(uint16_t index, size_t word,
                              uint8_t *bits, size_t capacity)
{
    if (index >= 2048 || word >= 24 || capacity < 33)
        return ZCL_OUT_OF_RANGE;
    for (size_t i = 0; i < 11; ++i) {
        size_t bit = word * 11 + i;
        unsigned value = ((unsigned)index >> (10U - (unsigned)i)) & 1U;
        bits[bit / 8] |= (uint8_t)(value << (7U - (unsigned)(bit % 8)));
    }
    return ZCL_OK;
}

static zcl_status read_word(const uint8_t *text, size_t length, size_t word,
                            uint8_t *bits, size_t capacity, size_t *consumed)
{
    size_t size = 0;
    uint16_t index = 0;
    while (size < length && text[size] != ' ' && size <= 8)
        ++size;
    zcl_status status = find_word(text, size, &index);
    if (status == ZCL_OK)
        status = insert_word(index, word, bits, capacity);
    if (status == ZCL_OK)
        *consumed = size;
    zcl_secure_zero(&index, sizeof(index));
    return status;
}

static zcl_status validate_word_input(const uint8_t *text, size_t text_len,
                                      const uint8_t *bits, size_t bits_capacity,
                                      const size_t *word_count)
{
    if (text == NULL || bits == NULL || word_count == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (text_len == 0 || text_len > ZCL_MNEMONIC_MAX || bits_capacity < 33)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

zcl_status zcl_words_read(const uint8_t *text, size_t text_len,
                          uint8_t *bits, size_t bits_capacity, size_t *word_count)
{
    /* Internal: bits is zeroed scratch and is cleared by the caller on error. */
    zcl_status bounds = validate_word_input(text, text_len, bits, bits_capacity, word_count);
    if (bounds != ZCL_OK)
        return bounds;
    size_t offset = 0, count = 0;
    while (offset < text_len) {
        size_t consumed = 0;
        zcl_status status = read_word(text + offset, text_len - offset, count,
                                      bits, bits_capacity, &consumed);
        if (status != ZCL_OK)
            return status;
        ++count;
        offset += consumed;
        if (offset < text_len)
            ++offset;
    }
    if (text[text_len - 1] == ' ')
        return ZCL_INVALID_ENCODING;
    *word_count = count;
    return ZCL_OK;
}
