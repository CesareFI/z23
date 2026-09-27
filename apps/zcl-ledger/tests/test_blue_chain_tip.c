/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "blue_chain_tip.h"
#include "blue_utxo.h"

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
static const char txid[] =
    "0000000000000000000000000000000000000000000000000000000000000000";
static const char utxo[] =
    "{\"result\":{\"txid\":\"0000000000000000000000000000000000000000000000000000000000000000\","
    "\"coinbase\":false,\"height\":700000,\"num_outputs\":1,"
    "\"outputs\":[{\"n\":0,\"spent\":false,\"amount\":\"4.00000000\","
    "\"script_size\":25}]},\"error\":null}";

static bool accepts(const char *reply) {
    blue_chain_tip tip;
    return blue_chain_tip_parse(reply, strlen(reply), &tip) &&
           tip.next_height == 707002;
}

static void replace_once(const char *source, const char *old,
                         const char *new_text, char changed[512]) {
    const char *at = strstr(source, old);
    assert(at);
    size_t prefix = (size_t)(at - source);
    size_t replacement = strlen(new_text);
    size_t suffix = strlen(at + strlen(old));
    assert(prefix + replacement + suffix < 512);
    memcpy(changed, source, prefix);
    memcpy(changed + prefix, new_text, replacement);
    memcpy(changed + prefix + replacement, at + strlen(old), suffix + 1);
}

static bool accepts_replacement(const char *old, const char *new_text) {
    char changed[512];
    replace_once(good, old, new_text, changed);
    return accepts(changed);
}

static bool utxo_replacement(const char *old, const char *new_text) {
    char changed[512];
    replace_once(utxo, old, new_text, changed);
    return blue_utxo_parse(changed, strlen(changed), txid, 0,
                           400000000, 25, 707002);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "getblockchaininfo") == 0) {
        if (getenv("BLUE_CHAIN_TIP_TEST_STALL")) {
            struct timespec delay = {.tv_sec = 9};
            nanosleep(&delay, NULL);
        }
        const char *marker = getenv("BLUE_TIP_REORG_MARKER");
        if (marker) {
            FILE *file = fopen(marker, "r");
            if (file) {
                assert(fclose(file) == 0);
                char changed[sizeof good];
                memcpy(changed, good, sizeof good);
                char *hash = strstr(changed, "0123456789abcdef");
                assert(hash);
                hash[0] = '1';
                puts(changed);
                return 0;
            }
            file = fopen(marker, "w");
            assert(file && fclose(file) == 0);
        }
        puts(good);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "gettxdetail") == 0) {
        const char *spent = getenv("BLUE_UTXO_TEST_SPENT") ? "true" : "false";
        printf("{\"result\":{\"txid\":%s,\"coinbase\":false,"
               "\"height\":700000,\"num_outputs\":1,\"outputs\":["
               "{\"n\":0,\"spent\":%s,\"amount\":\"4.00000000\","
               "\"script_size\":25}]},\"error\":null,\"id\":1}\n",
               argv[2], spent);
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
    assert(blue_utxo_parse(utxo, strlen(utxo), txid, 0,
                           400000000, 25, 707002));
    assert(!utxo_replacement("\"spent\":false", "\"spent\":true"));
    assert(!utxo_replacement("4.00000000", "4.00000001"));
    assert(!utxo_replacement("\"height\":700000",
                             "\"height\":707002"));
    assert(!utxo_replacement("\"script_size\":25",
                             "\"script_size\":24"));
    assert(!utxo_replacement("\"coinbase\":false,\"height\":700000",
                             "\"coinbase\":true,\"height\":706950"));
    assert(!utxo_replacement("\"n\":0,", "\"n\":0,\"n\":0,"));
    assert(!accepts("{\"result\":null,\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"test\",\"blocks\":1,"
                    "\"headers\":1},\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"main\",\"blocks\":1,"
                    "\"headers\":2,\"bestblockhash\":\"00\"},\"error\":null}"));
    assert(!accepts("{\"result\":{\"chain\":\"main\",\"blocks\":1,"
                    "\"headers\":1,\"bestblockhash\":\"00\"},\"error\":null}"));
    blue_chain_tip tip, same;
    assert(blue_chain_tip_query(argv[0], &tip));
    assert(tip.next_height == 707002);
    assert(blue_chain_tip_parse(good, strlen(good), &same));
    assert(blue_chain_tip_same(&tip, &same));
    assert(!blue_chain_tip_query("relative/path", &tip));
    assert(!blue_chain_tip_query("/bin/false", &tip));
    assert(setenv("BLUE_CHAIN_TIP_TEST_STALL", "1", 1) == 0);
    assert(!blue_chain_tip_query(argv[0], &tip));
    assert(unsetenv("BLUE_CHAIN_TIP_TEST_STALL") == 0);
    return 0;
}
