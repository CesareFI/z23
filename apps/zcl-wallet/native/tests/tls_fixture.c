/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "tls_fixture.h"
#include <openssl/x509v3.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "TLS fixture failed at %d\n", __LINE__); abort(); } } while (0)

static EVP_PKEY *new_key(int rsa_bits)
{
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(rsa_bits == 0 ? EVP_PKEY_EC : EVP_PKEY_RSA, NULL);
    CHECK(context != NULL);
    CHECK(EVP_PKEY_keygen_init(context) == 1);
    if (rsa_bits == 0) {
        CHECK(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_X9_62_prime256v1) == 1);
    } else {
        CHECK(EVP_PKEY_CTX_set_rsa_keygen_bits(context, rsa_bits) == 1);
    }
    EVP_PKEY *key = NULL;
    CHECK(EVP_PKEY_keygen(context, &key) == 1 && key != NULL);
    EVP_PKEY_CTX_free(context);
    return key;
}

static void extension(X509 *certificate, X509 *issuer, int nid, const char *value)
{
    X509V3_CTX context;
    X509V3_set_ctx(&context, issuer, certificate, NULL, NULL, 0);
    X509_EXTENSION *entry = X509V3_EXT_conf_nid(NULL, &context, nid, value);
    CHECK(entry != NULL);
    CHECK(X509_add_ext(certificate, entry, -1) == 1);
    X509_EXTENSION_free(entry);
}

static X509 *new_certificate(EVP_PKEY *key, const char *name, long starts, long ends)
{
    X509 *certificate = X509_new();
    CHECK(certificate != NULL);
    CHECK(X509_set_version(certificate, 2) == 1);
    CHECK(ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1) == 1);
    CHECK(X509_gmtime_adj(X509_getm_notBefore(certificate), starts) != NULL);
    CHECK(X509_gmtime_adj(X509_getm_notAfter(certificate), ends) != NULL);
    CHECK(X509_set_pubkey(certificate, key) == 1);
    X509_NAME *subject = X509_get_subject_name(certificate);
    CHECK(subject != NULL);
    CHECK(X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
        (const unsigned char *)name, (int)strlen(name), -1, 0) == 1);
    return certificate;
}

static void keys_init(tls_fixture_keys *keys, long starts, long ends, const char *usage, int rsa_bits)
{
    memset(keys, 0, sizeof(*keys));
    /* Ephemeral TLS-only fixture keys remain in memory and are never printed,
     * serialized or related to a wallet. OpenSSL owns/cleans their storage. */
    keys->authority_key = new_key(rsa_bits == 0 ? 0 : 2048);
    keys->server_key = new_key(rsa_bits);
    keys->authority = new_certificate(keys->authority_key, "Unfunded local TLS fixture CA", -3600, 86400);
    CHECK(X509_set_issuer_name(keys->authority, X509_get_subject_name(keys->authority)) == 1);
    extension(keys->authority, keys->authority, NID_basic_constraints, "critical,CA:TRUE,pathlen:0");
    extension(keys->authority, keys->authority, NID_key_usage, "critical,keyCertSign,cRLSign");
    CHECK(X509_sign(keys->authority, keys->authority_key, EVP_sha256()) > 0);
    keys->certificate = new_certificate(keys->server_key, "wallet-fixture.invalid", starts, ends);
    CHECK(X509_set_issuer_name(keys->certificate, X509_get_subject_name(keys->authority)) == 1);
    extension(keys->certificate, keys->authority, NID_basic_constraints, "critical,CA:FALSE");
    extension(keys->certificate, keys->authority, NID_key_usage, "critical,digitalSignature");
    extension(keys->certificate, keys->authority, NID_ext_key_usage, usage);
    extension(keys->certificate, keys->authority, NID_subject_alt_name, "DNS:wallet-fixture.invalid");
    CHECK(X509_sign(keys->certificate, keys->authority_key, EVP_sha256()) > 0);
    int length = i2d_X509(keys->authority, NULL);
    CHECK(length > 0 && (size_t)length <= sizeof(keys->root));
    unsigned char *output = keys->root;
    CHECK(i2d_X509(keys->authority, &output) == length);
    CHECK(output == keys->root + length);
    keys->root_length = (size_t)length;
    keys->context = SSL_CTX_new(TLS_server_method());
    CHECK(keys->context != NULL);
    /* Let the negative fixture serve a weak certificate so the C client's
     * verification policy, rather than OpenSSL setup, is what rejects it. */
    if (rsa_bits != 0 && rsa_bits < 2048) SSL_CTX_set_security_level(keys->context, 0);
    CHECK(SSL_CTX_set_min_proto_version(keys->context, TLS1_2_VERSION) == 1);
    CHECK(SSL_CTX_set_max_proto_version(keys->context, TLS1_2_VERSION) == 1);
    CHECK(SSL_CTX_set_cipher_list(keys->context, rsa_bits == 0 ?
        "ECDHE-ECDSA-CHACHA20-POLY1305" : "ECDHE-RSA-CHACHA20-POLY1305") == 1);
    CHECK(SSL_CTX_use_certificate(keys->context, keys->certificate) == 1);
    CHECK(SSL_CTX_use_PrivateKey(keys->context, keys->server_key) == 1);
    CHECK(SSL_CTX_check_private_key(keys->context) == 1);
}

void tls_fixture_keys_init(tls_fixture_keys *keys, long starts, long ends, const char *usage)
{
    keys_init(keys, starts, ends, usage, 0);
}

void tls_fixture_rsa_keys_init(tls_fixture_keys *keys, int bits)
{
    keys_init(keys, -60, 3600, "serverAuth", bits);
}

