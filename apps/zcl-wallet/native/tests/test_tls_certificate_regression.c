/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <stdint.h>
#include <stdio.h>

#include "tls_empty_name_crash.h"

int zcl_fuzz_tls_input(const uint8_t *data, size_t size);

int main(void)
{
    const int state = zcl_fuzz_tls_input(zcl_tls_empty_name_crash,
        sizeof(zcl_tls_empty_name_crash));
    if (state != -1) {
        fprintf(stderr, "Empty-name certificate flight reached TLS state %d\n", state);
        return 1;
    }
    puts("TLS empty-name certificate flight rejected without undefined behavior");
    return 0;
}
