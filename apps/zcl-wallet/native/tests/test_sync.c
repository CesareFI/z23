/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_address_parse
#undef zcl_electrum_tip_reply
#include "sync_fixture.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sync check failed at %d\n", __LINE__); abort(); } } while (0)
#define TEXT(s) (const uint8_t *)(s), sizeof(s) - 1
static char frame[4096];
static uintptr_t parsed_identity, tip_identity;
static unsigned parsed_calls, parsed_clears, tip_calls, tip_clears;
static bool fail_parse, fail_tip;

zcl_status zcl_sync_test_parse(const uint8_t *text, size_t length, zcl_network network, zcl_address *address);
zcl_status zcl_sync_test_tip(const uint8_t *bytes, size_t length, uint32_t id,
    zcl_network network, zcl_reported_tip *tip);
void zcl_sync_test_zero(void *buffer, size_t length);

zcl_status zcl_sync_test_parse(const uint8_t *text, size_t length, zcl_network network, zcl_address *address)
{
    CHECK(parsed_identity == 0 && parsed_calls == parsed_clears);
    parsed_identity = (uintptr_t)address;
    ++parsed_calls;
    if (fail_parse) { memset(address, 0xa5, sizeof(*address)); return ZCL_INVALID_ENCODING; }
    return zcl_address_parse(text, length, network, address);
}

zcl_status zcl_sync_test_tip(const uint8_t *bytes, size_t length, uint32_t id,
    zcl_network network, zcl_reported_tip *tip)
{
    CHECK(tip_identity == 0 && tip_calls == tip_clears);
    tip_identity = (uintptr_t)tip;
    ++tip_calls;
    if (fail_tip) { memset(tip, 0x5a, sizeof(*tip)); return ZCL_IO_FAILURE; }
    return zcl_electrum_tip_reply(bytes, length, id, network, tip);
}

void zcl_sync_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    if ((uintptr_t)buffer == parsed_identity) {
        CHECK(length == sizeof(zcl_address));
        parsed_identity = 0;
        ++parsed_clears;
    } else {
        CHECK((uintptr_t)buffer == tip_identity && length == sizeof(zcl_reported_tip));
        tip_identity = 0;
        ++tip_clears;
    }
}

static void retired(void)
{
    CHECK(parsed_identity == 0 && tip_identity == 0);
    CHECK(parsed_calls == parsed_clears && tip_calls == tip_clears);
}

static bool contains(const uint8_t *bytes, size_t length, const uint8_t *value, size_t count)
{
    if (count > length) return false;
    for (size_t i = 0; i <= length - count; ++i)
        if (memcmp(bytes + i, value, count) == 0) return true;
    return false;
}

static void no_report(const zcl_sync *session, zcl_status expected)
{
    zcl_sync_report output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    CHECK(zcl_sync_get_report(session, &output) == expected);
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
}

static void request(zcl_sync *session, unsigned step)
{
    uint8_t bytes[258];
    memset(bytes, 0xa5, sizeof(bytes));
    size_t length = 777;
    CHECK(zcl_sync_request(session, bytes + 1, 1, &length) == ZCL_BUFFER_TOO_SMALL);
    CHECK(length == 777 && !session->waiting && bytes[1] == 0xa5);
    CHECK(zcl_sync_request(session, bytes + 1, 256, &length) == ZCL_OK);
    CHECK(length > 0 && length <= 256 && bytes[0] == 0xa5 && bytes[length + 1] == 0xa5);
    CHECK(bytes[length] == '\n');
    CHECK(!contains(bytes + 1, length, session->candidate.address, 35));
    CHECK(contains(bytes + 1, length, TEXT("blockchain.scripthash.get_balance")) == (step == 5));
    CHECK(contains(bytes + 1, length,
        TEXT("acb87996319dca2c2e2afd6c0f7514b18e72e204069718976e1abdc8fcf5de75")) == (step == 5));
    uint8_t again[256];
    memset(again, 0xa5, sizeof(again));
    size_t untouched = 777;
    CHECK(zcl_sync_request(session, again, sizeof(again), &untouched) == ZCL_BUSY);
    CHECK(untouched == 777 && again[0] == 0xa5);
}

