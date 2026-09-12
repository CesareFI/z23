/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* White-box regressions for the precisely pinned local provider fixes. */
#include "identify.c"
#include "decode.c"
#include <stdio.h>
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Provider check failed at %d\n", __LINE__); abort(); } } while (0)

static void projection(void)
{
    quirc_float_t c[8] = {0};
    struct quirc_point p = {7, 9};
    c[2] = 1; c[6] = -1;
    CHECK(perspective_map(c, 1, 0, &p) == 0);
    CHECK(p.x == 7 && p.y == 9);
    c[6] = 0; c[2] = 1e100;
    CHECK(perspective_map(c, 1, 0, &p) == 0);
    c[2] = 0; c[0] = 1; c[4] = 1;
    CHECK(perspective_map(c, 25, 36, &p) == 1);
    CHECK(p.x == 25 && p.y == 36);
}

static void allocation_and_indices(void)
{
    struct quirc *q = quirc_new();
    CHECK(q != NULL);
    CHECK(quirc_resize(q, 64, 64) == 0);
    CHECK(quirc_resize(q, 80, 40) == 0);
    CHECK(quirc_resize(q, 21, 21) == 0);
    CHECK(quirc_resize(q, 0, 21) == -1);
    CHECK(quirc_resize(q, -1, 21) == -1);
    CHECK(quirc_resize(q, INT_MAX, INT_MAX) == -1);
    CHECK(q->w == 21 && q->h == 21);
    struct quirc_code *code = calloc(1, sizeof(*code));
    CHECK(code != NULL);
    q->num_grids = QUIRC_MAX_GRIDS;
    quirc_extract(q, QUIRC_MAX_GRIDS, code);
    CHECK(code->size == 0);
    quirc_extract(q, INT_MIN, code);
    CHECK(code->size == 0);
    free(code);
    quirc_destroy(q);
}

static void work_limits(void)
{
    struct quirc *q = quirc_new();
    CHECK(q != NULL && quirc_resize(q, 64, 64) == 0);
    CHECK(quirc_begin(q, NULL, NULL) != NULL);
    q->grid_budget = 0;
    record_qr_grid(q, 0, 1, 2);
    CHECK(quirc_is_limited(q));
    CHECK(quirc_begin(q, NULL, NULL) != NULL);
    CHECK(!quirc_is_limited(q));
    q->capstones[0].c[0] = 1;
    q->capstones[0].c[4] = 1;
    q->capstones[2].c[0] = 1;
    q->capstones[2].c[4] = 1;
    q->grids[0].caps[0] = 0;
    q->grids[0].caps[2] = 2;
    q->grids[0].align = (struct quirc_point){32, 32};
    q->alignment_budget = 0;
    find_alignment_pattern(q, 0);
    CHECK(quirc_is_limited(q));
    CHECK(quirc_begin(q, NULL, NULL) != NULL);
    q->fitness_budget = 0;
    CHECK(fitness_cell(q, 0, 0, 0) == 0);
    CHECK(quirc_is_limited(q));
    quirc_destroy(q);
}

static void modes(void)
{
    struct quirc_data *data = calloc(1, sizeof(*data));
    struct datastream *stream = calloc(1, sizeof(*stream));
    CHECK(data != NULL && stream != NULL);
    stream->data[0] = 0x30; /* Unsupported structured-append mode. */
    stream->data_bits = 4;
    CHECK(decode_payload(data, stream) == QUIRC_ERROR_UNKNOWN_DATA_TYPE);
    memset(stream, 0, sizeof(*stream));
    stream->data_bits = 4;
    CHECK(decode_payload(data, stream) == QUIRC_SUCCESS);
    free(stream);
    free(data);
}

int main(void)
{
    projection();
    allocation_and_indices();
    work_limits();
    modes();
    puts("Provider allocation, geometry, index, work-budget and mode regressions passed");
    return 0;
}
