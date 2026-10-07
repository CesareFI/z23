/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "electrum_genesis_fixture.h"
#include <stdio.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int exercise(const uint8_t *data, size_t size, const char *directory, size_t number)
{
    if (LLVMFuzzerTestOneInput(data, size) != 0) return 0;
    if (directory == NULL) return 1;
    char path[1024];
    const int length = snprintf(path, sizeof(path), "%s/framing-%zu", directory, number);
    if (length < 0 || (size_t)length >= sizeof(path)) return 0;
    FILE *file = fopen(path, "wbx");
    if (file == NULL) return 0;
    int okay = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) okay = 0;
    return okay;
}

static int cases(const char *directory)
{
    /* Fixed host scratch: no large stack frame, allocation or retained input. */
    static uint8_t bytes[ZCL_ELECTRUM_FRAME_MAX + 1];
    static const size_t small[] = {0, 1, 2, 255, 256, 257};
    size_t number = 0;
    for (unsigned byte = 0; byte <= 255; ++byte) {
        memset(bytes, (int)byte, sizeof(bytes));
        for (size_t i = 0; i < sizeof(small) / sizeof(small[0]); ++i)
            if (!exercise(bytes, small[i], directory, number++)) return 0;
    }
    static const uint8_t chunks[] = {0, 1, 9, 10, 31, 127, 255};
    for (size_t chunk = 0; chunk < sizeof(chunks); ++chunk) {
        for (size_t length = ZCL_ELECTRUM_FRAME_MAX - 1; length <= sizeof(bytes); ++length) {
            for (unsigned newline = 0; newline < 3; ++newline) {
                memset(bytes, 'A', sizeof(bytes));
                bytes[0] = chunks[chunk];
                if (newline == 1) bytes[length - 1] = '\n';
                if (newline == 2) bytes[length / 2] = '\n';
                if (!exercise(bytes, length, directory, number++)) return 0;
            }
        }
    }
    static const uint8_t mixed[] = "\n{}\r\n{\"id\":1,\"result\":[\"fixture\",\"1.2\"]}\n\0tail\nremaining";
    if (!exercise(mixed, sizeof(mixed) - 1, directory, number++)) return 0;
    return number == 1600;
}

static int identity_cases(const char *directory, zcl_network network, size_t number)
{
    /* Same pinned public identities as the deterministic protocol unit tests.
     * Positive controls ensure an always-refusing parser cannot pass. */
    static const char main_hash[] = "0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602";
    static const char test_hash[] = "03e1c4bb705c871bf9bfda3e74b7f8f86bff267993c215a89d5795e3708e5e1f";
    static char frame[4096];
    int length = snprintf(frame, sizeof(frame),
        "{\"id\":1,\"result\":{\"genesis_hash\":\"%s\",\"hash_function\":\"sha256\"}}\n",
        network == ZCL_MAINNET ? main_hash : test_hash);
    if (length <= 0 || (size_t)length >= sizeof(frame)) return 0;
    if (zcl_electrum_features_reply((const uint8_t *)frame, (size_t)length, 1, network) != ZCL_OK) return 0;
    if (!exercise((const uint8_t *)frame, (size_t)length, directory, number)) return 0;
    length = snprintf(frame, sizeof(frame), "{\"id\":1,\"result\":{\"count\":1,\"hex\":\"%s\"}}\n",
        network == ZCL_MAINNET ? main_genesis : test_genesis);
    if (length <= 0 || (size_t)length >= sizeof(frame)) return 0;
    if (zcl_electrum_genesis_reply((const uint8_t *)frame, (size_t)length, 1, network) != ZCL_OK) return 0;
    return exercise((const uint8_t *)frame, (size_t)length, directory, number + 1);
}

int main(int argc, char **argv)
{
    const char *directory = argc == 2 ? argv[1] : NULL;
    if (argc > 2 || !cases(directory) || !identity_cases(directory, ZCL_MAINNET, 1600) ||
        !identity_cases(directory, ZCL_TESTNET, 1602)) {
        fprintf(stderr, "Electrum framing corpus replay/generation failed\n");
        return 1;
    }
    return puts("Electrum: 1600 framing cases plus four live main/test identity controls passed") < 0 ? 1 : 0;
}
