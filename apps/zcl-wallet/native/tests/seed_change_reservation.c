/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "change_storage_fixture.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/* Existing caller-owned corpus directory, fixed exclusive filenames only.
 * All state belongs to the published zero-entropy test wallet. */
static int seed(int directory, const char *name, const uint8_t *first,
    size_t first_len, const uint8_t *second, size_t second_len, bool corrupt)
{
    uint8_t bytes[166] = {4}; /* Reservation harness raw-state mode. */
    CHECK(first_len <= 80 && second_len <= 80);
    memcpy(bytes + 6, first, first_len);
    memcpy(bytes + 6 + first_len, second, second_len);
    size_t length = 6 + first_len + second_len;
    if (corrupt) bytes[length - 1] ^= 1;
    int fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    ssize_t count = write(fd, bytes, length);
    int closed = close(fd);
    CHECK(count >= 0 && (size_t)count == length && closed == 0);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    change_storage_data data = {0};
    CHECK(change_data_init(&data) == 0);
    int directory = open(argv[1], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    CHECK(directory >= 0);
    int result = seed(directory, "authenticated-initial", data.state[0], 80, data.state[1], 0, false);
    if (result == 0) result = seed(directory, "authenticated-second", data.state[0], 80, data.state[1], 80, false);
    if (result == 0) result = seed(directory, "invalid-mac", data.state[0], 80, data.state[1], 0, true);
    if (result == 0) result = seed(directory, "misplaced-head", data.state[1], 80, data.state[0], 0, false);
    if (result == 0) result = seed(directory, "partial-successor", data.state[0], 80, data.state[1], 79, false);
    CHECK(close(directory) == 0);
    if (result != 0) return result;
    puts("Seeded five public authenticated/invalid/partial reservation records");
    return 0;
}
