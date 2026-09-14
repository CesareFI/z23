/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_context.h"
#include "context_reference.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Branch context check at %d\n", __LINE__); abort(); } } while (0)

static void compare(zcl_network network, uint32_t height)
{
    struct { uint32_t before, branch, after; } box = {17, UINT32_MAX, 29};
    uint32_t expected = UINT32_MAX;
    const zcl_status status = zcl_test_context_branch(network, height, &expected);
    CHECK(zcl_transaction_v4_branch(network, height, &box.branch) == status);
    CHECK(box.before == 17 && box.after == 29 && box.branch == expected);
}

int main(void)
{
    for (size_t network = 0; network < 2; ++network) {
        for (uint32_t height = 0; height <= 800000; ++height) compare((zcl_network)network, height);
        compare((zcl_network)network, INT32_MAX - 1);
        compare((zcl_network)network, INT32_MAX);
        compare((zcl_network)network, UINT32_C(2147483648));
        compare((zcl_network)network, UINT32_MAX);
        CHECK(zcl_transaction_v4_branch((zcl_network)network, 1000000, NULL) == ZCL_INVALID_ARGUMENT);
    }
    compare((zcl_network)-1, 1000000);
    compare((zcl_network)2, 1000000);
    CHECK(puts("Both v4 branch schedules match projected original epochs at 1,600,002 consecutive heights and integer edges") >= 0);
    return 0;
}
