/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_WALLET_TEST_UX_H
#define ZCL_BLUE_WALLET_TEST_UX_H

#include "os.h"

void blue_wallet_test_display(const bagl_element_t *elements, size_t count,
    unsigned int (*button)(unsigned int, unsigned int));

#define UX_DISPLAY(elements_array, preprocessor) \
    blue_wallet_test_display((elements_array), \
        sizeof(elements_array) / sizeof((elements_array)[0]), \
        elements_array##_button)

#endif
