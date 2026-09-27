/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_review.h"
#include "blue_payment_apdu.h"
#include "blue_payment_screen.h"
#include "blue_payment_simulate.h"
#include "blue_payment_live.h"
#include "zcl_zip243_host.h"
#include "zcl_zip243.h"

#include "crypto/blake2b.h"
#include <openssl/evp.h>
#include <openssl/sha.h>

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[256];
    size_t length, output_end[2], second_amount;
} fixture;

static void append_u32(fixture *item, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        item->bytes[item->length++] = (uint8_t)(value >> (i * 8));
}

static void append_u64(fixture *item, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        item->bytes[item->length++] = (uint8_t)(value >> (i * 8));
}

static fixture make_fixture(void) {
    fixture item = {0};
    append_u32(&item, 0x80000004);
    append_u32(&item, 0x892f2085);
    item.bytes[item.length++] = 1;
    memset(item.bytes + item.length, 0xaa, 32);
    item.length += 32;
    append_u32(&item, 0);
    item.bytes[item.length++] = 0;
    append_u32(&item, UINT32_MAX - 1);
    item.bytes[item.length++] = 2;
    append_u64(&item, 100000000);
    item.bytes[item.length++] = 25;
    const uint8_t p2pkh[3] = {0x76, 0xa9, 0x14};
    memcpy(item.bytes + item.length, p2pkh, sizeof p2pkh);
    item.length += sizeof p2pkh;
    memset(item.bytes + item.length, 0x11, 20);
    item.length += 20;
    item.bytes[item.length++] = 0x88;
    item.bytes[item.length++] = 0xac;
    item.output_end[0] = item.length;
    item.second_amount = item.length;
    append_u64(&item, 200000000);
    item.bytes[item.length++] = 23;
    item.bytes[item.length++] = 0xa9;
    item.bytes[item.length++] = 0x14;
    memset(item.bytes + item.length, 0x22, 20);
    item.length += 20;
    item.bytes[item.length++] = 0x87;
    item.output_end[1] = item.length;
    append_u32(&item, 100);
    append_u32(&item, 200);
    append_u64(&item, 0);
    item.bytes[item.length++] = 0;
    item.bytes[item.length++] = 0;
    item.bytes[item.length++] = 0;
    return item;
}

static fixture make_previous(void) {
    fixture item = {0};
    append_u32(&item, 1);
    item.bytes[item.length++] = 1;
    memset(item.bytes + item.length, 0, 32);
    item.length += 32;
    append_u32(&item, UINT32_MAX);
    item.bytes[item.length++] = 0;
    append_u32(&item, UINT32_MAX);
    item.bytes[item.length++] = 1;
    append_u64(&item, 400000000);
    item.bytes[item.length++] = 25;
    const uint8_t prefix[] = {0x76, 0xa9, 0x14};
    memcpy(item.bytes + item.length, prefix, sizeof prefix);
    item.length += sizeof prefix;
    memset(item.bytes + item.length, 0x33, 20);
    item.length += 20;
    item.bytes[item.length++] = 0x88;
    item.bytes[item.length++] = 0xac;
    append_u32(&item, 0);
    return item;
}

static bool sha_init(void *context) {
    return EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
}

static bool sha_update(void *context, const uint8_t *bytes, size_t length) {
    return EVP_DigestUpdate(context, bytes, length) == 1;
}

static bool sha_final(void *context, uint8_t digest[32]) {
    unsigned length = 0;
    return EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32;
}

static bool screen_hash(const uint8_t *bytes, size_t length,
    uint8_t digest[32]) {
    return SHA256(bytes, length, digest) != NULL;
}

static uint64_t expected_bound_digests(const fixture *spend,
    const zcl_tx_previous_transaction *previous, size_t count,
    uint8_t digests[ZCL_TX_PREFLIGHT_MAX_INPUTS][32]) {
    struct blake2b_ctx context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&context);
    zcl_tx_transparent_facts facts;
    assert(zcl_tx_transparent_bound_digests(spend->bytes, spend->length,
        previous, count, 0x76b809bb, screen_hash, &hasher,
        &facts, digests, ZCL_TX_PREFLIGHT_MAX_INPUTS) == 0);
    return facts.fee_zat;
}

