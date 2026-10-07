/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MARKET_PARTICIPATION_SERVICE_H
#define ZCL_MARKET_PARTICIPATION_SERVICE_H

#include "base/result.h"
#include <stdbool.h>
#include <stdint.h>
struct node_db;

void market_participation_set_context(struct node_db *ndb);
bool market_participation_offer_allowed(const uint8_t offer_id[32]);
/* Record the refusal before removing listing projections. A failed purge
 * leaves the refusal effective. No content file or blockchain data is deleted. */
struct zcl_result market_participation_refuse(const uint8_t root[32]);

#endif