void tls_fixture_keys_clear(tls_fixture_keys *keys)
{
    SSL_CTX_free(keys->context);
    X509_free(keys->authority);
    X509_free(keys->certificate);
    EVP_PKEY_free(keys->authority_key);
    EVP_PKEY_free(keys->server_key);
    memset(keys, 0, sizeof(*keys));
}

void tls_fixture_empty_names(tls_fixture_keys *keys)
{
    X509_NAME *empty = X509_NAME_new();
    CHECK(empty != NULL);
    CHECK(X509_set_subject_name(keys->certificate, empty) == 1);
    CHECK(X509_set_issuer_name(keys->certificate, empty) == 1);
    X509_NAME_free(empty);
    CHECK(X509_sign(keys->certificate, keys->authority_key, EVP_sha256()) > 0);
    CHECK(SSL_CTX_use_certificate(keys->context, keys->certificate) == 1);
}

static void pause_tick(void)
{
    const struct timespec delay = {0, 5000000};
    (void)nanosleep(&delay, NULL);
}

static int wait_client(tls_fixture_server *server)
{
    for (unsigned tick = 0; tick < 400; ++tick) {
        if (atomic_load(&server->stop)) return -1;
        struct pollfd listener = {server->listener, POLLIN, 0};
        const int result = poll(&listener, 1, 5);
        if (result > 0) return accept(server->listener, NULL, NULL);
        CHECK(result == 0);
    }
    return -1;
}

static void reply(SSL *ssl, tls_fixture_server *server)
{
    static const char expected[] = "fixture\n";
    uint8_t request[sizeof(expected) - 1];
    size_t received = 0;
    while (received < sizeof(request)) {
        const int n = SSL_read(ssl, request + received, (int)(sizeof(request) - received));
        if (n <= 0) return;
        CHECK((size_t)n <= sizeof(request) - received);
        received += (size_t)n;
    }
    CHECK(memcmp(request, expected, sizeof(request)) == 0);
    server->application_received = true;
    static const char text[] = "public reply\n";
    for (size_t i = 0; i < sizeof(text) - 1; ++i) {
        if (SSL_write(ssl, text + i, 1) != 1) return; /* Force multiple TLS records. */
    }
}

static void serve(int descriptor, tls_fixture_server *server)
{
    if (server->mode == TLS_FIXTURE_SILENT) {
        for (unsigned tick = 0; tick < 400 && !atomic_load(&server->stop); ++tick) pause_tick();
        return;
    }
    const struct timeval timeout = {1, 0};
    CHECK(setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    CHECK(setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0);
    SSL *ssl = SSL_new(server->keys->context);
    BIO *bio = BIO_new_socket(descriptor, BIO_NOCLOSE);
    CHECK(ssl != NULL && bio != NULL);
    SSL_set_bio(ssl, bio, bio); /* SSL owns this BIO; the fixture owns the socket. */
    if (SSL_accept(ssl) == 1) {
        if (server->mode == TLS_FIXTURE_CORRUPT) {
            const uint8_t invalid[37] = {23, 3, 3, 0, 32};
            CHECK(send(descriptor, invalid, sizeof(invalid), MSG_NOSIGNAL) == (ssize_t)sizeof(invalid));
        } else if (server->mode == TLS_FIXTURE_FLOOD) {
            uint8_t bytes[4096];
            memset(bytes, 'a', sizeof(bytes));
            for (size_t i = 0; i < 80; ++i) {
                if (SSL_write(ssl, bytes, sizeof(bytes)) != sizeof(bytes)) break;
            }
        } else if (server->mode == TLS_FIXTURE_ECHO) {
            reply(ssl, server);
        }
    }
    SSL_free(ssl);
}

static void *server_thread(void *argument)
{
    tls_fixture_server *server = argument;
    const int descriptor = wait_client(server);
    if (descriptor >= 0) {
        serve(descriptor, server);
        CHECK(close(descriptor) == 0);
    }
    return NULL;
}

void tls_fixture_start(tls_fixture_server *server, tls_fixture_keys *keys, tls_fixture_mode mode)
{
    memset(server, 0, sizeof(*server));
    atomic_init(&server->stop, false);
    server->keys = keys;
    server->mode = mode;
    server->listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    CHECK(server->listener >= 0);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(server->listener, (const struct sockaddr *)&address, sizeof(address)) == 0);
    CHECK(listen(server->listener, 1) == 0);
    socklen_t length = sizeof(address);
    CHECK(getsockname(server->listener, (struct sockaddr *)&address, &length) == 0);
    CHECK(length == sizeof(address));
    server->port = ntohs(address.sin_port);
    CHECK(pthread_create(&server->thread, NULL, server_thread, server) == 0);
}

void tls_fixture_stop(tls_fixture_server *server)
{
    atomic_store(&server->stop, true);
    CHECK(pthread_join(server->thread, NULL) == 0);
    CHECK(close(server->listener) == 0);
}

zcl_endpoint tls_fixture_endpoint(const tls_fixture_server *server)
{
    zcl_endpoint endpoint = {0};
    static const char host[] = "wallet-fixture.invalid";
    memcpy(endpoint.host, host, sizeof(host) - 1);
    endpoint.host_length = sizeof(host) - 1;
    endpoint.port = server->port;
    endpoint.address_count = 1;
    endpoint.addresses[0].length = 4;
    endpoint.addresses[0].bytes[0] = 127;
    endpoint.addresses[0].bytes[3] = 1;
    return endpoint;
}
