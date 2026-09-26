/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_screen.h"

#include <stddef.h>
#include <string.h>

static uint32_t read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static uint64_t read_u64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i)
        value |= (uint64_t)bytes[i] << (8 * i);
    return value;
}

static bool append_text(char line[ZCL_BLUE_REVIEW_LINE_SIZE],
                        size_t *used, const char *text) {
    size_t length = strlen(text);
    if (length >= ZCL_BLUE_REVIEW_LINE_SIZE - *used) return false;
    memcpy(line + *used, text, length);
    *used += length;
    line[*used] = 0;
    return true;
}

static bool append_number(char line[ZCL_BLUE_REVIEW_LINE_SIZE],
                          size_t *used, uint64_t value) {
    char digits[20];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    if (count >= ZCL_BLUE_REVIEW_LINE_SIZE - *used) return false;
    while (count) line[(*used)++] = digits[--count];
    line[*used] = 0;
    return true;
}

static bool format_pair(char line[ZCL_BLUE_REVIEW_LINE_SIZE],
                        const char *label, uint32_t first, uint32_t second) {
    size_t used = 0;
    return append_text(line, &used, label) &&
        append_number(line, &used, first) &&
        append_text(line, &used, "/") &&
        append_number(line, &used, second);
}

bool blue_review_screen_format(
    const uint8_t reply[76],
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE]) {
    if (!reply || !lines) return false;
    memset(lines, 0, ZCL_BLUE_REVIEW_LINES * ZCL_BLUE_REVIEW_LINE_SIZE);
    size_t used = 0;
    if (!format_pair(lines[0], "T INPUT/OUT: ",
                     read_u32(reply), read_u32(reply + 4)) ||
        !append_text(lines[1], &used, "PUB ZAT: ") ||
        !append_number(lines[1], &used, read_u64(reply + 20)) ||
        !format_pair(lines[2], "SAP S/O: ",
                     read_u32(reply + 8), read_u32(reply + 12)))
        return false;
    used = 0;
    if (!append_text(lines[3], &used, "SPROUT JS: ") ||
        !append_number(lines[3], &used, read_u32(reply + 16)))
        return false;
    used = 0;
    if (!append_text(lines[4], &used, "NO SIGNING; SHIELDED HIDDEN"))
        return false;
    static const char hex[] = "0123456789abcdef";
    used = 0;
    if (!append_text(lines[5], &used, "TX SHA256: ")) return false;
    for (unsigned i = 0; i < 8; ++i) {
        lines[5][used++] = hex[reply[44 + i] >> 4];
        lines[5][used++] = hex[reply[44 + i] & 15];
    }
    lines[5][used] = 0;
    return true;
}
