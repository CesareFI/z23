/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_STORAGE_FIXTURE_H
#define ZCL_STORAGE_FIXTURE_H
#include "zcl_storage.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "storage check failed at %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

/* Every test fixture owns a fresh mkdtemp directory and its descriptor.
 * Cleanup only unlinks these fixed test names inside that descriptor. */
typedef struct { char path[29]; int directory; } storage_fixture;
int fixture_open(storage_fixture *fixture);
int fixture_close(storage_fixture *fixture);
size_t fixture_path_len(void);
int fixture_record(uint8_t *record, size_t capacity, size_t *length);
int fixture_write(const storage_fixture *fixture, const char *name,
                  const uint8_t *bytes, size_t length);
zcl_status fixture_read(const storage_fixture *fixture, uint8_t *record, size_t capacity,
                        size_t *length, bool *pending);
zcl_status fixture_create(const storage_fixture *fixture, const uint8_t *record, size_t length);
zcl_status fixture_promote(const storage_fixture *fixture, const uint8_t *record, size_t length);
#endif
