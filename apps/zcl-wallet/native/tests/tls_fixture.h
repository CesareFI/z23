/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef TLS_FIXTURE_H
#define TLS_FIXTURE_H
#include "zcl_transport.h"
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdatomic.h>

typedef enum { TLS_FIXTURE_ECHO, TLS_FIXTURE_SILENT, TLS_FIXTURE_CORRUPT,
    TLS_FIXTURE_CLOSE, TLS_FIXTURE_FLOOD } tls_fixture_mode;
typedef struct {
    SSL_CTX *context;
    EVP_PKEY *authority_key, *server_key;
    X509 *authority, *certificate;
    uint8_t root[4096];
    size_t root_length;
} tls_fixture_keys;
typedef struct {
    tls_fixture_keys *keys;
    pthread_t thread;
    atomic_bool stop;
    int listener;
    uint16_t port;
    tls_fixture_mode mode;
    bool application_received;
} tls_fixture_server;
void tls_fixture_keys_init(tls_fixture_keys *keys, long starts, long ends, const char *usage);
void tls_fixture_rsa_keys_init(tls_fixture_keys *keys, int bits);
void tls_fixture_empty_names(tls_fixture_keys *keys);
void tls_fixture_keys_clear(tls_fixture_keys *keys);
void tls_fixture_start(tls_fixture_server *server, tls_fixture_keys *keys, tls_fixture_mode mode);
void tls_fixture_stop(tls_fixture_server *server);
zcl_endpoint tls_fixture_endpoint(const tls_fixture_server *server);
#endif
