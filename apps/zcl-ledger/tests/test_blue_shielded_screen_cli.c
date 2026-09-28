/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L

#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int hex_digit(int ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static void write_vector(const char *source, const char *target) {
    FILE *input = fopen(source, "r");
    FILE *output = fopen(target, "wb");
    assert(input && output);
    int ch;
    do {
        ch = fgetc(input);
        if (ch == '#')
            while (ch != '\n' && ch != EOF) ch = fgetc(input);
    } while (ch == '\n');
    size_t length = 0;
    while (ch != '\n' && ch != EOF) {
        int high = hex_digit(ch);
        int low = hex_digit(fgetc(input));
        assert(high >= 0 && low >= 0);
        assert(fputc((high << 4) | low, output) != EOF);
        ++length;
        ch = fgetc(input);
    }
    assert(length == 4118);
    assert(!ferror(input) && fclose(input) == 0);
    assert(fclose(output) == 0);
}

static void run_simulator(const char *program, const char *wire,
    const char *prefix) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        FILE *sink = freopen("/dev/null", "w", stdout);
        if (!sink) _exit(126);
        execl(program, program, wire, "0x76b809bb", prefix,
            (char *)NULL);
        _exit(127);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void check_png(const char *prefix, const char *suffix) {
    char path[4096];
    int count = snprintf(path, sizeof path, "%s-%s.png", prefix, suffix);
    assert(count > 0 && (size_t)count < sizeof path);
    FILE *file = fopen(path, "rb");
    assert(file);
    uint8_t header[24];
    assert(fread(header, 1, sizeof header, file) == sizeof header);
    static const uint8_t expected[] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        0, 0, 1, 64, 0, 0, 1, 224
    };
    assert(memcmp(header, expected, sizeof expected) == 0);
    assert(fclose(file) == 0 && unlink(path) == 0);
}

static void check_pages(const char *prefix) {
    check_png(prefix, "ready");
    for (unsigned pass = 1; pass <= 6; ++pass) {
        char label[32];
        assert(snprintf(label, sizeof label, "pass-%u-start", pass) > 0);
        check_png(prefix, label);
        assert(snprintf(label, sizeof label, "pass-%u-complete", pass) > 0);
        check_png(prefix, label);
    }
    check_png(prefix, "summary");
    check_png(prefix, "digest");
    check_png(prefix, "summary-dark");
    check_png(prefix, "digest-dark");
    for (unsigned detail = 1; detail <= 6; ++detail) {
        for (unsigned page = 0; page < 2; ++page) {
            for (unsigned dark = 0; dark < 2; ++dark) {
                char label[48];
                assert(snprintf(label, sizeof label, "%s-large-%s-%u",
                    page ? "digest" : "summary",
                    dark ? "dark" : "light", detail) > 0);
                check_png(prefix, label);
            }
        }
    }
}

int main(int argc, char **argv) {
    assert(argc == 3);
    char directory[] = "/tmp/zcl-blue-screen-XXXXXX";
    assert(mkdtemp(directory));
    char wire[256], prefix[256];
    assert(snprintf(wire, sizeof wire, "%s/fixture.bin", directory) > 0);
    assert(snprintf(prefix, sizeof prefix, "%s/screen", directory) > 0);
    write_vector(argv[1], wire);
    run_simulator(argv[2], wire, prefix);
    check_pages(prefix);
    assert(unlink(wire) == 0 && rmdir(directory) == 0);
    puts("Blue shielded screen PNG sequence: 41 valid 320x480 images");
    return 0;
}