static void begin(blue_payment_review *review, const fixture *item,
    struct blake2b_ctx *blake_context, EVP_MD_CTX *sha_context) {
    zcl_zip243_hasher blake = zcl_zip243_host_hasher(blake_context);
    zcl_tx_replay_sha256 sha = {.context = sha_context, .init = sha_init,
        .update = sha_update, .final = sha_final};
    assert(blue_payment_review_begin(review, (uint32_t)item->length, 0,
        0x76b809bb, &blake, &sha));
}

static void first_two_passes(blue_payment_review *review,
    const fixture *item) {
    for (unsigned pass = 0; pass < 2; ++pass) {
        assert(blue_payment_review_feed(review, item->bytes, item->length));
        assert(blue_payment_review_next_pass(review));
    }
    assert(review->total_outputs == 2);
}

static void test_success(const fixture *item) {
    struct blake2b_ctx blake_context, reference_context;
    EVP_MD_CTX *sha_context = EVP_MD_CTX_new();
    assert(sha_context);
    blue_payment_review review;
    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    uint8_t script_code[25] = {0x76, 0xa9, 0x14};
    memset(script_code + 3, 0x33, 20);
    script_code[23] = 0x88;
    script_code[24] = 0xac;
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    const blue_payment_output *output = blue_payment_review_pending(&review);
    assert(output && output->index == 0 && output->amount_zat == 100000000);
    assert(output->type == ZCL_TX_STREAM_P2PKH);
    for (size_t i = 0; i < 20; ++i) assert(output->hash160[i] == 0x11);
    blue_payment_screen screen;
    assert(blue_payment_screen_format(output, review.total_outputs,
        screen_hash, &screen));
    assert(strcmp(screen.title, "OUTPUT 1/2") == 0);
    assert(strcmp(screen.amount, "1.00000000 ZCL") == 0);
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        item->bytes + item->output_end[0],
        item->output_end[1] - item->output_end[0]));
    output = blue_payment_review_pending(&review);
    assert(output && output->index == 1 && output->amount_zat == 200000000);
    assert(output->type == ZCL_TX_STREAM_P2SH);
    for (size_t i = 0; i < 20; ++i) assert(output->hash160[i] == 0x22);
    assert(blue_payment_screen_format(output, review.total_outputs,
        screen_hash, &screen));
    assert(strcmp(screen.title, "OUTPUT 2/2") == 0);
    assert(strcmp(screen.amount, "2.00000000 ZCL") == 0);
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        item->bytes + item->output_end[1],
        item->length - item->output_end[1]));
    zcl_tx_stream_facts facts;
    uint8_t actual[32], expected[32];
    assert(blue_payment_review_finish(&review, script_code,
        sizeof script_code, 300000001, &facts, actual));
    assert(facts.inputs == 1 && facts.outputs == 2);
    assert(facts.output_zat == 300000000);
    zcl_zip243_hasher reference = zcl_zip243_host_hasher(&reference_context);
    assert(zcl_zip243_transparent_digest(item->bytes, item->length, 0,
        script_code, sizeof script_code, 300000001, 0x76b809bb,
        &reference, expected) == 0);
    assert(memcmp(actual, expected, sizeof actual) == 0);
    uint8_t rebound[32];
    assert(zcl_tx_replay_zip243_bound_digest(&review.replay,
        item->bytes + 9, UINT32_MAX - 1, script_code, 300000001,
        rebound));
    assert(memcmp(rebound, expected, sizeof rebound) == 0);
    script_code[0] = 0x6a;
    memset(rebound, 0x5a, sizeof rebound);
    assert(!zcl_tx_replay_zip243_bound_digest(&review.replay,
        item->bytes + 9, UINT32_MAX - 1, script_code, 300000001,
        rebound));
    for (size_t i = 0; i < sizeof rebound; ++i) assert(rebound[i] == 0x5a);
    assert(!blue_payment_review_acknowledge(&review));
    EVP_MD_CTX_free(sha_context);
}

