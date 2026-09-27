/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_REVIEW_SCREEN_H
#define ZCL_BLUE_REVIEW_SCREEN_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "The Blue review screen requires ISO C23"
#endif

enum { ZCL_BLUE_REVIEW_LINES = 6, ZCL_BLUE_REVIEW_LINE_SIZE = 32 };

/* Formats the 76-byte summary and SHA-256 reply for a read-only screen. */
bool blue_review_screen_format(
    const uint8_t reply[76],
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE]);

typedef bool (*blue_review_hash_fn)(const uint8_t *bytes, size_t length,
                                    uint8_t digest[32]);

/* Formats one independently parsed public output from reviewed wire bytes. */
bool blue_review_screen_output(
    const uint8_t *wire, size_t length, uint32_t index,
    blue_review_hash_fn hash,
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE]);

#endif
