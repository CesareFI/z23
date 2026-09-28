/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_shielded_review_app.h"
#include "blue_shielded_review_client.h"
#include "blue_sapling_fixture.h"
#include "zcl_zip243_host.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { BRANCH = 0x76b809bb };
enum {
    CORRUPT_SHORT = 1,
    CORRUPT_STATUS,
    CORRUPT_OVERSIZE,
    CORRUPT_BODY
};

typedef struct {
    blue_shielded_review_app app;
    struct blake2b_ctx blake;
    zcl_zip243_hasher hasher;
    unsigned calls, fail_at, erases, corrupt_at, corrupt_kind;
    bool alter_digest, alter_progress, old_version;
} simulator;

static void reset_simulator(simulator *sim) {
    memset(sim, 0, sizeof *sim);
    sim->hasher = zcl_zip243_host_hasher(&sim->blake);
    blue_shielded_review_app_reset(&sim->app);
}

static void alter_reply(const simulator *sim, uint8_t *reply,
    size_t body_length, size_t capacity, size_t *reply_length) {
    if (sim->calls != sim->corrupt_at) return;
    if (sim->corrupt_kind == CORRUPT_SHORT)
        *reply_length = body_length + 1;
    else if (sim->corrupt_kind == CORRUPT_STATUS)
        reply[body_length] ^= 1u;
    else if (sim->corrupt_kind == CORRUPT_OVERSIZE)
        *reply_length = capacity + 1;
    else reply[body_length ? body_length - 1 : body_length] ^= 1u;
}

static bool exchange(void *context, const uint8_t *apdu,
    size_t apdu_length, uint8_t *reply, size_t capacity,
    size_t *reply_length) {
    simulator *sim = context;
    ++sim->calls;
    if (apdu_length >= 2 && apdu[1] == 0x24) ++sim->erases;
    if (sim->calls == sim->fail_at || capacity < 2) return false;
    size_t body_length = 0;
    uint16_t status = blue_shielded_review_app_command(&sim->app,
        apdu, apdu_length, reply, capacity - 2,
        &body_length, &sim->hasher);
    if (body_length + 2 > capacity) return false;
    if (status == 0x9000 && apdu[1] == 0x23 &&
        sim->alter_digest) reply[44] ^= 1u;
    if (status == 0x9000 && apdu[1] == 0x21 &&
        sim->alter_progress) reply[1] ^= 1u;
    if (status == 0x9000 && apdu[1] == 0x01 &&
        sim->old_version) reply[3] = 6;
    reply[body_length] = (uint8_t)(status >> 8);
    reply[body_length + 1] = (uint8_t)status;
    *reply_length = body_length + 2;
    alter_reply(sim, reply, body_length, capacity, reply_length);
    return true;
}

static void assert_zero(const zcl_tx_review *review,
    const uint8_t digest[32]) {
    const uint8_t *bytes = (const uint8_t *)review;
    for (size_t i = 0; i < sizeof *review; ++i) assert(bytes[i] == 0);
    for (unsigned i = 0; i < 32; ++i) assert(digest[i] == 0);
}

static void successful_review(const uint8_t *wire) {
    simulator sim;
    reset_simulator(&sim);
    zcl_tx_review review;
    uint8_t digest[32];
    assert(blue_shielded_review_client_run(wire,
        BLUE_SYNTHETIC_SAPLING_BYTES, BRANCH,
        exchange, &sim, &review, digest));
    assert(review.sapling_spends == 1 && review.sapling_outputs == 1);
    assert(sim.app.transaction.complete && !sim.erases);
    assert(memcmp(digest, sim.app.transaction.digest, 32) == 0);
    assert(sim.calls == 50);
}

static void failed_review(const uint8_t *wire,
    unsigned fail_at, bool alter_digest, bool alter_progress,
    bool old_version) {
    simulator sim;
    reset_simulator(&sim);
    sim.fail_at = fail_at;
    sim.alter_digest = alter_digest;
    sim.alter_progress = alter_progress;
    sim.old_version = old_version;
    zcl_tx_review review;
    uint8_t digest[32];
    memset(&review, 0xa5, sizeof review);
    memset(digest, 0xa5, sizeof digest);
    assert(!blue_shielded_review_client_run(wire,
        BLUE_SYNTHETIC_SAPLING_BYTES, BRANCH,
        exchange, &sim, &review, digest));
    assert_zero(&review, digest);
    assert(sim.erases == 1);
    if (fail_at) assert(sim.calls == fail_at + 1);
    assert(!sim.app.transaction.active && !sim.app.transaction.complete);
}

static void corrupted_reply(const uint8_t *wire,
    unsigned call, unsigned kind) {
    simulator sim;
    reset_simulator(&sim);
    sim.corrupt_at = call;
    sim.corrupt_kind = kind;
    zcl_tx_review review;
    uint8_t digest[32];
    memset(&review, 0xa5, sizeof review);
    memset(digest, 0xa5, sizeof digest);
    assert(!blue_shielded_review_client_run(wire,
        BLUE_SYNTHETIC_SAPLING_BYTES, BRANCH,
        exchange, &sim, &review, digest));
    assert_zero(&review, digest);
    assert(sim.calls == call + 1 && sim.erases == 1);
    assert(!sim.app.transaction.active && !sim.app.transaction.complete);
}

