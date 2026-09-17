/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_MERKLE_INTERNAL_H
#define ZCL_TRANSACTION_MERKLE_INTERNAL_H
#include "zcl_transaction.h"

#define ZCL_MERKLE_BRANCH_MAX ((size_t)32)
typedef struct {
    uint32_t transaction_count; /* Claimed tree width, NOT authenticated by a header. */
    uint32_t transaction_index;
    size_t sibling_count;
    uint8_t siblings[ZCL_MERKLE_BRANCH_MAX][32]; /* Display order, leaf level first. */
} zcl_merkle_branch;

/* Internal public-data consistency check. All IDs/root/siblings use displayed
 * byte order, as wallet transaction IDs do. Hashing uses original raw uint256
 * order, left/right index bits and odd-width final-node duplication. Require
 * exact depth for the claimed width; reject equal real siblings on this path,
 * as the original partial-Merkle extractor does. At most32 pairs, no heap/I/O.
 *
 * OK establishes ONLY this hash path under the supplied width/index/root.
 * It does not authenticate count, root, header, transaction bytes, off-path
 * uniqueness, consensus, inclusion in an accepted chain, freshness, maturity
 * or unspentness, and grants no signing authority. In particular a header root
 * alone does not authenticate tree width or distinguish leaves/internal nodes.
 * No block-size/height predicate is introduced here. Caller owns stable spans;
 * inputs remain unchanged and no pointer/state survives the synchronous call. */
zcl_status zcl_merkle_branch_check(const uint8_t transaction_id[32],
    const zcl_merkle_branch *branch, const uint8_t expected_root[32]);
#endif
