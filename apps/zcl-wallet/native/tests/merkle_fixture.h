/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MERKLE_FIXTURE_H
#define ZCL_MERKLE_FIXTURE_H
#include "transaction_merkle_internal.h"
typedef struct {
    zcl_merkle_branch branch;
    uint8_t leaf[32], root[32];
} merkle_fixture;
/* Independent full-tree construction for small public fixtures. */
bool merkle_fixture_tree(uint32_t count, uint32_t index, merkle_fixture *fixture);
/* Opaque synthetic subtrees for uint32 boundary paths; not actual blocks. */
bool merkle_fixture_path(uint32_t count, uint32_t index, merkle_fixture *fixture);
/* Independent width/branch oracle; OpenSSL in the ZCL_ORACLE host profile. */
bool merkle_fixture_check(const merkle_fixture *fixture);
#endif
