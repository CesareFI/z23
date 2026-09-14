/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CONTEXT_REFERENCE_H
#define ZCL_CONTEXT_REFERENCE_H
#include "zcl_wallet.h"
/* Independent traversal of all epochs projected from pinned original objects;
 * no wallet lookup, network state or original C++ execution is involved. */
zcl_status zcl_test_context_branch(zcl_network network, uint32_t height, uint32_t *branch);
#endif
