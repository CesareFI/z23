/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MBEDTLS_CONFIG_H
#define ZCL_MBEDTLS_CONFIG_H
/* Only the portable hash primitives needed by the existing Zclassic formats.
 * No TLS, PSA state, allocator, platform hooks, assembly or optional runtime
 * instruction probing is enabled by this configuration. */
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_RIPEMD160_C
#define MBEDTLS_SELF_TEST
#endif
