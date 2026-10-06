/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_review_internal.h"
#include "change_custody_internal.h"
#include "bip32_internal.h"
#include <string.h>

typedef struct {
    zcl_review_wallet_input claim;
    zcl_change_custody wallet;
    uint8_t directory[ZCL_STORAGE_PATH_MAX];
    uint8_t entropy[ZCL_ENTROPY_MAX];
    uint8_t expected[35];
    uint8_t derived[35];
} review_wallet_work;

static zcl_status claim_bounds(const zcl_review_wallet_input *claim)
{
    if (claim->directory_len == 0 || claim->directory_len > ZCL_STORAGE_PATH_MAX) return ZCL_OUT_OF_RANGE;
    if (claim->record_len < 124 || claim->record_len > ZCL_WALLET_RECORD_MAX) return ZCL_OUT_OF_RANGE;
    if (claim->entropy_len < 16 || claim->entropy_len > ZCL_ENTROPY_MAX || claim->entropy_len % 4 != 0)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status claim_profile(const zcl_review_wallet_input *claim)
{
    if (claim->directory == NULL || claim->record == NULL || claim->entropy == NULL) return ZCL_INVALID_ARGUMENT;
    if (claim->chain > 1) return ZCL_UNSUPPORTED;
    if (claim->chain == 0 && claim->index != 0) return ZCL_UNSUPPORTED;
    if (claim->index >= ZCL_CHANGE_STORAGE_MAX_RECORDS - 1) return ZCL_OUT_OF_RANGE;
    return claim_bounds(claim);
}

static zcl_status admit_claim_record(const zcl_review_wallet_input *claim,
    const zcl_address *destination)
{
    zcl_wallet_record parsed = {0};
    zcl_status status = zcl_wallet_record_parse(claim->record, claim->record_len, &parsed);
    if (status == ZCL_OK && claim->entropy_len != parsed.info.entropy_len)
        status = ZCL_OUT_OF_RANGE;
    if (status == ZCL_OK && parsed.info.network != destination->network)
        status = ZCL_UNSUPPORTED;
    zcl_secure_zero(&parsed, sizeof(parsed));
    return status;
}

static zcl_status prepare_claim(review_wallet_work *work, const zcl_address *destination)
{
    zcl_review_wallet_input *claim = &work->claim;
    zcl_status status = claim_profile(claim);
    if (status == ZCL_OK) status = admit_claim_record(claim, destination);
    if (status != ZCL_OK) return status;
    memcpy(work->entropy, claim->entropy, claim->entropy_len);
    memcpy(work->directory, claim->directory, claim->directory_len);
    status = zcl_change_custody_prepare(claim->record, claim->record_len, work->entropy,
        claim->entropy_len, &work->wallet);
    if (status != ZCL_OK) return status;
    if (work->wallet.network != destination->network) return ZCL_UNSUPPORTED;
    claim->directory = work->directory;
    claim->record = work->wallet.record;
    claim->entropy = work->entropy;
    return ZCL_OK;
}

static zcl_status committed_wallet(const zcl_review_wallet_input *claim)
{
    uint8_t record[ZCL_WALLET_RECORD_MAX] = {0};
    size_t length = 0;
    bool pending = false;
    zcl_status status = zcl_storage_read(claim->directory, claim->directory_len,
        record, sizeof(record), &length, &pending);
    if (status == ZCL_OK && pending) status = ZCL_NOT_FOUND;
    if (status == ZCL_OK && (length != claim->record_len || memcmp(record, claim->record, length) != 0))
        status = ZCL_ALREADY_EXISTS;
    zcl_secure_zero(record, sizeof(record));
    return status;
}

static zcl_status receive_owner(const zcl_review_wallet_input *claim, uint8_t *address, size_t capacity)
{
    zcl_status status = committed_wallet(claim);
    if (status != ZCL_OK) return status;
    uint8_t blinding[32] = {0};
    status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_wallet_recovered_address(claim->record, ZCL_WALLET_HEADER_BYTES,
            claim->entropy, claim->entropy_len, blinding, sizeof(blinding), address, capacity);
    zcl_secure_zero(blinding, sizeof(blinding));
    return status;
}

static zcl_status claimed_address(const zcl_review_wallet_input *claim, uint8_t *address, size_t capacity)
{
    if (claim->chain == 0) return receive_owner(claim, address, capacity);
    return zcl_wallet_change_reserved_address(claim->directory, claim->directory_len,
        claim->record, claim->record_len, claim->entropy, claim->entropy_len, claim->index, address, capacity);
}

static zcl_status check_prepared_wallet(review_wallet_work *work, const zcl_address *destination)
{
    zcl_status status = prepare_claim(work, destination);
    size_t length = 0;
    if (status == ZCL_OK)
        status = zcl_address_encode(destination, work->expected, sizeof(work->expected), &length);
    if (status == ZCL_OK && length != sizeof(work->expected)) status = ZCL_INVALID_ENCODING;
    if (status == ZCL_OK) status = claimed_address(&work->claim, work->derived, sizeof(work->derived));
    if (status == ZCL_OK && memcmp(work->derived, work->expected, sizeof(work->expected)) != 0)
        status = ZCL_NOT_FOUND;
    return status;
}

static zcl_status check_wallet(const zcl_review_wallet_input *claim, const zcl_address *destination)
{
    review_wallet_work work;
    memset(&work, 0, sizeof(work));
    work.claim = *claim;
    const zcl_status status = check_prepared_wallet(&work, destination);
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

static zcl_status review_destination(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_address **destination)
{
    const zcl_status status = zcl_review_live(owner, id, now_ms);
    if (status != ZCL_OK) return status;
    if (input_index >= owner->data.assessment.input_count || input_index >= ZCL_TX_INPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    const zcl_address *input = &owner->data.assessment.inputs[input_index].destination;
    if (input->kind != ZCL_P2PKH) return ZCL_UNSUPPORTED;
    *destination = input;
    return ZCL_OK;
}

zcl_status zcl_review_input_wallet_check(zcl_review_owner *owner, uint64_t id, uint64_t now_ms,
    size_t input_index, const zcl_review_wallet_input *claim)
{
    if (owner == NULL || claim == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_address *destination = NULL;
    const zcl_status status = review_destination(owner, id, now_ms, input_index, &destination);
    return status == ZCL_OK ? check_wallet(claim, destination) : status;
}

typedef struct {
    review_wallet_work wallet;
    zcl_review_block block;
    zcl_extended_private key;
    uint8_t digest[32], hash[20];
    zcl_signature signature;
} wallet_sign_work;

static zcl_status derive_signing_key(wallet_sign_work *work)
{
    uint8_t seed[64] = {0}, blinding[32] = {0};
    zcl_ec_context context = {0};
    const zcl_review_wallet_input *claim = &work->wallet.claim;
    zcl_status status = zcl_entropy_seed(claim->entropy, claim->entropy_len, seed, sizeof(seed));
    if (status == ZCL_OK) status = zcl_random_bytes(blinding, sizeof(blinding));
    if (status == ZCL_OK) status = zcl_ec_begin(&context, blinding, sizeof(blinding));
    zcl_secure_zero(blinding, sizeof(blinding));
    if (status == ZCL_OK)
        status = zcl_seed_private(seed, sizeof(seed), work->wallet.wallet.network,
            claim->chain, claim->index, context.handle, &work->key);
    zcl_secure_zero(seed, sizeof(seed));
    zcl_ec_end(&context);
    return status;
}

static zcl_status signing_time(zcl_review_owner *owner, uint64_t id, const zcl_review_clock *clock)
{
    uint64_t now = UINT64_MAX;
    const zcl_status status = clock->read(clock->context, &now);
    return status == ZCL_OK ? zcl_review_live(owner, id, now) : status;
}

static zcl_status sign_and_verify(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, wallet_sign_work *work)
{
    zcl_status status = derive_signing_key(work);
    /* Only the derived scalar remains needed by the signer. Retire the owned
     * entropy copy before another clock/provider call, including refusal. */
    zcl_secure_zero(work->wallet.entropy, sizeof(work->wallet.entropy));
    zcl_secure_zero(work->key.chain_code, sizeof(work->key.chain_code));
    if (status == ZCL_OK) status = signing_time(owner, id, clock);
    if (status == ZCL_OK)
        status = zcl_signature_create(work->key.secret, sizeof(work->key.secret),
            work->digest, sizeof(work->digest), &work->signature);
    zcl_secure_zero(&work->key, sizeof(work->key));
    uint8_t script[ZCL_SIGNATURE_SCRIPT_MAX] = {0};
    size_t length = 0;
    if (status == ZCL_OK)
        status = zcl_signature_p2pkh(&work->signature, work->digest, sizeof(work->digest),
            work->hash, sizeof(work->hash), script, sizeof(script), &length);
    if (status == ZCL_OK && (length < 44 || length > sizeof(script))) status = ZCL_INVALID_ENCODING;
    zcl_secure_zero(script, sizeof(script));
    return status;
}

static zcl_status prepare_signing(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, size_t input_index, wallet_sign_work *work)
{
    uint64_t now = UINT64_MAX;
    zcl_status status = clock->read(clock->context, &now);
    const zcl_address *destination = NULL;
    if (status == ZCL_OK) status = review_destination(owner, id, now, input_index, &destination);
    if (status != ZCL_OK) return status;
    memcpy(work->hash, destination->hash, sizeof(work->hash));
    status = zcl_review_sighash_context(owner, id, now, input_index,
        &work->block, work->digest, sizeof(work->digest));
    if (status == ZCL_OK) status = check_prepared_wallet(&work->wallet, destination);
    return status;
}

static zcl_status wallet_signature(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, size_t input_index, const zcl_review_block *block,
    const zcl_review_wallet_input *claim, zcl_signature *output)
{
    wallet_sign_work work;
    memset(&work, 0, sizeof(work));
    work.wallet.claim = *claim;
    work.block = *block;
    zcl_status status = prepare_signing(owner, id, clock, input_index, &work);
    if (status == ZCL_OK) status = sign_and_verify(owner, id, clock, &work);
    if (status == ZCL_OK) status = signing_time(owner, id, clock);
    if (status == ZCL_OK) memcpy(output, &work.signature, sizeof(*output));
    zcl_secure_zero(&work, sizeof(work));
    return status;
}

zcl_status zcl_review_input_wallet_sign(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, size_t input_index, const zcl_review_block *block,
    const zcl_review_wallet_input *claim, zcl_signature *output)
{
    if (owner == NULL || clock == NULL || clock->read == NULL || block == NULL ||
        claim == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    return wallet_signature(owner, id, clock, input_index, block, claim, output);
}

static zcl_status reviewed_change(zcl_review_owner *owner, size_t output_index,
    const zcl_review_wallet_input *claim)
{
    if (claim->chain != 1) return ZCL_UNSUPPORTED;
    if (output_index >= owner->data.assessment.output_count || output_index >= ZCL_TX_OUTPUT_MAX)
        return ZCL_OUT_OF_RANGE;
    const zcl_address *destination = &owner->data.assessment.outputs[output_index].destination;
    if (destination->kind != ZCL_P2PKH) return ZCL_UNSUPPORTED;
    return check_wallet(claim, destination);
}

zcl_status zcl_review_output_change_check(zcl_review_owner *owner, uint64_t id,
    const zcl_review_clock *clock, size_t output_index, const zcl_review_wallet_input *claim)
{
    if (owner == NULL || clock == NULL || clock->read == NULL || claim == NULL)
        return ZCL_INVALID_ARGUMENT;
    zcl_status status = signing_time(owner, id, clock);
    if (status == ZCL_OK) status = reviewed_change(owner, output_index, claim);
    /* check_wallet has already retired every owned secret before this final
     * clock read. A slow derivation cannot return a stale change match. */
    if (status == ZCL_OK) status = signing_time(owner, id, clock);
    return status;
}
