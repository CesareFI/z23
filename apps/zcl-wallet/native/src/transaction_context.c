/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_context.h"

zcl_status zcl_transaction_v4_branch(zcl_network network, uint32_t height, uint32_t *branch)
{
    if (branch == NULL) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    if (height > INT32_MAX) return ZCL_OUT_OF_RANGE;
    /* Pinned original chainparams/upgrades, not modern Zcash constants.
     * Overwinter and Sapling activate together. Buttercup retains Bubbly's
     * branch ID; on testnet it is the first activation of that branch. */
    static const struct { uint32_t sapling, bubbles, bubbly; } heights[2] = {
        {476969, 585318, 585322}, {20, 6350, 78856}
    };
    if (height < heights[network].sapling) return ZCL_UNSUPPORTED;
    uint32_t candidate = UINT32_C(0x76b809bb);
    if (height >= heights[network].bubbles) candidate = UINT32_C(0x821a451c);
    if (height >= heights[network].bubbly) candidate = UINT32_C(0x930b540d);
    *branch = candidate;
    return ZCL_OK;
}
