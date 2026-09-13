/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "assessment_fixture.h"
#include <string.h>

static bool destination(zcl_tx_output *output, zcl_address_kind kind, uint8_t hash, uint64_t value)
{
    zcl_address address = {ZCL_MAINNET, kind, {0}};
    memset(address.hash, hash, sizeof(address.hash));
    output->value = value;
    return zcl_address_script(&address, output->script, sizeof(output->script), &output->script_len) == ZCL_OK;
}

bool assessment_fixture_rebind(assessment_fixture *fixture, size_t index)
{
    if (fixture == NULL || index >= 2) return false;
    size_t length = 0;
    if (zcl_transaction_serialize(&fixture->previous[index], fixture->wire[index],
        sizeof(fixture->wire[index]), &length) != ZCL_OK) return false;
    if (zcl_transaction_id(&fixture->previous[index], fixture->spending.inputs[index].previous_txid, 32) != ZCL_OK)
        return false;
    fixture->sources[index].wire = fixture->wire[index];
    fixture->sources[index].length = length;
    return true;
}

bool assessment_fixture_init(assessment_fixture *fixture)
{
    if (fixture == NULL) return false;
    memset(fixture, 0, sizeof(*fixture));
    fixture->spending.input_count = fixture->spending.output_count = 2;
    for (size_t i = 0; i < 2; ++i) {
        zcl_transparent_tx *previous = &fixture->previous[i];
        previous->input_count = 1;
        previous->output_count = 2;
        previous->inputs[0].previous_index = (uint32_t)(i + 1);
        previous->inputs[0].sequence = UINT32_MAX;
        if (!destination(&previous->outputs[0], ZCL_P2PKH, (uint8_t)(0x11 + 0x22 * i),
            i == 0 ? 10000 : 20000)) return false;
        if (!destination(&previous->outputs[1], ZCL_P2SH, (uint8_t)(0x22 + 0x22 * i),
            i == 0 ? 5000 : 1000)) return false;
        fixture->spending.inputs[i].previous_index = (uint32_t)i;
        fixture->spending.inputs[i].sequence = UINT32_MAX;
        if (!assessment_fixture_rebind(fixture, i)) return false;
    }
    if (!destination(&fixture->spending.outputs[0], ZCL_P2PKH, 0x55, 9000)) return false;
    return destination(&fixture->spending.outputs[1], ZCL_P2SH, 0x66, 1500);
}
