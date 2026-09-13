/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef SYNC_FIXTURE_H
#define SYNC_FIXTURE_H
#include "zcl_sync.h"
void sync_fixture_start(zcl_sync *session, zcl_network network, uint32_t first_id);
void sync_fixture_start_with_history(zcl_sync *session, zcl_network network, uint32_t first_id);
size_t sync_fixture_reply(zcl_network network, unsigned step, uint32_t id,
                           char *frame, size_t capacity);
#endif
