/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "change_custody_internal.h"
#include "storage_change_internal.h"
#include <string.h>

static zcl_status authenticated_head(const zcl_change_storage_snapshot *snapshot, uint32_t index)
{
    /* Caller authenticated an exact80-byte tail, hence file_bytes>=80. */
    if (snapshot->file_bytes % 80 != 0) return ZCL_INVALID_ENCODING;
    if (index != snapshot->file_bytes / 80 - 1) return ZCL_INVALID_ENCODING;
    return ZCL_ALREADY_EXISTS; /* Healthy, no repair and no durability change. */
}

/* OK only permits further damage qualification, never immediate repair. */
static zcl_status current_damage(const zcl_change_custody *wallet,
    const zcl_change_storage_snapshot *snapshot)
{
    if (snapshot->tail_len != 80) return ZCL_INVALID_ENCODING;
    uint32_t index = 0;
    zcl_status status = zcl_change_custody_decode(wallet, snapshot->tail, snapshot->tail_len, &index);
    if (status == ZCL_OK) return authenticated_head(snapshot, index);
    if (status == ZCL_INVALID_ENCODING || status == ZCL_UNSUPPORTED || status == ZCL_OUT_OF_RANGE)
        return ZCL_OK;
    return status; /* RNG/provider/internal failure is not damage permission. */
}

static zcl_status repair_shape(const zcl_change_recovery_snapshot *snapshot,
    zcl_change_repair_plan *plan, size_t *fragment_len)
{
    if (!snapshot->has_predecessor || snapshot->current.file_bytes <= 80) return ZCL_INVALID_ENCODING;
    size_t fragment = (size_t)(snapshot->current.file_bytes % 80);
    if (fragment == 0) fragment = 80;
    if (fragment < 16) return ZCL_UNSUPPORTED;
    zcl_status status = zcl_store_change_plan_repair(&snapshot->current, plan);
    if (status == ZCL_OK) *fragment_len = fragment;
    return status;
}

static zcl_status predecessor(const zcl_change_custody *wallet,
    const zcl_change_recovery_snapshot *snapshot, const zcl_change_repair_plan *plan)
{
    uint32_t index = 0;
    zcl_status status = zcl_change_custody_decode(wallet, snapshot->predecessor, 80, &index);
    /* Shape proved size>80, so the next-position plan is>=2. */
    if (status == ZCL_OK && index != plan->next_index - 2) status = ZCL_INVALID_ENCODING;
    return status;
}

static zcl_status supported_prefix(const zcl_change_custody *wallet,
    const zcl_change_recovery_snapshot *snapshot, const zcl_change_repair_plan *plan, size_t fragment_len)
{
    uint8_t expected[80] = {0};
    zcl_status status = zcl_change_custody_encode(wallet, plan->next_index - 1, expected);
    if (status != ZCL_OK) return status;
    /* Compare PUBLIC format/counter bytes only, never a MAC through memcmp.
     * Shape proved current tail80 and fragment16..80 before this offset. */
    const uint8_t *fragment = snapshot->current.tail + (80 - fragment_len);
    return memcmp(fragment, expected, 16) == 0 ? ZCL_OK : ZCL_UNSUPPORTED;
}

static zcl_status recovery_record(const zcl_change_custody *wallet,
    const zcl_change_recovery_snapshot *snapshot, uint8_t *replacement)
{
    zcl_change_repair_plan plan = {0};
    size_t fragment_len = 0;
    zcl_status status = current_damage(wallet, &snapshot->current);
    if (status == ZCL_OK) status = repair_shape(snapshot, &plan, &fragment_len);
    if (status == ZCL_OK) status = predecessor(wallet, snapshot, &plan);
    if (status == ZCL_OK) status = supported_prefix(wallet, snapshot, &plan, fragment_len);
    if (status == ZCL_OK) status = zcl_change_custody_encode(wallet, plan.next_index, replacement);
    return status;
}

zcl_status zcl_wallet_change_recover(const uint8_t *directory, size_t directory_len,
    const uint8_t *wallet_record, size_t wallet_len, const uint8_t *entropy, size_t entropy_len)
{
    zcl_change_custody wallet = {0};
    zcl_change_recovery_snapshot snapshot = {0};
    uint8_t replacement[80] = {0};
    zcl_status status = zcl_change_custody_prepare(wallet_record, wallet_len, entropy, entropy_len, &wallet);
    if (status == ZCL_OK)
        status = zcl_storage_change_probe(directory, directory_len, wallet.record, wallet.record_len, &snapshot);
    if (status == ZCL_OK) status = recovery_record(&wallet, &snapshot, replacement);
    if (status == ZCL_OK)
        status = zcl_storage_change_repair(directory, directory_len, wallet.record, wallet.record_len,
            &snapshot.current, replacement, sizeof(replacement));
    zcl_change_custody_clear(&wallet);
    return status;
}
