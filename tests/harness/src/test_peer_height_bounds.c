/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Peer-advertised heights must not overflow sync eligibility arithmetic. */
#include "test/test_core.h"
#include "net/net.h"
#include "net/p2p_message.h"
#include "net/version.h"
#include "sync/sync_planner.h"

/* Exercise the real wire codec. This helper owns and releases the stream on
 * every outcome; the caller receives only a successfully decoded value. */
static bool height_wire_roundtrip(int32_t height, int32_t *decoded)
{
    struct version_message wire, parsed;
    struct byte_stream stream;
    version_message_init(&wire);
    version_message_init(&parsed);
    wire.protocol_version = PROTOCOL_VERSION;
    wire.start_height = height;
    stream_init(&stream, 128);
    bool ok = !stream.error && version_message_serialize(&wire, &stream) &&
              version_message_deserialize(&parsed, &stream);
    if (ok)
        *decoded = parsed.start_height;
    stream_free(&stream);
    return ok;
}

static bool height_eligibility_cases(void)
{
    const struct {
        int32_t advertised;
        int local;
        bool behind;
    } cases[] = {
        {INT32_MAX, 10000, false},
        {INT32_MAX - SYNC_PEER_BEHIND_TOLERANCE + 1, 10000, false},
        {INT32_MAX - SYNC_PEER_BEHIND_TOLERANCE, INT32_MAX, false},
        {INT32_MAX - SYNC_PEER_BEHIND_TOLERANCE - 1, INT32_MAX, true},
        {10000, 10000, false},
        {10001, 10000, false},
        {10000 - SYNC_PEER_BEHIND_TOLERANCE, 10000, false},
        {10000 - SYNC_PEER_BEHIND_TOLERANCE - 1, 10000, true},
        {0, SYNC_PEER_BEHIND_TOLERANCE, false},
        {0, SYNC_PEER_BEHIND_TOLERANCE + 1, true},
        {-1, 10000, false},
        {INT32_MIN, 10000, false},
    };
    struct p2p_node node = {0};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int32_t decoded = 0;
        if (!height_wire_roundtrip(cases[i].advertised, &decoded) ||
            decoded != cases[i].advertised)
            return false;
        node.starting_height = decoded;
        if (syncsvc_peer_is_behind(&node, cases[i].local) != cases[i].behind) {
            fprintf(stderr, "peer height case %zu: advertised=%" PRId32
                            " local=%d behind=%d\n", i, decoded,
                    cases[i].local, cases[i].behind);
            return false;
        }
    }
    return !syncsvc_peer_is_behind(NULL, 10000);
}

static int height_sync_plans_test(void)
{
    int failures = 0;
    TEST("peer height: upper wire value remains eligible for sync planning") {
        struct p2p_node node = {0};
        struct sync_block_assignment plan = {0};
        int32_t decoded = 0;
        ASSERT(height_wire_roundtrip(INT32_MAX, &decoded));
        ASSERT(decoded == INT32_MAX);
        node.starting_height = decoded;
        node.state = PEER_SYNCING_HEADERS;
        ASSERT(syncsvc_should_request_headers(&node, 10000, 1000));
        syncsvc_plan_block_assignment(&plan, &node, 0, 10000);
        ASSERT(plan.should_assign);
        ASSERT(plan.max_assign > 0);
        PASS();
    } _test_next:;
    return failures;
}

int test_peer_height_bounds(void);

int test_peer_height_bounds(void)
{
    int failures = 0;
    TEST("peer height: wire extremes and exact behind-policy boundaries") {
        ASSERT(height_eligibility_cases());
        PASS();
    } _test_next:;
    failures += height_sync_plans_test();
    return failures;
}
