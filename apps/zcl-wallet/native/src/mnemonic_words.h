/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MNEMONIC_WORDS_H
#define ZCL_MNEMONIC_WORDS_H
#include "zcl_keys.h"

bool zcl_entropy_length_valid(size_t length);
zcl_status zcl_words_write(const uint8_t *bits, size_t bits_len, size_t word_count,
                           uint8_t *text, size_t text_capacity, size_t *text_len);
zcl_status zcl_words_read(const uint8_t *text, size_t text_len,
                          uint8_t *bits, size_t bits_capacity, size_t *word_count);
#endif
