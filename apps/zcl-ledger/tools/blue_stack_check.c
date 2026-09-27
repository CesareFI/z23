/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "The Blue stack check requires ISO C23"
#endif

enum { FRAME_COUNT = 39, REVIEW_FRAME_COUNT = 10, STACK_MARGIN = 512,
       PATH_COUNT = 12 };

static const char *const frame_names[FRAME_COUNT] = {
    "answer_command", "blue_review_app_command", "blue_review_handle",
    "zcl_zip243_shielded_digest", "zcl_tx_review_parse", "io_event",
    "show_latest", "blue_review_app_next", "blue_review_screen_output",
    "zcl_tx_outputs_visit", "main", "blue_wallet_handle",
    "blue_wallet_receive_split", "zcl_base58_encode",
    "wallet_payment_command", "blue_payment_apdu_handle", "dispatch",
    "blue_payment_review_feed", "zcl_tx_replay_zip243_feed_review",
    "zcl_tx_stream_feed", "output_seen", "capture_output",
    "blue_payment_screen_format", "output_address",
    "blue_payment_review_finish", "zcl_tx_replay_zip243_finish",
    "final_digest", "continue_review",
    "blue_payment_apdu_touch_continue", "blue_payment_review_acknowledge",
    "wallet_payment_display", "blue_payment_review_next_pass",
    "zcl_tx_replay_zip243_next", "zcl_tx_previous_stream_begin",
    "zcl_tx_previous_stream_feed", "zcl_tx_previous_stream_finish",
    "previous_compact_done", "previous_output_done", "capture_input"
};

typedef struct {
    unsigned bytes[FRAME_COUNT];
    bool present[FRAME_COUNT];
} stack_frames;

static bool read_stack_reserve(const char *path, unsigned *reserve) {
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof line, file)) {
        unsigned value;
        if (sscanf(line, " STACK_SIZE = %u", &value) == 1) {
            if (found) { fclose(file); return false; }
            *reserve = value;
            found = true;
        }
    }
    bool valid = found && !ferror(file);
    if (fclose(file) != 0) valid = false;
    return valid;
}

static bool record_frame(char *line, stack_frames *frames) {
    char *tab = strchr(line, '\t');
    if (!tab) return false;
    *tab = 0;
    char *name = strrchr(line, ':');
    if (!name) return false;
    ++name;
    for (unsigned i = 0; i < FRAME_COUNT; ++i) {
        if (strcmp(name, frame_names[i]) != 0) continue;
        char *end;
        errno = 0;
        unsigned long bytes = strtoul(tab + 1, &end, 10);
        if (errno || end == tab + 1 || *end != '\t' ||
            bytes > UINT_MAX || frames->present[i]) return false;
        frames->bytes[i] = (unsigned)bytes;
        frames->present[i] = true;
    }
    return true;
}

static bool read_stack_usage(const char *path, stack_frames *frames) {
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char line[512];
    bool valid = true;
    while (fgets(line, sizeof line, file)) {
        if (!strchr(line, '\n') || !record_frame(line, frames)) {
            valid = false;
            break;
        }
    }
    if (ferror(file)) valid = false;
    if (fclose(file) != 0) valid = false;
    return valid;
}

static unsigned sum_frames(const stack_frames *frames,
                           const unsigned *indices, size_t count) {
    unsigned sum = 0;
    for (size_t i = 0; i < count; ++i) sum += frames->bytes[indices[i]];
    return sum;
}

static bool collect_frames(int argc, char **argv, int script_arg,
                           bool wallet, stack_frames *frames,
                           unsigned *reserve) {
    if (!read_stack_reserve(argv[script_arg], reserve)) {
        fputs("Cannot read Blue stack reserve.\n", stderr);
        return false;
    }
    for (int i = script_arg + 1; i < argc; ++i)
        if (!read_stack_usage(argv[i], frames)) {
            fprintf(stderr, "Cannot read stack usage: %s\n", argv[i]);
            return false;
        }
    for (unsigned i = 0; i < FRAME_COUNT; ++i) {
        bool required = wallet ? i == 0 || i == 5 || i >= REVIEW_FRAME_COUNT
                               : i < REVIEW_FRAME_COUNT;
        if (required && !frames->present[i]) {
            fprintf(stderr, "Missing stack frame: %s\n", frame_names[i]);
            return false;
        }
    }
    return true;
}

