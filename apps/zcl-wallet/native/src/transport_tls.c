/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transport_internal.h"
#include "zcl_keys.h"
#include <poll.h>
#include <string.h>

static const mbedtls_x509_crt_profile certificate_profile = {
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSA) | MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSASSA_PSS) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY) | MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1) |
        MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP521R1),
    2048
};
static const int ciphers[] = {
    MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, 0
};

static int tls_random(void *context, unsigned char *output, size_t length)
{
    zcl_transport *transport = context;
    if (length == 0 || length > 1024 || transport->random_bytes > 8192 - length)
        return -1;
    for (size_t offset = 0; offset < length;) {
        const size_t count = length - offset < 64 ? length - offset : 64;
        zcl_status status = zcl_net_limit_check(&transport->limit);
        if (status == ZCL_OK) status = zcl_random_bytes(output + offset, count);
        if (status != ZCL_OK) {
            zcl_secure_zero(output, length);
            (void)zcl_net_fail(transport, status);
            return -1;
        }
        offset += count;
    }
    transport->random_bytes += length;
    return 0;
}

static zcl_status anchor_size(const zcl_trust_anchor *anchor, size_t remaining)
{
    if (anchor->der == NULL || anchor->length == 0) return ZCL_INVALID_ARGUMENT;
    if (anchor->length > ZCL_NET_CERT_MAX || anchor->length > remaining) return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status parse_anchor(zcl_transport *transport, const zcl_trust_anchor *anchor)
{
    if (mbedtls_x509_crt_parse_der(&transport->anchors, anchor->der, anchor->length) != 0)
        return ZCL_TLS_FAILURE;
    const mbedtls_x509_crt *last = &transport->anchors;
    for (size_t i = 0; last->next != NULL && i < ZCL_NET_ANCHORS_MAX; ++i) last = last->next;
    if (last->next != NULL || last->raw.len != anchor->length) return ZCL_INVALID_ENCODING;
    return ZCL_OK;
}

static zcl_status read_anchors(zcl_transport *transport, const zcl_trust_anchor *anchors, size_t count)
{
    if (anchors == NULL || count == 0) return ZCL_INVALID_ARGUMENT;
    if (count > ZCL_NET_ANCHORS_MAX) return ZCL_OUT_OF_RANGE;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        zcl_status status = anchor_size(&anchors[i], ZCL_NET_TRUST_MAX - total);
        if (status != ZCL_OK) return status;
        total += anchors[i].length;
        const zcl_status allowed = zcl_net_limit_check(&transport->limit);
        if (allowed != ZCL_OK) return allowed;
        status = parse_anchor(transport, &anchors[i]);
        if (status != ZCL_OK) return status;
    }
    return ZCL_OK;
}

zcl_status zcl_tls_configure(zcl_transport *transport, const zcl_endpoint *endpoint,
                             const zcl_trust_anchor *anchors, size_t anchor_count)
{
    zcl_status status = read_anchors(transport, anchors, anchor_count);
    if (status != ZCL_OK) return status;
    if (mbedtls_ssl_config_defaults(&transport->config, MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) return ZCL_TLS_FAILURE;
    mbedtls_ssl_conf_authmode(&transport->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&transport->config, &transport->anchors, NULL);
    mbedtls_ssl_conf_cert_profile(&transport->config, &certificate_profile);
    mbedtls_ssl_conf_ciphersuites(&transport->config, ciphers);
    mbedtls_ssl_conf_min_tls_version(&transport->config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&transport->config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&transport->config, tls_random, transport);
    if (mbedtls_ssl_setup(&transport->ssl, &transport->config) != 0) return ZCL_TLS_FAILURE;
    char hostname[ZCL_NET_HOST_MAX + 1];
    memcpy(hostname, endpoint->host, endpoint->host_length);
    hostname[endpoint->host_length] = 0;
    if (mbedtls_ssl_set_hostname(&transport->ssl, hostname) != 0) return ZCL_TLS_FAILURE;
    mbedtls_ssl_set_bio(&transport->ssl, transport, zcl_net_send, zcl_net_receive, NULL);
    return ZCL_OK;
}

zcl_status zcl_tls_retry(zcl_transport *transport, int result)
{
    if (transport->fault != ZCL_OK) return transport->fault;
    if (transport->heap.denied) return ZCL_RESOURCE_EXHAUSTED;
    if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) return ZCL_IO_FAILURE;
    if (result == MBEDTLS_ERR_SSL_WANT_READ) return zcl_net_wait(transport, POLLIN);
    if (result == MBEDTLS_ERR_SSL_WANT_WRITE) return zcl_net_wait(transport, POLLOUT);
    return ZCL_TLS_FAILURE;
}

zcl_status zcl_tls_handshake(zcl_transport *transport)
{
    for (;;) {
        const zcl_status allowed = zcl_net_limit_check(&transport->limit);
        if (allowed != ZCL_OK) return allowed;
        const int result = mbedtls_ssl_handshake(&transport->ssl);
        if (result == 0) break;
        const zcl_status status = zcl_tls_retry(transport, result);
        if (status != ZCL_OK) return status;
    }
    if (mbedtls_ssl_get_verify_result(&transport->ssl) != 0) return ZCL_TLS_FAILURE;
    if (mbedtls_ssl_get_version_number(&transport->ssl) != MBEDTLS_SSL_VERSION_TLS1_2)
        return ZCL_TLS_FAILURE;
    return zcl_net_limit_check(&transport->limit);
}