static void test_failures(const fixture *item) {
    struct blake2b_ctx blake_context;
    EVP_MD_CTX *sha_context = EVP_MD_CTX_new();
    assert(sha_context);
    blue_payment_review review;
    zcl_tx_stream_facts facts;
    uint8_t digest[32];
    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    assert(!blue_payment_review_feed(&review,
        item->bytes + item->output_end[0], 1));
    assert(!blue_payment_review_pending(&review));
    assert(!blue_payment_review_acknowledge(&review));

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(!blue_payment_review_feed(&review, item->bytes,
        item->output_end[0] + 1));
    assert(!blue_payment_review_pending(&review));

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(!blue_payment_review_feed(&review, item->bytes,
        item->output_end[1]));
    assert(!blue_payment_review_pending(&review));

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    assert(!blue_payment_review_finish(&review, NULL, 0, 0,
        &facts, digest));
    for (size_t i = 0; i < sizeof digest; ++i) assert(!digest[i]);

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    uint8_t changed[sizeof item->bytes];
    memcpy(changed, item->bytes, item->length);
    changed[item->second_amount] ^= 1;
    assert(blue_payment_review_feed(&review, changed,
        item->output_end[0]));
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        changed + item->output_end[0],
        item->output_end[1] - item->output_end[0]));
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        changed + item->output_end[1],
        item->length - item->output_end[1]));
    assert(!blue_payment_review_finish(&review, NULL, 0, 0,
        &facts, digest));
    for (size_t i = 0; i < sizeof digest; ++i) assert(!digest[i]);

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    blue_payment_review_abort(&review);
    assert(!blue_payment_review_feed(&review, item->bytes, 1));
    EVP_MD_CTX_free(sha_context);
}

static void test_simulation(const fixture *item) {
    blue_payment_screen screens[BLUE_PAYMENT_REVIEW_MAX_OUTPUTS];
    uint32_t count = 0;
    assert(blue_payment_simulate(item->bytes, item->length, 0x76b809bb,
        screens, &count));
    assert(count == 2);
    assert(strcmp(screens[0].title, "OUTPUT 1/2") == 0);
    assert(strcmp(screens[0].amount, "1.00000000 ZCL") == 0);
    assert(strcmp(screens[1].title, "OUTPUT 2/2") == 0);
    assert(strcmp(screens[1].amount, "2.00000000 ZCL") == 0);
    uint8_t changed[sizeof item->bytes];
    memcpy(changed, item->bytes, item->length);
    changed[item->output_end[1] - 1] = 0x86;
    assert(!blue_payment_simulate(changed, item->length, 0x76b809bb,
        screens, &count));
    assert(count == 0 && screens[0].title[0] == 0);
    assert(!blue_payment_simulate(item->bytes, item->length - 1,
        0x76b809bb,
        screens, &count));
    assert(count == 0 && screens[0].title[0] == 0);
}

typedef struct {
    blue_payment_apdu state;
    blue_payment_owned_hashes owned;
    struct blake2b_ctx blake_context;
    EVP_MD_CTX *sha_context;
    zcl_zip243_hasher blake;
    zcl_tx_replay_sha256 sha;
} apdu_fixture;

static void apdu_init(apdu_fixture *session) {
    memset(session, 0, sizeof *session);
    session->sha_context = EVP_MD_CTX_new();
    assert(session->sha_context);
    session->blake = zcl_zip243_host_hasher(&session->blake_context);
    session->sha = (zcl_tx_replay_sha256){.context = session->sha_context,
        .init = sha_init, .update = sha_update, .final = sha_final};
    memset(session->owned.external, 0x33, 20);
    memset(session->owned.internal, 0x44, 20);
}

static uint16_t command(apdu_fixture *session, uint8_t instruction,
    const uint8_t *body, size_t length, uint8_t reply[8],
    size_t *reply_length) {
    assert(length <= 255);
    uint8_t apdu[260] = {0xa5, instruction, 0, 0, (uint8_t)length};
    if (length) memcpy(apdu + 5, body, length);
    return blue_payment_apdu_handle(&session->state, apdu, length + 5,
        reply, 8, reply_length, &session->blake, &session->sha,
        screen_hash, &session->owned);
}