static void report_paths(const stack_frames *frames, bool wallet,
                         unsigned reserve, unsigned paths[PATH_COUNT]) {
    static const unsigned apdu[] = {0, 1, 2, 3, 4};
    static const unsigned ui[] = {0, 5, 6, 7, 8, 9, 4};
    static const unsigned wallet_derive[] = {10, 13};
    static const unsigned wallet_layout[] = {10, 12};
    static const unsigned wallet_apdu[] = {0, 11};
    static const unsigned wallet_event[] = {5};
    static const unsigned wallet_upload[] = {0, 14, 15, 16, 17, 18,
        19, 20, 21, 38};
    static const unsigned wallet_format[] = {0, 14, 15, 16, 22, 23, 13};
    static const unsigned wallet_finish[] = {0, 14, 15, 16, 24, 25, 26};
    static const unsigned wallet_next[] = {0, 14, 15, 16, 31, 32};
    static const unsigned wallet_touch[] = {5, 27, 28, 29, 30};
    static const unsigned wallet_previous_begin[] = {0, 14, 15, 16, 33};
    static const unsigned wallet_previous_feed[] = {0, 14, 15, 16,
        34, 36, 37};
    static const unsigned wallet_previous_finish[] = {0, 14, 15, 16, 35};
    memset(paths, 0, sizeof(unsigned) * PATH_COUNT);
    if (wallet) {
        paths[0] = sum_frames(frames, wallet_derive, 2);
        paths[1] = sum_frames(frames, wallet_layout, 2);
        paths[2] = sum_frames(frames, wallet_apdu, 2);
        paths[3] = sum_frames(frames, wallet_event, 1);
        paths[4] = sum_frames(frames, wallet_upload,
            sizeof wallet_upload / sizeof *wallet_upload);
        paths[5] = sum_frames(frames, wallet_format,
            sizeof wallet_format / sizeof *wallet_format);
        paths[6] = sum_frames(frames, wallet_finish,
            sizeof wallet_finish / sizeof *wallet_finish);
        paths[7] = sum_frames(frames, wallet_next,
            sizeof wallet_next / sizeof *wallet_next);
        paths[8] = sum_frames(frames, wallet_touch,
            sizeof wallet_touch / sizeof *wallet_touch);
        paths[9] = sum_frames(frames, wallet_previous_begin,
            sizeof wallet_previous_begin / sizeof *wallet_previous_begin);
        paths[10] = sum_frames(frames, wallet_previous_feed,
            sizeof wallet_previous_feed / sizeof *wallet_previous_feed);
        paths[11] = sum_frames(frames, wallet_previous_finish,
            sizeof wallet_previous_finish / sizeof *wallet_previous_finish);
        printf("Blue stack reserve %u; derive %u; layout %u; receive APDU %u; event %u; payment upload %u; format %u; finish %u; next %u; touch %u; previous begin %u; feed %u; finish %u; margin %u\n",
               reserve, paths[0], paths[1], paths[2], paths[3], paths[4],
               paths[5], paths[6], paths[7], paths[8], paths[9], paths[10],
               paths[11], STACK_MARGIN);
    } else {
        paths[0] = sum_frames(frames, apdu, sizeof apdu / sizeof *apdu);
        paths[1] = sum_frames(frames, ui, sizeof ui / sizeof *ui);
        printf("Blue stack reserve %u; APDU path %u; screen path %u; margin %u\n",
               reserve, paths[0], paths[1], STACK_MARGIN);
    }
}

int main(int argc, char **argv) {
    bool wallet = argc > 1 && strcmp(argv[1], "--wallet") == 0;
    int script_arg = wallet ? 2 : 1;
    if (argc < script_arg + 2) {
        fprintf(stderr, "Usage: %s [--wallet] SDK_SCRIPT_LD FILE.su...\n",
                argv[0]);
        return 2;
    }
    unsigned reserve = 0, paths[PATH_COUNT];
    stack_frames frames = {0};
    if (!collect_frames(argc, argv, script_arg, wallet, &frames, &reserve))
        return 1;
    report_paths(&frames, wallet, reserve, paths);
    bool fits = reserve >= STACK_MARGIN;
    for (size_t i = 0; i < sizeof paths / sizeof *paths; ++i)
        if (fits && paths[i] > reserve - STACK_MARGIN) fits = false;
    if (!fits) {
        fputs("Blue stack budget exceeded; refusing image.\n", stderr);
        return 1;
    }
    return 0;
}
