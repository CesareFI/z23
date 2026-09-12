/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MBEDTLS_CONFIG_H
#define ZCL_MBEDTLS_CONFIG_H
/* The normal wallet only needs these hash primitives. The quarantined
 * certificate/TLS profile is available solely to explicit host review builds. */
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_RIPEMD160_C
#define MBEDTLS_SELF_TEST
#ifdef ZCL_TLS_REVIEW
#if defined(__ANDROID__)
#error "TLS is BLOCKED — REQUIRES FURTHER SECURITY REVIEW"
#endif
#include "zcl_tls_alloc.h"
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO zcl_tls_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO zcl_tls_free
/* Portable C hash and authenticated TLS client profile. No software AES,
 * PSK, static-RSA key exchange, DTLS, tickets, renegotiation or PSA state.
 * Protocol/hostname/CA verification is mandatory in transport_tls.c. */
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_WINDOW_SIZE 3
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_RSA_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096
#define MBEDTLS_X509_MAX_INTERMEDIATE_CA 8
#define MBEDTLS_MPI_MAX_SIZE 512
#endif
#endif
