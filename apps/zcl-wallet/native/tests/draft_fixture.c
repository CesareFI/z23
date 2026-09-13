/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "draft_fixture.h"
#include <string.h>

bool draft_fixture_init(zcl_draft_request *request, assessment_fixture *fixture, zcl_network network)
{
    if (request == NULL || fixture == NULL) return false;
    if (!assessment_fixture_init(fixture)) return false;
    memset(request, 0, sizeof(*request));
    request->network = network;
    request->maximum_fee = 500;
    request->input_count = request->output_count = 2;
    for (size_t i = 0; i < 2; ++i) {
        request->inputs[i].previous = fixture->sources[i];
        request->inputs[i].output_index = fixture->spending.inputs[i].previous_index;
        request->inputs[i].sequence = fixture->spending.inputs[i].sequence;
        request->outputs[i].value = fixture->spending.outputs[i].value;
        if (zcl_address_from_script(fixture->spending.outputs[i].script,
            fixture->spending.outputs[i].script_len, network, &request->outputs[i].destination) != ZCL_OK)
            return false;
    }
    return true;
}
