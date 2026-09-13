/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include <stdio.h>
static assessment_fixture fixture;
int main(void)
{
    if (!assessment_fixture_init(&fixture)) return 1;
    /* Caller creates a new corpus directory before running this fixture. */
    const char *names[2] = {"previous0", "previous1"};
    for (size_t i = 0; i < 2; ++i) {
        FILE *file = fopen(names[i], "wb");
        if (file == NULL) return 1;
        const bool written = fwrite(fixture.wire[i], 1, fixture.sources[i].length, file) == fixture.sources[i].length;
        const int closed = fclose(file);
        if (!written || closed != 0) return 1;
    }
    return 0;
}