static void apdu_begin(apdu_fixture *session, const fixture *item) {
    uint8_t body[12] = {0};
    uint32_t length = (uint32_t)item->length;
    for (unsigned i = 0; i < 4; ++i)
        body[i] = (uint8_t)(length >> (8 * i));
    const uint8_t branch[4] = {0xbb, 0x09, 0xb8, 0x76};
    memcpy(body + 8, branch, sizeof branch);
    uint8_t reply[8];
    size_t reply_length = 99;
    assert(command(session, 0x20, body, sizeof body, reply,
        &reply_length) == 0x9000);
    assert(reply_length == 0);
}

static void apdu_passes(apdu_fixture *session, const fixture *item) {
    uint8_t reply[8];
    size_t reply_length;
    for (unsigned i = 0; i < 2; ++i) {
        assert(command(session, 0x21, item->bytes, item->length,
            reply, &reply_length) == 0x9000);
        assert(reply_length == 2 && reply[1] == 0);
        assert(command(session, 0x22, NULL, 0,
            reply, &reply_length) == 0x9000);
        assert(reply_length == 2 && reply[0] == i + 2);
    }
    assert(reply[1] == 2);
}

static void test_apdu(const fixture *item) {
    apdu_fixture session;
    apdu_init(&session);
    assert(!blue_payment_apdu_touch_continue(&session.state));
    apdu_begin(&session, item);
    apdu_passes(&session, item);
    uint8_t reply[8];
    size_t reply_length;
    assert(command(&session, 0x21, item->bytes,
        item->output_end[0], reply, &reply_length) == 0x9000);
    assert(reply_length == 2 && reply[0] == 3 && reply[1] == 1);
    assert(strcmp(session.state.screen.title, "OUTPUT 1/2") == 0);
    assert(command(&session, 0x25, NULL, 0,
        reply, &reply_length) == 0x9000);
    assert(reply_length == 6 && reply[0] == 1 && reply[2] == 1);
    assert(blue_payment_apdu_touch_continue(&session.state));
    assert(session.state.screen.title[0] == 0);
    assert(command(&session, 0x21, item->bytes + item->output_end[0],
        item->output_end[1] - item->output_end[0],
        reply, &reply_length) == 0x9000);
    assert(reply[1] == 1 &&
        strcmp(session.state.screen.title, "OUTPUT 2/2") == 0);
    assert(blue_payment_apdu_touch_continue(&session.state));
    assert(command(&session, 0x21, item->bytes + item->output_end[1],
        item->length - item->output_end[1],
        reply, &reply_length) == 0x9000);
    assert(reply[1] == 0);
    assert(command(&session, 0x23, NULL, 0,
        reply, &reply_length) == 0x9000);
    assert(reply_length == 1 && reply[0] == 2);
    assert(command(&session, 0x25, NULL, 0,
        reply, &reply_length) == 0x9000);
    assert(reply_length == 6 && reply[0] == 0 && reply[3] == 1 &&
        reply[4] == 2 && reply[5] == 2);
    assert(!blue_payment_apdu_touch_continue(&session.state));
    EVP_MD_CTX_free(session.sha_context);
}

