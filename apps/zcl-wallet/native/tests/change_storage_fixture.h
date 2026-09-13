/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_STORAGE_FIXTURE_H
#define ZCL_CHANGE_STORAGE_FIXTURE_H
#include "storage_fixture.h"
#include "zcl_change_storage.h"
#include <sys/types.h>

/* Published zero entropy; inert ciphertext, no GCM/hardware custody claim. */
typedef struct {
    uint8_t wallet[140];
    size_t wallet_len;
    uint8_t state[3][80];
} change_storage_data;
int change_data_init(change_storage_data *data);
zcl_status change_create(const storage_fixture *fixture, const change_storage_data *data);
zcl_status change_observe(const storage_fixture *fixture, const change_storage_data *data,
    zcl_change_storage_snapshot *snapshot);
zcl_status change_append(const storage_fixture *fixture, const change_storage_data *data,
    const zcl_change_storage_snapshot *snapshot, size_t next);
int change_bytes(const storage_fixture *fixture, const uint8_t *expected, size_t length, off_t offset);
#endif
