/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "blue_payment_live.h"
#include "ledger_hid.h"
#include "zcl_tx_prevout.h"
#include "zcl_zip243_host.h"
#include "crypto/blake2b.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct { int fd; } live_device;

static bool sha256_bytes(const uint8_t *bytes, size_t length,
    uint8_t digest[32]) {
    return SHA256(bytes, length, digest) != NULL;
}

static void free_previous(uint8_t *bytes[ZCL_TX_PREFLIGHT_MAX_INPUTS],
    size_t count) {
    for (size_t i = 0; i < count; ++i) free(bytes[i]);
}

static bool parse_branch(const char *text, uint32_t *branch) {
    if (!text || strlen(text) != 8) return false;
    uint32_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        char digit = text[i];
        unsigned nibble;
        if (digit >= '0' && digit <= '9') nibble = (unsigned)(digit - '0');
        else if (digit >= 'a' && digit <= 'f')
            nibble = (unsigned)(digit - 'a' + 10);
        else if (digit >= 'A' && digit <= 'F')
            nibble = (unsigned)(digit - 'A' + 10);
        else return false;
        value = (value << 4) | nibble;
    }
    *branch = value;
    return true;
}

static bool read_exact(int fd, uint8_t *bytes, size_t length) {
    size_t position = 0;
    while (position < length) {
        ssize_t got = read(fd, bytes + position, length - position);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return false;
        position += (size_t)got;
    }
    uint8_t extra;
    return read(fd, &extra, 1) == 0;
}

static bool read_wire(const char *path, uint8_t **wire, size_t *length) {
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    struct stat info;
    bool valid = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
        info.st_size > 0 && info.st_size <= ZCL_TX_STREAM_MAX_BYTES;
    uint8_t *bytes = valid ? malloc((size_t)info.st_size) : NULL;
    if (!bytes) valid = false;
    size_t expected = valid ? (size_t)info.st_size : 0;
    if (valid) valid = read_exact(fd, bytes, expected);
    if (close(fd) != 0) valid = false;
    if (!valid) { free(bytes); return false; }
    *wire = bytes;
    *length = expected;
    return true;
}

