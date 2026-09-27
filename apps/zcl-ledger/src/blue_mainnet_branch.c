/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_mainnet_branch.h"

/* Mainnet heights: core/chainparams/src/chainparams.c.
 * Branch IDs: core/params/src/upgrades.c. */
bool blue_mainnet_branch_for_height(uint32_t height, uint32_t *branch) {
    if (!branch || height < 476969) return false;
    if (height < 585318) *branch = 0x76b809bb;
    else if (height < 585322) *branch = 0x821a451c;
    else *branch = 0x930b540d;
    return true;
}
