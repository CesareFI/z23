/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_TEST_MARKETPLACE_FIXTURE_H
#define ZCL_TEST_MARKETPLACE_FIXTURE_H
#include "net/marketplace.h"
#include "util/util.h"

/* Cache/codec unit tests opt in explicitly and supply permissive owner ports.
 * These fixtures do not claim durable operator-policy coverage; the registered
 * marketplace_policy group binds the real model/service and tests its refusal. */
static inline bool marketplace_fixture_root(const uint8_t root[32])
{
    return root != NULL;
}
static inline bool marketplace_fixture_wire(const char *command,
                                             const uint8_t *wire, size_t size)
{
    (void)size;
    return command && wire;
}
static inline void test_marketplace_opt_in(void)
{
    const char *args[] = {"test", "-marketplace=1"};
    ParseParameters(2, args);
    marketplace_set_filters(marketplace_fixture_root, marketplace_fixture_wire);
}
#endif