static int hex_digit(int ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static size_t read_vector(const char *path, uint8_t wire[8192]) {
    FILE *file = fopen(path, "r");
    assert(file);
    int ch;
    do {
        ch = fgetc(file);
        if (ch == '#') {
            while (ch != '\n' && ch != EOF) ch = fgetc(file);
        }
    } while (ch == '#' || ch == '\n');
    size_t length = 0;
    while (ch != EOF && ch != '\n') {
        int high = hex_digit(ch);
        int low = hex_digit(fgetc(file));
        assert(high >= 0 && low >= 0 && length < 8192);
        wire[length++] = (uint8_t)((high << 4) | low);
        ch = fgetc(file);
    }
    assert(!ferror(file) && fclose(file) == 0);
    return length;
}

static void published_vector(const char *path) {
    static uint8_t wire[8192];
    size_t length = read_vector(path, wire);
    assert(length == 4118);
    simulator sim;
    reset_simulator(&sim);
    zcl_tx_review review;
    uint8_t digest[32];
    static const uint8_t expected[32] = {
        0x63, 0xd1, 0x85, 0x34, 0xde, 0x5f, 0x2d, 0x1c,
        0x9e, 0x16, 0x9b, 0x73, 0xf9, 0xc7, 0x83, 0x71,
        0x8a, 0xdb, 0xef, 0x5c, 0x8a, 0x7d, 0x55, 0xb5,
        0xe7, 0xa3, 0x7a, 0xff, 0xa1, 0xdd, 0x3f, 0xf3
    };
    assert(blue_shielded_review_client_run(wire, length, BRANCH,
        exchange, &sim, &review, digest));
    assert(review.sapling_spends == 3 && review.sapling_outputs == 3);
    assert(memcmp(digest, expected, sizeof expected) == 0);
}

static void consensus_spend_vector(const char *path) {
    static uint8_t wire[8192];
    static const uint8_t expected[32] = {
        0xd4, 0x96, 0x7a, 0x82, 0x69, 0x00, 0x77, 0x09,
        0xfd, 0x06, 0x3a, 0x59, 0x2f, 0x73, 0x59, 0xb8,
        0x64, 0xfa, 0x39, 0x0c, 0x76, 0xf4, 0x60, 0x9d,
        0xc9, 0xf9, 0xb9, 0x60, 0x69, 0xc2, 0x7c, 0x8b
    };
    size_t length = read_vector(path, wire);
    assert(length == BLUE_SYNTHETIC_SAPLING_BYTES);
    simulator sim;
    reset_simulator(&sim);
    zcl_tx_review review;
    uint8_t digest[32];
    assert(blue_shielded_review_client_run(wire, length, BRANCH,
        exchange, &sim, &review, digest));
    assert(review.sapling_spends == 1 && review.sapling_outputs == 1);
    assert(sim.app.transaction.complete && sim.calls == 50);
    assert(memcmp(digest, expected, sizeof expected) == 0);
    assert(memcmp(sim.app.transaction.digest, expected,
        sizeof expected) == 0);
    assert(strcmp(sim.app.lines[2], "SHIELDED SPEND/OUT: 1/1") == 0);
    assert(blue_shielded_review_app_next(&sim.app));
    assert(strcmp(sim.app.lines[0], "ZIP243 DIGEST") == 0);
    blue_shielded_review_app_toggle_text(&sim.app);
    blue_shielded_review_app_toggle_dark(&sim.app);
    assert(sim.app.large_text && sim.app.dark);
    blue_shielded_review_app_reset(&sim.app);
    assert(!sim.app.transaction.complete);
    assert(strcmp(sim.app.lines[0], "CONNECT Z23") == 0);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    uint8_t wire[BLUE_SYNTHETIC_SAPLING_BYTES];
    blue_sapling_fixture(wire);
    successful_review(wire);
    for (unsigned call = 1; call <= 50; ++call)
        failed_review(wire, call, false, false, false);
    for (unsigned call = 1; call <= 50; ++call)
        for (unsigned kind = CORRUPT_SHORT;
             kind <= CORRUPT_BODY; ++kind)
            corrupted_reply(wire, call, kind);
    failed_review(wire, 0, true, false, false);
    failed_review(wire, 0, false, true, false);
    failed_review(wire, 0, false, false, true);
    simulator sim;
    reset_simulator(&sim);
    zcl_tx_review review = {0};
    uint8_t digest[32] = {0};
    assert(!blue_shielded_review_client_run(wire,
        BLUE_SYNTHETIC_SAPLING_BYTES, 0,
        exchange, &sim, &review, digest));
    assert(sim.calls == 0);
    wire[0] ^= 1u;
    assert(!blue_shielded_review_client_run(wire,
        BLUE_SYNTHETIC_SAPLING_BYTES, BRANCH,
        exchange, &sim, &review, digest));
    assert(sim.calls == 0);
    published_vector(argv[1]);
    consensus_spend_vector(argv[2]);
    puts("Blue shielded client replay, disconnect and substitutions: passed");
    return 0;
}