static void test_apdu_fail_closed(const fixture *item) {
    apdu_fixture session;
    apdu_init(&session);
    uint8_t wrong_branch[12] = {0};
    uint32_t wire_length = (uint32_t)item->length;
    for (unsigned i = 0; i < 4; ++i)
        wrong_branch[i] = (uint8_t)(wire_length >> (i * 8));
    uint8_t reply[8];
    size_t reply_length;
    assert(command(&session, 0x20, wrong_branch, sizeof wrong_branch,
        reply, &reply_length) == 0x6a80);
    assert(reply_length == 0 && !session.state.active);
    apdu_begin(&session, item);
    apdu_passes(&session, item);
    assert(command(&session, 0x21, item->bytes,
        item->output_end[0], reply, &reply_length) == 0x9000);
    assert(command(&session, 0x29, NULL, 0,
        reply, &reply_length) == 0x6d00);
    assert(!session.state.active && !session.state.review.pending);
    assert(session.state.screen.title[0] == 0);

    apdu_begin(&session, item);
    const uint8_t previous_length[4] = {85, 0, 0, 0};
    assert(command(&session, 0x26, previous_length,
        sizeof previous_length, reply, &reply_length) == 0x6985);
    assert(!session.state.active && !session.state.review.verified);

    apdu_begin(&session, item);
    apdu_passes(&session, item);
    assert(command(&session, 0x21, item->bytes,
        item->output_end[0], reply, &reply_length) == 0x9000);
    assert(command(&session, 0x21, item->bytes + item->output_end[0],
        1, reply, &reply_length) == 0x6a80);
    assert(!blue_payment_apdu_touch_continue(&session.state));

    apdu_begin(&session, item);
    apdu_passes(&session, item);
    assert(command(&session, 0x21, item->bytes,
        item->output_end[0] + 1, reply, &reply_length) == 0x6a80);
    assert(!session.state.active && !session.state.review.pending);
    apdu_begin(&session, item);
    assert(command(&session, 0x24, NULL, 0,
        reply, &reply_length) == 0x9000);
    assert(!session.state.active);
    apdu_begin(&session, item);
    uint8_t malformed[] = {0xa5, 0x21, 0, 0, 1};
    assert(blue_payment_apdu_handle(&session.state, malformed,
        sizeof malformed, reply, sizeof reply, &reply_length,
        &session.blake, &session.sha, screen_hash,
        &session.owned) == 0x6700);
    assert(reply_length == 0 && !session.state.active);
    apdu_begin(&session, item);
    apdu_passes(&session, item);
    blue_payment_apdu_abort(&session.state);
    assert(command(&session, 0x21, item->bytes, 1,
        reply, &reply_length) == 0x6985);
    assert(!session.state.active && session.state.screen.title[0] == 0);
    EVP_MD_CTX_free(session.sha_context);
}

static void test_apdu_mutations(void) {
    apdu_fixture session;
    apdu_init(&session);
    uint32_t random = 0x23c1a55u;
    for (unsigned run = 0; run < 10000; ++run) {
        uint8_t apdu[260] = {0xa5, (uint8_t)(0x20 + run % 9), 0, 0, 0};
        random = random * 1664525u + 1013904223u;
        apdu[4] = (uint8_t)(random >> 24);
        for (size_t i = 5; i < sizeof apdu; ++i) {
            random = random * 1664525u + 1013904223u;
            apdu[i] = (uint8_t)(random >> 24);
        }
        random = random * 1664525u + 1013904223u;
        size_t length = run % 3 == 0 ? (size_t)apdu[4] + 5 :
            random % (sizeof apdu + 1);
        uint8_t reply[8];
        memset(reply, 0xa5, sizeof reply);
        size_t reply_length = 99;
        uint16_t status = blue_payment_apdu_handle(&session.state, apdu,
            length, reply, sizeof reply, &reply_length, &session.blake,
            &session.sha, screen_hash, &session.owned);
        assert(reply_length <= sizeof reply);
        if (status != 0x9000)
            assert(reply_length == 0 && !session.state.active);
        for (size_t i = reply_length; i < sizeof reply; ++i)
            assert(reply[i] == 0xa5);
    }
    blue_payment_apdu_abort(&session.state);
    EVP_MD_CTX_free(session.sha_context);
}

typedef struct {
    apdu_fixture apdu;
    unsigned exchanges, continued;
    bool refuse_touch, wrong_identity, fail_previous_chunk, no_owned_hashes;
} live_fixture;

static bool live_exchange(void *context, const uint8_t *apdu,
    size_t apdu_length, uint8_t *reply, size_t capacity,
    size_t *reply_length) {
    live_fixture *live = context;
    ++live->exchanges;
    if (capacity < 7 || apdu_length < 5) return false;
    if (live->fail_previous_chunk && apdu[1] == 0x27) return false;
    if (apdu[1] == 0x01) {
        const uint8_t identity[7] = {'Z', 'C', 'L',
            live->wrong_identity ? 8 : 11, 15, 0x90, 0};
        memcpy(reply, identity, sizeof identity);
        *reply_length = sizeof identity;
        return true;
    }
    size_t payload = 0;
    uint16_t status = blue_payment_apdu_handle(&live->apdu.state,
        apdu, apdu_length, reply, capacity - 2, &payload,
        &live->apdu.blake, &live->apdu.sha, screen_hash,
        live->no_owned_hashes ? NULL : &live->apdu.owned);
    reply[payload] = (uint8_t)(status >> 8);
    reply[payload + 1] = (uint8_t)status;
    *reply_length = payload + 2;
    return true;
}

