/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#undef NDEBUG
#include <assert.h>

static int run(const char *program, const char *rpc_binary, const char *output,
    const char *spend, const char *previous, char *error, size_t capacity) {
    int pipefd[2];
    assert(pipe(pipefd) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(pipefd[0]);
        assert(dup2(pipefd[1], STDERR_FILENO) >= 0);
        close(pipefd[1]);
        if (output)
            execl(program, program, output, (char *)NULL);
        else if (previous)
            execl(program, program, "--test", "/dev/hidraw999",
                  rpc_binary, spend, previous, (char *)NULL);
        else
            execl(program, program, "--test", "/dev/hidraw999",
                  rpc_binary, spend, (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    size_t used = 0;
    while (used + 1 < capacity) {
        ssize_t count = read(pipefd[0], error + used, capacity - used - 1);
        if (count <= 0) break;
        used += (size_t)count;
    }
    error[used] = 0;
    close(pipefd[0]);
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static size_t read_file(const char *path, uint8_t bytes[256]) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t length = fread(bytes, 1, 256, file);
    assert(!ferror(file) && length > 80 && length < 256);
    assert(fclose(file) == 0);
    return length;
}

static void write_file(const char *path, const uint8_t *bytes, size_t length) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(bytes, 1, length, file) == length);
    assert(fclose(file) == 0);
}

static size_t previous_version(const uint8_t *fixture, size_t length,
    unsigned version, uint8_t previous[256]) {
    assert(length > 80 && length < 256 && version >= 1 && version <= 4);
    for (size_t i = length - 11; i < length; ++i) assert(fixture[i] == 0);
    if (version == 4) {
        memcpy(previous, fixture, length);
        return length;
    }
    size_t body_length = length - 8 - (version == 3 ? 11 : 15);
    size_t header_length = version == 3 ? 8 : 4;
    uint32_t header = version == 3 ? 0x80000003u : version;
    for (unsigned i = 0; i < 4; ++i)
        previous[i] = (uint8_t)(header >> (8 * i));
    if (version == 3) {
        uint32_t group = 0x03c48270u;
        for (unsigned i = 0; i < 4; ++i)
            previous[4 + i] = (uint8_t)(group >> (8 * i));
    }
    memcpy(previous + header_length, fixture + 8, body_length);
    size_t result = header_length + body_length;
    if (version >= 2) previous[result++] = 0;
    return result;
}

int main(int argc, char **argv) {
    assert(argc == 4);
    char directory[] = "/tmp/zcl-wallet-review-cli-XXXXXX";
    assert(mkdtemp(directory));
    char fixture[256], spend_path[256], previous_path[256], error[512];
    assert(snprintf(fixture, sizeof fixture, "%s/fixture.bin", directory) > 0);
    assert(snprintf(spend_path, sizeof spend_path, "%s/spend.bin", directory) > 0);
    assert(snprintf(previous_path, sizeof previous_path,
                    "%s/previous.bin", directory) > 0);
    assert(run(argv[1], argv[3], fixture, NULL, NULL, error,
               sizeof error) == 0);
    uint8_t base[256], previous[256], spend[256], first[32], txid[32];
    size_t length = read_file(fixture, base);
    for (unsigned version = 1; version <= 4; ++version) {
        size_t previous_length = previous_version(base, length,
                                                  version, previous);
        uint64_t amount = 400000000;
        size_t amount_offset = version >= 3 ? 51 : 47;
        for (unsigned i = 0; i < 8; ++i)
            previous[amount_offset + i] = (uint8_t)(amount >> (i * 8));
        assert(SHA256(previous, previous_length, first));
        assert(SHA256(first, sizeof first, txid));
        memcpy(spend, base, length);
        memcpy(spend + 9, txid, sizeof txid);
        write_file(spend_path, spend, length);
        write_file(previous_path, previous, previous_length);
        if (version == 4) {
            assert(run(argv[2], argv[3], NULL, spend_path, NULL, error,
                       sizeof error) == 2);
            assert(strstr(error, "PREVIOUS_TX.bin") != NULL);
            assert(run(argv[2], argv[3], NULL, spend_path, fixture, error,
                       sizeof error) == 1);
            assert(strstr(error, "outpoints do not match") != NULL);
        }
        assert(run(argv[2], argv[3], NULL, spend_path, previous_path, error,
                   sizeof error) == 1);
        assert(strstr(error, "not an accessible Ledger Blue") != NULL);
    }
    assert(unlink(fixture) == 0);
    assert(unlink(spend_path) == 0);
    assert(unlink(previous_path) == 0);
    assert(rmdir(directory) == 0);
    return 0;
}
