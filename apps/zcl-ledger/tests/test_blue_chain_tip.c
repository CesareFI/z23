/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "blue_chain_tip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#undef NDEBUG
#include <assert.h>

static const char good[] =
    "{\"result\":{\"chain\":\"main\",\"blocks\":707001,"
    "\"headers\":707001,\"bestblockhash\":"
    "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"},"
    "\"error\":null,\"id\":1}";

static bool accepts(const char *reply) {
    uint32_t height = 0;
    return blue_chain_tip_parse(reply, strlen(reply), &height) &&
           height == 707002;
}

static bool accepts_replacement(const char *old, const char *new_text) {
    const char *at = strstr(good, old);
    assert(at);
    char changed[512];
    size_t prefix = (size_t)(at - good);
    size_t replacement = strlen(new_text);
    size_t suffix = strlen(at + strlen(old));
    assert(prefix + replacement + suffix < sizeof changed);
    memcpy(changed, good, prefix);
    memcpy(changed + prefix, new_text, replacement);
    memcpy(changed + prefix + replacement, at + strlen(old), suffix + 1);
    return accepts(changed);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "getblockchaininfo") == 0) {
        if (getenv("BLUE_CHAIN_TIP_TEST_STALL")) {
            struct timespec delay = {.tv_sec = 9};
            nanosleep(&delay, NULL);
        }
        puts(good);
        return 0;
    }
    assert(argc == 1);
    assert(accepts(good));
    assert(!accepts_replacement("\"main\"", "\"test\""));
    assert(!accepts_replacement("\"blocks\":707001,",
                                "\"blocks\":707001,\"blocks\":707001,"));
    assert(!accepts_replacement("\"headers\":707001,",
                                "\"headers\":707002,"));
    assert(!accepts_replacement("\"headers\":707001,",
                                "\"headers\":707001,\"initialblockdownload\":true,"));
    assert(!accepts_replacement("\"error\":null", "\"error\":{}"));
    assert(!accepts_replacement("abcdef", "abcdeg"));
    assert(!accepts_replacement("\"chain\":\"main\",", ""));
    assert(!accepts("{\"result\":null,\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"test\",\"blocks\":1,"
                    "\"headers\":1},\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"main\",\"blocks\":1,"
                    "\"headers\":2,\"bestblockhash\":\"00\"},\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"main\",\"blocks\":1,"
                    "\"headers\":1,\"bestblockhash\":\"00\"},\"error\":null}"));
    uint32_t height = 0;
    assert(blue_chain_tip_query(argv[0], &height));
    assert(height == 707002);
    assert(!blue_chain_tip_query("relative/path", &height));
    assert(!blue_chain_tip_query("/bin/false", &height));
    assert(setenv("BLUE_CHAIN_TIP_TEST_STALL", "1", 1) == 0);
    assert(!blue_chain_tip_query(argv[0], &height));
    assert(unsetenv("BLUE_CHAIN_TIP_TEST_STALL") == 0);
    return 0;
}