static int open_blue(const char *path) {
    int fd = open(path, O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    struct hidraw_devinfo info;
    if (fd < 0) return -1;
    if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0 ||
        info.vendor != 0x2c97 || info.product != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static bool live_exchange(void *context, const uint8_t *apdu,
    size_t apdu_length, uint8_t *reply, size_t capacity,
    size_t *reply_length) {
    live_device *device = context;
    if (ledger_hid_exchange_timeout(device->fd, apdu, apdu_length,
            reply, capacity, reply_length, 5000) < 0) {
        fprintf(stderr, "Blue USB command %02x timed out or failed.\n",
            apdu_length >= 2 ? apdu[1] : 0);
        return false;
    }
    if (apdu_length >= 2 && *reply_length >= 2 &&
        (reply[*reply_length - 2] != 0x90 ||
         reply[*reply_length - 1] != 0 ||
         (apdu[1] != 0x21 && apdu[1] != 0x27 && apdu[1] != 0x25)))
        fprintf(stderr, "Blue USB command %02x replied %02x%02x.\n",
            apdu[1], reply[*reply_length - 2], reply[*reply_length - 1]);
    return true;
}

static int review_status(live_device *device, uint32_t index) {
    static const uint8_t command[] = {0xa5, 0x25, 0, 0, 0};
    uint8_t reply[16];
    size_t length = 0;
    if (!live_exchange(device, command, sizeof command, reply,
                       sizeof reply, &length) || length != 8 ||
        reply[0] != 1 || reply[1] != 3 || reply[3] != 0 ||
        reply[6] != 0x90 || reply[7] != 0) return -1;
    if (reply[2] == 0 && reply[5] == index + 1) return 1;
    if (reply[2] == 1 && reply[5] == index) return 0;
    return -1;
}

static bool wait_for_touch(void *context, uint32_t index,
    const blue_payment_screen *screen) {
    live_device *device = context;
    printf("\n%s  %s  %s\n%s\n",
        screen->title, screen->kind, screen->amount,
        screen->address);
    puts("Compare the full address on the Blue, then tap CONTINUE there.");
    fflush(stdout);
    for (unsigned attempt = 0; attempt < 600; ++attempt) {
        int status = review_status(device, index);
        if (status != 0) return status > 0;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 500000000};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc < 6 || argc > 5 + ZCL_TX_PREFLIGHT_MAX_INPUTS ||
        strcmp(argv[1], "--test") != 0) {
        fprintf(stderr, "Usage: %s --test /dev/hidrawN BRANCH_ID_HEX UNSIGNED_TX.bin PREVIOUS_TX.bin...\n",
            argv[0]);
        return 2;
    }
    uint32_t branch_id;
    uint8_t *wire = NULL;
    size_t length = 0;
    if (!parse_branch(argv[3], &branch_id) ||
        !read_wire(argv[4], &wire, &length)) {
        fputs("Expected an eight-digit branch ID and a regular unsigned transaction file.\n",
              stderr);
        return 1;
    }
    blue_payment_live_plan *plan = malloc(sizeof *plan);
    if (!plan || !blue_payment_live_prepare(wire, length, branch_id, plan)) {
        fputs("The transaction cannot be reviewed by this Blue Wallet.\n", stderr);
        free(plan);
        free(wire);
        return 1;
    }
    size_t previous_count = (size_t)argc - 5;
    uint8_t *previous_bytes[ZCL_TX_PREFLIGHT_MAX_INPUTS] = {0};
    zcl_tx_previous_transaction previous[ZCL_TX_PREFLIGHT_MAX_INPUTS] = {0};
    bool loaded = true;
    for (size_t i = 0; i < previous_count; ++i) {
        if (!read_wire(argv[i + 5], &previous_bytes[i],
                &previous[i].length)) {
            loaded = false;
            break;
        }
        previous[i].wire = previous_bytes[i];
    }
    zcl_tx_transparent_facts facts;
    uint8_t digests[ZCL_TX_PREFLIGHT_MAX_INPUTS][32];
    struct blake2b_ctx blake_context;
    zcl_zip243_hasher hasher = zcl_zip243_host_hasher(&blake_context);
    if (!loaded || zcl_tx_transparent_bound_digests(wire, length,
            previous, previous_count, branch_id, sha256_bytes, &hasher,
            &facts, digests, ZCL_TX_PREFLIGHT_MAX_INPUTS) < 0) {
        fputs("Input outpoints do not match previous transactions or digest calculation failed.\n",
              stderr);
        free_previous(previous_bytes, previous_count);
        free(plan);
        free(wire);
        return 1;
    }
    live_device device = {.fd = open_blue(argv[2])};
    if (device.fd < 0) {
        fputs("The selected interface is not an accessible Ledger Blue.\n",
              stderr);
        free_previous(previous_bytes, previous_count);
        free(plan);
        free(wire);
        return 1;
    }
    printf("Read-only test review: %u hash-bound input(s), %u output(s), "
           "fee %llu zatoshi. Chain inclusion and UTXO status are unverified.\n",
           facts.transparent_inputs, facts.transparent_outputs,
           (unsigned long long)facts.fee_zat);
    puts("The app cannot sign a payment.");
    fflush(stdout);
    bool valid = blue_payment_live_run_bound(wire, length, plan,
        previous, previous_count, facts.fee_zat,
        (const uint8_t (*)[32])digests,
        live_exchange, wait_for_touch, &device);
    close(device.fd);
    free_previous(previous_bytes, previous_count);
    free(plan);
    free(wire);
    if (!valid) {
        fputs("Blue review stopped; no payment was signed.\n", stderr);
        return 1;
    }
    puts("Blue matched every input digest and displayed the fee; NO SIGNING. Tap EXIT on the Blue.");
    return 0;
}
