/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 */
/* purpose: verify bounded QA frame arguments and unchanged output on refusal. */
#include "test/test_core.h"
#include "sky_combat/qa_frames.h"

int test_skycombat_qa_frames(void);
int test_skycombat_qa_frames(void)
{
    int failures = 0;
    TEST("QA frames: absent argument preserves interactive selection") {
        char *args[] = {"skycombat", NULL};
        unsigned out = 99;
        ASSERT(sky_combat_qa_frames_parse(1, args, &out));
        ASSERT_EQ(out, 0u);
    }
    TEST("QA frames: canonical lower, ordinary and upper bounds") {
        const struct { char *arg; unsigned expected; } valid[] = {
            {"--qa-frames=1", 1}, {"--qa-frames=60", 60},
            {"--qa-frames=3599", 3599}, {"--qa-frames=3600", 3600}
        };
        for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
            char *args[] = {"skycombat", valid[i].arg};
            unsigned out = 99;
            ASSERT(sky_combat_qa_frames_parse(2, args, &out));
            ASSERT_EQ(out, valid[i].expected);
        }
    }
    TEST("QA frames: malformed, noncanonical and overflowing spans refuse unchanged") {
        char *invalid[] = {
            "--qa-frames=", "--qa-frames=0", "--qa-frames=01",
            "--qa-frames=-1", "--qa-frames=+1", "--qa-frames= 1", "--qa-frames=1 ",
            "--qa-frames=1x", "--qa-frames=1.0", "--qa-frames=3601",
            "--qa-frames=4294967296", "--qa-frames=99999999999999999999999999",
            "--qa-frames", "--help", "", "--qa-frames=1\n"
        };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            char *args[] = {"skycombat", invalid[i]};
            unsigned out = 99;
            ASSERT(!sky_combat_qa_frames_parse(2, args, &out));
            ASSERT_EQ(out, 99u);
        }
    }
    TEST("QA frames: duplicate, missing and invalid call shape refuse unchanged") {
        char *args[] = {"skycombat", "--qa-frames=1", "--qa-frames=2"};
        unsigned out = 99;
        ASSERT(!sky_combat_qa_frames_parse(3, args, &out));
        ASSERT_EQ(out, 99u);
        ASSERT(!sky_combat_qa_frames_parse(2, args, NULL));
        ASSERT(!sky_combat_qa_frames_parse(0, args, &out));
        ASSERT_EQ(out, 99u);
        args[1] = NULL;
        ASSERT(!sky_combat_qa_frames_parse(2, args, &out));
        ASSERT_EQ(out, 99u);
        ASSERT(!sky_combat_qa_frames_parse(1, NULL, &out));
        ASSERT_EQ(out, 99u);
    }
_test_next:;
    return failures;
}
