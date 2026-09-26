/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_REVIEW_SCREEN_H
#define ZCL_BLUE_REVIEW_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "The Blue review screen requires ISO C23"
#endif

enum { ZCL_BLUE_REVIEW_LINES = 6, ZCL_BLUE_REVIEW_LINE_SIZE = 32 };

/* Formats the 76-byte summary and SHA-256 reply for a read-only screen. */
bool blue_review_screen_format(
    const uint8_t reply[76],
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE]);

#endif
