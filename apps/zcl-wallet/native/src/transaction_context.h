/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_CONTEXT_H
#define ZCL_TRANSACTION_CONTEXT_H
#include "zcl_wallet.h"

/* Public scalar lookup for the pinned Zclassic v4 profile, not chain evidence.
 * height is the candidate block's height (not the previous tip), <=INT32_MAX.
 * Pre-Sapling heights and networks other than main/test refuse. Historical
 * source identity and boundary projections are in TRANSACTIONS.md. Never infer
 * that a server's height, branch or claimed current chain is authenticated.
 * No allocation, pointer retention or output change on failure. */
zcl_status zcl_transaction_v4_branch(zcl_network network, uint32_t height, uint32_t *branch);
#endif
