/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_CHANGE_CUSTODY_RETIREMENT_H
#define ZCL_CHANGE_CUSTODY_RETIREMENT_H
#include <stdbool.h>
#include <stddef.h>

/* Source-copy tests only: consume full parsed/custody clears, leaving blinding
 * spans to their existing per-operation observer. Never read expired storage. */
bool change_custody_retirement_zero(void *pointer, size_t length);
void change_custody_retirement_check(void);
#endif
