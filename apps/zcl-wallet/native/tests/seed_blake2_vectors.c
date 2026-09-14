/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Public oracle vectors only; independent of the reviewed reference source. */
#include <sodium.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(v) do { if (!(v)) { fputs("Public oracle generation failed\n", stderr); abort(); } } while (0)
static uint8_t input[4096];

static void bytes(const uint8_t *value, size_t length)
{
    CHECK(length <= 32);
    CHECK(printf("{") >= 0);
    for (size_t i = 0; i < length; ++i)
        CHECK(printf("%s0x%02x", i == 0 ? "" : ",", (unsigned)value[i]) >= 0);
    CHECK(printf("}") >= 0);
}

int main(void)
{
    const size_t lengths[] = {0, 1, 31, 32, 63, 64, 127, 128, 129, 255, 256, 257, 1925, 4096};
    CHECK(sodium_init() >= 0);
    for (size_t i = 0; i < sizeof(input); ++i) input[i] = (uint8_t)((17 * i + 3) & 255);
    CHECK(printf("/* Public libsodium %s BLAKE2b-256 oracle; input[i]=(17*i+3)&255. */\n",
                 sodium_version_string()) >= 0);
    CHECK(printf("static const struct { size_t length; uint8_t personal[16], digest[32]; } vectors[] = {\n") >= 0);
    for (unsigned profile = 0; profile < 4; ++profile) {
        uint8_t personal[16] = {0}, salt[16] = {0};
        for (size_t i = 0; i < sizeof(personal); ++i)
            personal[i] = (uint8_t)((profile * (i + 1) * 17) & 255);
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            uint8_t digest[32] = {0};
            CHECK(crypto_generichash_blake2b_salt_personal(digest, sizeof(digest), input,
                (unsigned long long)lengths[i], NULL, 0, salt, personal) == 0);
            CHECK(printf("    {%zu, ", lengths[i]) >= 0);
            bytes(personal, sizeof(personal));
            CHECK(printf(", ") >= 0);
            bytes(digest, sizeof(digest));
            CHECK(printf("},\n") >= 0);
        }
    }
    CHECK(printf("};\n") >= 0 && fflush(stdout) == 0);
    return 0;
}
