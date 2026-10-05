/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 * purpose: Define the bounded data-only HUD wire validator contract. */
#ifndef SKY_EXPR_FORMAT_H
#define SKY_EXPR_FORMAT_H
#include <stddef.h>
#include <stdint.h>
#define EXPR_NONE UINT16_MAX
#define EXPR_MAX_BYTES 10272u
enum expr_opcode { EX_CONST_I32=1, EX_FIELD, EX_ADD_SAT, EX_SUB_SAT,
 EX_MIN, EX_MAX, EX_CLAMP, EX_MULDIV, EX_LT, EX_LE, EX_EQ, EX_AND,
 EX_OR, EX_NOT, EX_SELECT };
enum expr_type { EX_I32=1, EX_BOOL };
enum expr_field { XF_SCREEN_W, XF_SCREEN_H, XF_ELAPSED_SECONDS, XF_TEAM_COUNT,
 XF_SCORE_0, XF_SCORE_1, XF_SCORE_2, XF_SCORE_3, XF_SCORE_LIMIT, XF_HEALTH_MILLI,
 XF_MAX_HEALTH_MILLI, XF_BOOST_MS, XF_WEAPON_INDEX, XF_SHIELD, XF_CONNECTED,
 XF_FLASH, XF_COUNT };
enum expr_status { EX_OK, EX_ARGUMENT, EX_FORMAT, EX_BOUNDS, EX_LIMIT,
 EX_OPCODE, EX_TYPE, EX_REFERENCE, EX_FIELD_ID, EX_SCHEMA, EX_LITERAL,
 EX_TEXT_BUDGET };
struct expr_info { unsigned expressions, draws, texts, pieces, literal_bytes,
 worst_text, worst_draw_text; };
struct expr_error { enum expr_status code; unsigned index; const char *reason; };
/* bytes must denote n valid immutable bytes for the call. No allocation or
 * input retention; info is required and changes only on success. error may be
 * NULL; its trusted reason/index describe the rejected record, not an offset. */
enum expr_status expr_validate(const void *bytes, size_t n,
 struct expr_info *info, struct expr_error *error);
#endif
