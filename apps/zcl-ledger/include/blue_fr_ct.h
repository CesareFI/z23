/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_FR_CT_H
#define ZCL_BLUE_FR_CT_H

#include "sapling/fr.h"

/* Isolated Cortex-M3 field candidate. Inputs must be canonical Montgomery
 * elements. No device key or signing command calls these functions. */
void blue_fr_add_ct(struct fr *result, const struct fr *a, const struct fr *b);
void blue_fr_sub_ct(struct fr *result, const struct fr *a, const struct fr *b);
void blue_fr_neg_ct(struct fr *result, const struct fr *a);
void blue_fr_mul_ct(struct fr *result, const struct fr *a, const struct fr *b);

#endif
