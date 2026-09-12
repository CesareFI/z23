/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "electrum_genesis_fixture.h"
#include <stdio.h>
#include <string.h>

static int write_frame(const char *name, const char *text, size_t length)
{
    FILE *file = fopen(name, "wb");
    if (file == NULL) return 1;
    const int failed = fwrite(text, 1, length, file) != length;
    const int closed = fclose(file);
    return failed || closed != 0;
}

static int genesis(const char *name, const char *hex)
{
    char frame[4096];
    int n = snprintf(frame, sizeof(frame), "{\"id\":1,\"result\":{\"hex\":\"%s\",\"height\":0,\"count\":1,\"max\":2016}}\n", hex);
    if (n < 0 || (size_t)n >= sizeof(frame)) return 1;
    return write_frame(name, frame, (size_t)n);
}

static int maximum_frame(void)
{
    static char frame[16385];
    static const char prefix[] = "{\"id\":1,\"result\":{\"confirmed\":0,\"unconfirmed\":0},\"extension\":\"";
    memset(frame, 'a', sizeof(frame));
    memcpy(frame, prefix, sizeof(prefix) - 1);
    memcpy(frame + sizeof(frame) - 3, "\"}\n", 3);
    return write_frame("maximum-frame", frame, sizeof(frame));
}

int main(void)
{
    /* Run in an explicitly created corpus directory; every byte is public. */
    static const char balance[] = "{\"id\":1,\"result\":{\"confirmed\":100000000,\"unconfirmed\":-10}}\n";
    static const char version[] = "{\"id\":1,\"result\":[\"ElectrumX fixture\",\"1.2\"]}\n";
    static const char features[] = "{\"id\":1,\"result\":{\"genesis_hash\":\"0007104ccda289427919efc39dc9e4d499804b7bebc22df55f8b834301260602\",\"hash_function\":\"sha256\"}}\n";
    if (write_frame("balance", balance, sizeof(balance) - 1) != 0) return 1;
    if (write_frame("version", version, sizeof(version) - 1) != 0) return 1;
    if (write_frame("features", features, sizeof(features) - 1) != 0) return 1;
    if (genesis("main-genesis", main_genesis) != 0) return 1;
    if (maximum_frame() != 0) return 1;
    return genesis("test-genesis", test_genesis);
}
