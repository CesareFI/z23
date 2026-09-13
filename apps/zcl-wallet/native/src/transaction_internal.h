/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TRANSACTION_INTERNAL_H
#define ZCL_TRANSACTION_INTERNAL_H
#include "zcl_transaction.h"
#define ZCL_TX_HEADER UINT32_C(0x80000004)
#define ZCL_TX_VERSION_GROUP UINT32_C(0x892f2085)
/* Validates the entire bounded object before any public serialization write. */
zcl_status zcl_transaction_check(const zcl_transparent_tx *tx, size_t *wire_size);
#endif
