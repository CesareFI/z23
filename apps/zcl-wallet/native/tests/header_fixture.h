/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_HEADER_FIXTURE_H
#define ZCL_HEADER_FIXTURE_H
#include "header_internal.h"
void header_fixture_decode(const char *hex, uint8_t *wire, size_t length);
void header_fixture_check(const uint8_t *wire, size_t length, const zcl_header_view *view);
#endif
