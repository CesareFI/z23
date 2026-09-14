/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "context_reference.h"
#include "chain_context_reference.h"
#include <stdlib.h>

zcl_status zcl_test_context_branch(zcl_network network, uint32_t height, uint32_t *branch)
{
    if (branch == NULL) abort();
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    if (height > INT32_MAX) return ZCL_OUT_OF_RANGE;
    size_t active = 0;
    for (size_t epoch = 0; epoch < sizeof(reference_epochs) / sizeof(reference_epochs[0]); ++epoch) {
        const int32_t activation = reference_epochs[epoch].activation[network];
        if (activation >= 0 && (int64_t)height >= activation) active = epoch;
    }
    if (active < ZCL_REFERENCE_SAPLING_EPOCH) return ZCL_UNSUPPORTED;
    *branch = reference_epochs[active].branch;
    return ZCL_OK;
}