static bool live_continue(void *context, uint32_t index,
    const blue_payment_screen *screen) {
    live_fixture *live = context;
    assert(index == live->continued);
    assert(live->apdu.state.review.pending);
    assert(strcmp(screen->title, live->apdu.state.screen.title) == 0);
    assert(strcmp(screen->amount, live->apdu.state.screen.amount) == 0);
    assert(strcmp(screen->address, live->apdu.state.screen.address) == 0);
    if (live->refuse_touch) return false;
    ++live->continued;
    return blue_payment_apdu_touch_continue(&live->apdu.state);
}

static void test_live_driver(const fixture *item) {
    blue_payment_live_plan plan;
    assert(blue_payment_live_prepare(item->bytes, item->length,
        0x76b809bb, &plan));
    assert(plan.count == 2 && plan.wire_length == item->length);
    assert(plan.output_end[0] == item->output_end[0]);
    assert(plan.output_end[1] == item->output_end[1]);
    live_fixture live = {0};
    apdu_init(&live.apdu);
    assert(blue_payment_live_run(item->bytes, item->length, &plan,
        live_exchange, live_continue, &live));
    assert(live.continued == 2 && live.apdu.state.review.verified);
    assert(!live.apdu.state.active);
    EVP_MD_CTX_free(live.apdu.sha_context);

    uint8_t changed[sizeof item->bytes];
    memcpy(changed, item->bytes, item->length);
    changed[item->second_amount] ^= 1;
    live = (live_fixture){0};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run(changed, item->length, &plan,
        live_exchange, live_continue, &live));
    assert(live.exchanges == 0);
    live.wrong_identity = true;
    assert(!blue_payment_live_run(item->bytes, item->length, &plan,
        live_exchange, live_continue, &live));
    assert(live.exchanges == 1 && !live.apdu.state.active);
    live.wrong_identity = false;
    live.refuse_touch = true;
    assert(!blue_payment_live_run(item->bytes, item->length, &plan,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.active && !live.apdu.state.review.pending);
    EVP_MD_CTX_free(live.apdu.sha_context);
}

static void test_live_bound(void) {
    fixture spend = make_fixture();
    fixture previous = make_previous();
    uint8_t first[32], txid[32];
    assert(SHA256(previous.bytes, previous.length, first));
    assert(SHA256(first, sizeof first, txid));
    memcpy(spend.bytes + 9, txid, sizeof txid);
    blue_payment_live_plan plan;
    assert(blue_payment_live_prepare(spend.bytes, spend.length,
        0x76b809bb, &plan));
    assert(plan.inputs == 1);
    zcl_tx_previous_transaction source = {
        .wire = previous.bytes, .length = previous.length};
    uint8_t digests[ZCL_TX_PREFLIGHT_MAX_INPUTS][32];
    assert(expected_bound_digests(&spend, &source, 1, digests) ==
        100000000);
    live_fixture live = {0};
    apdu_init(&live.apdu);
    assert(blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(live.apdu.state.fee_ready &&
        live.apdu.state.bound_inputs == 1 &&
        live.apdu.state.fee_zat == 100000000);
    EVP_MD_CTX_free(live.apdu.sha_context);

    live = (live_fixture){0};
    apdu_init(&live.apdu);
    memset(live.apdu.owned.external, 0x55, 20);
    memset(live.apdu.owned.internal, 0x33, 20);
    assert(blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(live.apdu.state.fee_ready);
    EVP_MD_CTX_free(live.apdu.sha_context);

    live = (live_fixture){0};
    apdu_init(&live.apdu);
    memset(live.apdu.owned.external, 0x55, 20);
    memset(live.apdu.owned.internal, 0x66, 20);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready && !live.apdu.state.review.verified);
    EVP_MD_CTX_free(live.apdu.sha_context);

    live = (live_fixture){.no_owned_hashes = true};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready && !live.apdu.state.review.verified);
    EVP_MD_CTX_free(live.apdu.sha_context);

    live = (live_fixture){0};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000001, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready);
    EVP_MD_CTX_free(live.apdu.sha_context);

    digests[0][0] ^= 1;
    live = (live_fixture){0};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000,
        (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready);
    EVP_MD_CTX_free(live.apdu.sha_context);
    digests[0][0] ^= 1;

    live = (live_fixture){.fail_previous_chunk = true};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready && !live.apdu.state.review.verified);
    EVP_MD_CTX_free(live.apdu.sha_context);

    previous.bytes[previous.length - 1] ^= 1;
    live = (live_fixture){0};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, &source, 1, 100000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready && !live.apdu.state.review.verified);
    EVP_MD_CTX_free(live.apdu.sha_context);
}

