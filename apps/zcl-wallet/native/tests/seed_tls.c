/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "tls_fixture.h"
#include "mbedtls/ssl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "TLS seed check failed at %d\n", __LINE__); abort(); } } while (0)
int zcl_fuzz_tls_input(const uint8_t *data, size_t size);

static void be24(uint8_t *output, size_t value)
{
    CHECK(value <= 0xffffff);
    output[0] = (uint8_t)(value >> 16);
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)value;
}

static void write_seed(const char *name, const uint8_t *bytes, size_t length)
{
    FILE *file = fopen(name, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(bytes, 1, length, file) == length);
    CHECK(fclose(file) == 0);
}

static void seed(tls_fixture_keys *keys, const char *name)
{
    static uint8_t packet[12000];
    memset(packet, 0, sizeof(packet));
    CHECK(keys->root_length <= 4096);
    packet[0] = (uint8_t)(keys->root_length >> 8);
    packet[1] = (uint8_t)keys->root_length;
    memcpy(packet + 2, keys->root, keys->root_length);
    uint8_t *record = packet + 2 + keys->root_length;
    uint8_t *hello = record + 5;
    hello[0] = 2; /* ServerHello, TLS 1.2, empty session ID, no compression. */
    be24(hello + 1, 49);
    hello[4] = 3; hello[5] = 3;
    memset(hello + 6, 0x24, 32);
    hello[39] = 0xcc;
    hello[40] = EVP_PKEY_base_id(keys->server_key) == EVP_PKEY_RSA ? 0xa8 : 0xa9;
    hello[43] = 9; /* renegotiation_info and extended_master_secret */
    hello[44] = 0xff; hello[45] = 1; hello[47] = 1;
    hello[49] = 0; hello[50] = 0x17;
    uint8_t *certificate = hello + 53;
    const int encoded = i2d_X509(keys->certificate, NULL);
    CHECK(encoded > 0 && encoded <= 4096);
    const size_t length = (size_t)encoded;
    certificate[0] = 11;
    be24(certificate + 1, length + 6);
    be24(certificate + 4, length + 3);
    be24(certificate + 7, length);
    unsigned char *output = certificate + 10;
    CHECK(i2d_X509(keys->certificate, &output) == encoded);
    CHECK(output == certificate + 10 + length);
    /* Stop before ServerKeyExchange: this public seed reaches certificate
     * validation without retaining any TLS private key or signed transcript. */
    const size_t payload = 53 + 10 + length;
    record[0] = 22; record[1] = 3; record[2] = 3;
    record[3] = (uint8_t)(payload >> 8); record[4] = (uint8_t)payload;
    const size_t packet_length = 2 + keys->root_length + 5 + payload;
    const int state = zcl_fuzz_tls_input(packet, packet_length);
    if (state != MBEDTLS_SSL_SERVER_KEY_EXCHANGE) fprintf(stderr, "TLS seed state %d\n", state);
    CHECK(state == MBEDTLS_SSL_SERVER_KEY_EXCHANGE);
    write_seed(name, packet, packet_length);
}

int main(void)
{
    tls_fixture_keys keys;
    tls_fixture_keys_init(&keys, -60, 86400, "serverAuth");
    seed(&keys, "ecdsa-certificate-flight");
    tls_fixture_keys_clear(&keys);
    tls_fixture_rsa_keys_init(&keys, 2048);
    seed(&keys, "rsa-certificate-flight");
    tls_fixture_keys_clear(&keys);
    return 0;
}
