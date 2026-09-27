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

enum { FRAME_COUNT = 10, STACK_MARGIN = 512 };

static const char *const frame_names[FRAME_COUNT] = {
    "answer_command", "blue_review_app_command", "blue_review_handle",
    "zcl_zip243_shielded_digest", "zcl_tx_review_parse", "io_event",
    "show_latest", "blue_review_app_next", "blue_review_screen_output",
    "zcl_tx_outputs_visit"
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

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s SDK_SCRIPT_LD FILE.su...\n", argv[0]);
        return 2;
    }
    unsigned reserve = 0;
    stack_frames frames = {0};
    if (!read_stack_reserve(argv[1], &reserve)) {
        fputs("Cannot read Blue stack reserve.\n", stderr);
        return 1;
    }
    for (int i = 2; i < argc; ++i)
        if (!read_stack_usage(argv[i], &frames)) {
            fprintf(stderr, "Cannot read stack usage: %s\n", argv[i]);
            return 1;
        }
    for (unsigned i = 0; i < FRAME_COUNT; ++i)
        if (!frames.present[i]) {
            fprintf(stderr, "Missing stack frame: %s\n", frame_names[i]);
            return 1;
        }
    static const unsigned apdu[] = {0, 1, 2, 3, 4};
    static const unsigned ui[] = {0, 5, 6, 7, 8, 9, 4};
    unsigned apdu_bytes = sum_frames(&frames, apdu, sizeof apdu / sizeof *apdu);
    unsigned ui_bytes = sum_frames(&frames, ui, sizeof ui / sizeof *ui);
    printf("Blue stack reserve %u; APDU path %u; screen path %u; margin %u\n",
           reserve, apdu_bytes, ui_bytes, STACK_MARGIN);
    if (reserve < STACK_MARGIN || apdu_bytes > reserve - STACK_MARGIN ||
        ui_bytes > reserve - STACK_MARGIN) {
        fputs("Blue stack budget exceeded; refusing image.\n", stderr);
        return 1;
    }
    return 0;
}
