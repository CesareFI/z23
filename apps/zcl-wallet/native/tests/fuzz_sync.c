/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "sync_fixture.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 || size > ZCL_ELECTRUM_FRAME_MAX + 1) return 0;
    const bool history = (data[0] & 64) != 0;
    const unsigned steps = history ? 7 : 6;
    const unsigned stop = (unsigned)data[0] % steps + 1;
    const zcl_network network = (data[0] & 8) == 0 ? ZCL_MAINNET : ZCL_TESTNET;
    const uint32_t first_id = (data[0] & 16) == 0 ? 1 : UINT32_MAX - (history ? 6u : 5u);
    zcl_sync session;
    if (history) sync_fixture_start_with_history(&session, network, first_id);
    else sync_fixture_start(&session, network, first_id);
    /* Host-only single-threaded fixture scratch, never runtime global state. */
    static char fixture[4096];
    uint8_t request[ZCL_ELECTRUM_REQUEST_MAX];
    for (unsigned step = 1; step <= stop; ++step) {
        size_t written = 0;
        if (zcl_sync_request(&session, request, sizeof(request), &written) != ZCL_OK) abort();
        if (written == 0 || written > sizeof(request)) abort();
        if (step == stop) break;
        const size_t count = sync_fixture_reply(network, (unsigned)session.phase, session.request_id,
            fixture, sizeof(fixture));
        if (zcl_sync_reply(&session, (const uint8_t *)fixture, count) != ZCL_OK) abort();
    }
    const zcl_status status = zcl_sync_reply(&session, data + 1, size - 1);
    zcl_sync_report report, before;
    memset(&report, 0xa5, sizeof(report));
    memcpy(&before, &report, sizeof(before));
    const zcl_status published = zcl_sync_get_report(&session, &report);
    if (published == ZCL_OK) {
        if (stop != steps || status != ZCL_OK || session.phase != ZCL_SYNC_DONE) abort();
        if (report.balance.total != 993 || report.tip.height != 0) abort();
        if (report.has_history != history || (history && report.history.count != 2)) abort();
    } else if (memcmp(&report, &before, sizeof(report)) != 0) abort();
    if (status != ZCL_OK) {
        if (session.phase != ZCL_SYNC_FAILED || session.waiting) abort();
        if (session.candidate.balance.confirmed != 0 || session.candidate.balance.total != 0) abort();
        if (session.candidate.has_history || session.candidate.history.count != 0) abort();
        if (zcl_sync_reply(&session, data + 1, size - 1) != status) abort();
    }
    if (zcl_sync_abort(&session, ZCL_CANCELLED) != ZCL_CANCELLED) abort();
    if (zcl_sync_get_report(&session, &report) != ZCL_CANCELLED) abort();
    return 0;
}