static void advance(zcl_sync *session, unsigned through)
{
    for (unsigned step = 1; step <= through; ++step) {
        no_report(session, ZCL_BUSY);
        request(session, step);
        const size_t length = sync_fixture_reply(session->candidate.network, step,
            session->request_id, frame, sizeof(frame));
        CHECK(zcl_sync_reply(session, (const uint8_t *)frame, length) == ZCL_OK);
        retired();
    }
}

static void completed(zcl_network network, uint32_t first_id)
{
    zcl_sync session;
    sync_fixture_start(&session, network, first_id);
    retired();
    uint8_t address[35];
    memcpy(address, session.candidate.address, sizeof(address));
    advance(&session, 6);
    CHECK(session.request_id == first_id + 5 && !session.waiting && session.phase == ZCL_SYNC_DONE);
    zcl_sync_report report = {0};
    CHECK(zcl_sync_get_report(&session, &report) == ZCL_OK);
    CHECK(report.network == network && memcmp(report.address, address, sizeof(address)) == 0);
    CHECK(report.balance.confirmed == 1000 && report.balance.pending_delta == -7 && report.balance.total == 993);
    uint8_t genesis[32];
    CHECK(zcl_network_genesis(network, genesis, sizeof(genesis)) == ZCL_OK);
    CHECK(report.tip.height == 0 && memcmp(report.tip.hash, genesis, sizeof(genesis)) == 0);
    CHECK(zcl_sync_abort(&session, ZCL_CANCELLED) == ZCL_CANCELLED);
    no_report(&session, ZCL_CANCELLED);
    CHECK(session.candidate.balance.total == 0 && session.candidate.balance.confirmed == 0);
}

static void wrong_ids_and_abort(void)
{
    for (unsigned stop = 1; stop <= 6; ++stop) {
        zcl_sync session;
        sync_fixture_start(&session, ZCL_MAINNET, 10);
        advance(&session, stop - 1);
        request(&session, stop);
        const size_t length = sync_fixture_reply(ZCL_MAINNET, stop,
            session.request_id + 1, frame, sizeof(frame));
        CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_INVALID_ENCODING);
        no_report(&session, ZCL_INVALID_ENCODING);
        size_t output_length = 123;
        uint8_t output[256] = {0};
        CHECK(zcl_sync_request(&session, output, sizeof(output), &output_length) == ZCL_INVALID_ENCODING);
        CHECK(output_length == 123 && session.candidate.balance.total == 0);
        sync_fixture_start(&session, ZCL_MAINNET, 10);
        advance(&session, stop - 1);
        request(&session, stop);
        CHECK(zcl_sync_abort(&session, ZCL_TIMED_OUT) == ZCL_TIMED_OUT);
        no_report(&session, ZCL_TIMED_OUT);
    }
}

static void invalid_abort_reason_preserves_session(void)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_MAINNET, 10);
    request(&session, 1);
    zcl_sync before;
    memcpy(&before, &session, sizeof(before));
    CHECK(zcl_sync_abort(&session, (zcl_status)-1) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(&session, &before, sizeof(session)) == 0);
    CHECK(zcl_sync_abort(&session, (zcl_status)(ZCL_TLS_FAILURE + 1)) == ZCL_INVALID_ARGUMENT);
    CHECK(memcmp(&session, &before, sizeof(session)) == 0);
}

static void changed_tip(bool change_height)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    advance(&session, 5);
    request(&session, 6);
    const size_t length = sync_fixture_reply(ZCL_MAINNET, 6, 6, frame, sizeof(frame));
    char *field = strstr(frame, change_height ? "\"height\":0" : "\"hex\":\"04");
    CHECK(field != NULL);
    if (change_height) field[9] = '1';
    else field[7] = '1';
    CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_IO_UNCERTAIN);
    retired();
    no_report(&session, ZCL_IO_UNCERTAIN);
}

