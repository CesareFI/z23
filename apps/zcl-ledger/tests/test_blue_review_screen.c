/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_review_screen.h"
#include "blue_review_protocol.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

int main(void) {
    zcl_tx_review review = {
        .transparent_inputs = 2, .transparent_outputs = 3,
        .sapling_spends = 1, .sapling_outputs = 4,
        .sprout_joinsplits = 0, .transparent_output_zat = 39062500
    };
    uint8_t reply[76] = {0};
    blue_review_encode_summary(&review, reply);
    memcpy(reply + 44, (const uint8_t[]){0x63, 0xd1, 0x85, 0x34,
                                       0xde, 0x5f, 0x2d, 0x1c}, 8);
    char lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE];
    assert(blue_review_screen_format(reply, lines));
    assert(strcmp(lines[0], "T INPUT/OUT: 2/3") == 0);
    assert(strcmp(lines[1], "PUB ZAT: 39062500") == 0);
    assert(strcmp(lines[2], "SAP S/O: 1/4") == 0);
    assert(strcmp(lines[3], "SPROUT JS: 0") == 0);
    assert(strcmp(lines[4], "NO SIGNING; SHIELDED HIDDEN") == 0);
    assert(strcmp(lines[5], "TX SHA256: 63d18534de5f2d1c") == 0);
    assert(!blue_review_screen_format(NULL, lines));
    assert(!blue_review_screen_format(reply, NULL));
    review.transparent_output_zat = UINT64_MAX;
    blue_review_encode_summary(&review, reply);
    assert(blue_review_screen_format(reply, lines));
    assert(strcmp(lines[1], "PUB ZAT: 18446744073709551615") == 0);
    return 0;
}
