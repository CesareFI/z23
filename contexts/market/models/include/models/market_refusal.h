/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_MODELS_MARKET_REFUSAL_H
#define ZCL_MODELS_MARKET_REFUSAL_H

#include "models/database.h"
#include "models/activerecord.h"

struct market_refusal { uint8_t root[32]; };
struct ar_callbacks *db_market_refusal_callbacks(void);
bool db_market_refusal_validate(const struct market_refusal *record,
                                struct ar_errors *errors);
/* Durable local refusal in node_state, independent of removable offer rows.
 * Stores only the refused root, never advertised content. */
bool db_market_refusal_save(struct node_db *ndb,
                            const struct market_refusal *record);
/* True only when a successful query proves no refusal exists. */
bool db_market_refusal_allows(struct node_db *ndb, const uint8_t root[32]);

#endif