static void wrong_network_and_notification(void)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    advance(&session, 1);
    request(&session, 2);
    const size_t length = sync_fixture_reply(ZCL_TESTNET, 2, 2, frame, sizeof(frame));
    CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) != ZCL_OK);
    CHECK(session.phase == ZCL_SYNC_FAILED);
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    advance(&session, 4);
    request(&session, 5);
    CHECK(zcl_sync_reply(&session, TEXT("{\"method\":\"blockchain.headers.subscribe\",\"params\":[]}"))
        == ZCL_INVALID_ENCODING);
    no_report(&session, ZCL_INVALID_ENCODING);
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    CHECK(zcl_sync_reply(&session, TEXT("{\"id\":1,\"result\":[\"x\",\"1.2\"]}")) == ZCL_INVALID_ENCODING);
}

static void start_errors(void)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    uint8_t address[35];
    memcpy(address, session.candidate.address, sizeof(address));
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_MAINNET, UINT32_MAX - 4) == ZCL_OUT_OF_RANGE);
    no_report(&session, ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_MAINNET, 0) == ZCL_OUT_OF_RANGE);
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_TESTNET, 1) == ZCL_UNSUPPORTED);
    CHECK(zcl_sync_start(&session, NULL, 0, ZCL_MAINNET, 1) == ZCL_INVALID_ARGUMENT);
    no_report(&session, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_MAINNET, 1) == ZCL_OK);
    address[0] ^= 1;
    CHECK(session.candidate.address[0] != address[0]); /* No retained caller pointer. */
    request(&session, 1);
    CHECK(zcl_sync_reply(&session, TEXT("{}")) != ZCL_OK);
    CHECK(zcl_sync_get_report(NULL, NULL) == ZCL_INVALID_ARGUMENT);
    memset(&session, 0, sizeof(session));
    session.phase = ZCL_SYNC_FAILED; /* Inconsistent state must not mean success. */
    no_report(&session, ZCL_INVALID_ARGUMENT);
    uint8_t output[256];
    size_t length = 99;
    CHECK(zcl_sync_request(&session, output, sizeof(output), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(length == 99);
}

static void dirty_parser_refusal(void)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_MAINNET, 1);
    uint8_t address[35];
    memcpy(address, session.candidate.address, sizeof(address));
    fail_parse = true;
    CHECK(zcl_sync_start(&session, address, sizeof(address), ZCL_MAINNET, 1) == ZCL_INVALID_ENCODING);
    fail_parse = false;
    retired();
    CHECK(session.phase == ZCL_SYNC_FAILED && session.request_id == 0 && !session.waiting);
    const zcl_sync_report empty = {0};
    CHECK(memcmp(&session.candidate, &empty, sizeof(empty)) == 0);
    no_report(&session, ZCL_INVALID_ENCODING);
}

static void dirty_tip_refusal(unsigned stop)
{
    zcl_sync session;
    sync_fixture_start(&session, ZCL_TESTNET, 10);
    uint8_t address[35];
    memcpy(address, session.candidate.address, sizeof(address));
    advance(&session, stop - 1);
    request(&session, stop);
    const uint32_t id = session.request_id;
    const size_t length = sync_fixture_reply(ZCL_TESTNET, stop, id, frame, sizeof(frame));
    fail_tip = true;
    CHECK(zcl_sync_reply(&session, (const uint8_t *)frame, length) == ZCL_IO_FAILURE);
    fail_tip = false;
    retired();
    CHECK(session.phase == ZCL_SYNC_FAILED && session.request_id == id && !session.waiting);
    zcl_sync_report expected = {0};
    memcpy(expected.address, address, sizeof(address));
    expected.network = ZCL_TESTNET;
    CHECK(memcmp(&session.candidate, &expected, sizeof(expected)) == 0);
    no_report(&session, ZCL_IO_FAILURE);
}

int main(void)
{
    completed(ZCL_MAINNET, 1); completed(ZCL_TESTNET, UINT32_MAX - 5);
    wrong_ids_and_abort(); invalid_abort_reason_preserves_session();
    changed_tip(false); changed_tip(true);
    wrong_network_and_notification(); start_errors();
    dirty_parser_refusal(); dirty_tip_refusal(4); dirty_tip_refusal(6);
    retired();
    puts("Sync ordering, identity-before-address, complete-only report, cancellation, request IDs and changed-tip refusal passed");
    return 0;
}
