/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef BLUE_CHAIN_TIP_H
#define BLUE_CHAIN_TIP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool blue_chain_tip_parse(const char *reply, size_t length,
                          uint32_t *next_height);
bool blue_chain_tip_query(const char *rpc_binary, uint32_t *next_height);

#endif
