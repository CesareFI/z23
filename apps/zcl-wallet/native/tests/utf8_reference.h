/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_UTF8_REFERENCE_H
#define ZCL_TEST_UTF8_REFERENCE_H
#include "zcl_wallet.h"

/* Test-only byte grammar from Unicode 17 Table 3-7 / RFC 3629 section 4.
 * Uses neither a production decoding helper nor its codepoint range table.
 * Immutable borrowed input, no allocation or retained state. */
zcl_status utf8_reference_text(const uint8_t *text, size_t length);
bool utf8_reference_forbidden(uint32_t code);
#endif