static void test_live_two_inputs(void) {
    fixture spend = make_fixture();
    fixture first = make_previous();
    fixture second = make_previous();
    second.bytes[second.length - 1] = 1;
    assert(spend.length + 41 <= sizeof spend.bytes);
    memmove(spend.bytes + 91, spend.bytes + 50, spend.length - 50);
    memcpy(spend.bytes + 50, spend.bytes + 9, 41);
    spend.bytes[8] = 2;
    spend.length += 41;
    spend.output_end[0] += 41;
    spend.output_end[1] += 41;
    spend.second_amount += 41;
    uint8_t first_hash[32], txid[32];
    assert(SHA256(first.bytes, first.length, first_hash));
    assert(SHA256(first_hash, sizeof first_hash, txid));
    memcpy(spend.bytes + 9, txid, sizeof txid);
    assert(SHA256(second.bytes, second.length, first_hash));
    assert(SHA256(first_hash, sizeof first_hash, txid));
    memcpy(spend.bytes + 50, txid, sizeof txid);
    blue_payment_live_plan plan;
    assert(blue_payment_live_prepare(spend.bytes, spend.length,
        0x76b809bb, &plan));
    assert(plan.inputs == 2);
    zcl_tx_previous_transaction previous[2] = {
        {.wire = first.bytes, .length = first.length},
        {.wire = second.bytes, .length = second.length}
    };
    uint8_t digests[ZCL_TX_PREFLIGHT_MAX_INPUTS][32];
    assert(expected_bound_digests(&spend, previous, 2, digests) ==
        500000000);
    live_fixture live = {0};
    apdu_init(&live.apdu);
    assert(blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, previous, 2, 500000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(live.apdu.state.fee_ready &&
        live.apdu.state.bound_inputs == 2 &&
        live.apdu.state.input_zat == 800000000 &&
        live.apdu.state.fee_zat == 500000000);
    EVP_MD_CTX_free(live.apdu.sha_context);

    live = (live_fixture){0};
    apdu_init(&live.apdu);
    zcl_tx_previous_transaction reversed[2] = {previous[1], previous[0]};
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, reversed, 2, 500000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready);
    EVP_MD_CTX_free(live.apdu.sha_context);

    memcpy(spend.bytes + 50, spend.bytes + 9, 36);
    assert(blue_payment_live_prepare(spend.bytes, spend.length,
        0x76b809bb, &plan));
    live = (live_fixture){0};
    apdu_init(&live.apdu);
    assert(!blue_payment_live_run_bound(spend.bytes, spend.length,
        &plan, previous, 2, 500000000, (const uint8_t (*)[32])digests,
        live_exchange, live_continue, &live));
    assert(!live.apdu.state.fee_ready);
    EVP_MD_CTX_free(live.apdu.sha_context);
}

int main(int argc, char **argv) {
    fixture item = make_fixture();
    test_success(&item);
    test_failures(&item);
    test_simulation(&item);
    test_apdu(&item);
    test_apdu_fail_closed(&item);
    test_apdu_mutations();
    test_live_driver(&item);
    test_live_bound();
    test_live_two_inputs();
    if (argc == 2) {
        FILE *file = fopen(argv[1], "wb");
        assert(file);
        assert(fwrite(item.bytes, 1, item.length, file) == item.length);
        assert(fclose(file) == 0);
    }
    return 0;
}
