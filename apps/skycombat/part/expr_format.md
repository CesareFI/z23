<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 -->
# Data-only HUD expression format, version 0

The validator admits inert bounded data. This commit has no evaluator,
assembler, renderer, loader, activation, or execution authority. Input memory
must be valid and immutable for the call. `expr_validate` retains no pointers,
allocates nothing, and changes the required `expr_info` output only on success.
Optional `expr_error` receives a fixed trusted reason and record index; refusals
log context. Status names and values are declared in `expr_format.h` (OK=0).

Integers are explicit little-endian bytes, never native structs. I32 immediates
are two's-complement bit patterns. There is no padding, checksum, compression,
extension or trailing byte. External hashes identify bytes, not correctness.
Section order: header, expressions, text descriptors, pieces, draws, literals.
Exact size is `32+16*E+8*T+12*P+24*D+L`, at most 10272 bytes. All counts may be
zero; the 32-byte empty part is valid. Counts precede length arithmetic/access.

| Header byte offset | Width | Required value |
|---|---|---|
| 0 | 4 | ASCII HUDX |
| 4,6 | 2 each | version 0, header size 32 |
| 8 | 4 | normalized snapshot schema 1 |
| 12,14,16,18 | 2 each | E<=256, D<=64, T<=64, P<=256 |
| 20 | 4 | L<=1024 |
| 24,28 | 4 each | zero |

Every reserved/unused field must have its specified value. References are u16
indices; NONE=65535. Unknown identities, kinds, fields or opcodes refuse.
An expression is 16 bytes: opcode u8 at0, reserved zero u8 at1, operands
a/b/c u16 at2/4/6, immediate u32 at8, reserved zero u32 at12. Active operands
refer strictly backward; inactive operands are NONE. Only CONST_I32 and FIELD
use the immediate; all other immediates are zero. Types are inferred as I32
or BOOL, with no implicit conversion.

| Opcode | Name | Arity | Required types -> result |
|---|---|---|---|
| 1 | CONST_I32 | 0 | immediate bits -> I32 |
| 2 | FIELD | 0 | schema field id -> field type |
| 3,4,5,6 | ADD_SAT,SUB_SAT,MIN,MAX | 2 | I32,I32 -> I32 |
| 7,8 | CLAMP,MULDIV | 3 | I32,I32,I32 -> I32 |
| 9,10 | LT,LE | 2 | I32,I32 -> BOOL |
| 11 | EQ | 2 | matching types -> BOOL |
| 12,13,14 | AND,OR,NOT | 2,2,1 | BOOL operands -> BOOL |
| 15 | SELECT | 3 | BOOL, matching branch types -> branch type |

Schema fields 0..12 are I32: screen_w, screen_h, elapsed_seconds, team_count,
score_0..score_3, score_limit, health_milli, max_health_milli, boost_ms,
weapon_index. Fields 13..15 are BOOL: shield, connected, flash. The format does
not admit other host data, pointers, imports, callbacks, commands or jumps.
Validation checks structure/types, not numeric domains or evaluation results.
Division by zero, reversed CLAMP, numeric draw ranges, normalization and eager
evaluation remain future evaluator duties; successful validation proves none
of these runtime properties or rendered behavior.

Text descriptors are 8 bytes: first/count u16 at0/2, reserved zero u32 at4.
Counts are nonzero and descriptors exactly partition P in order, with no gaps
or overlap. Pieces are 12 bytes: kind u8 at0, reserved zero u8 at1, ref u16 at2,
offset/length u32 at4/8. LITERAL=1 requires ref NONE, offset equal to the running
pool cursor in piece order, length<=L-offset, then advances the cursor. The
final cursor must equal L. Zero-length literals use the current cursor,
including at L. Sharing, reordering, overlap and unused pool bytes refuse.
All pool bytes must be printable ASCII 32..126. DECIMAL=2 requires an existing
I32 ref and zero offset/length. Its conservative output length is 11 bytes.
All templates' literal lengths plus 11 per DECIMAL must total <=1024.

Draws are 24 bytes: kind u8 at0, zero u8 at1, BOOL visibility ref u16 at2,
seven I32 refs x/y/w/h/rgba/font_px/align u16 at4/6/8/10/12/14/16, text ref u16
at18, reserved zero u32 at20. RECT=1/RECT_LINES=2 require text NONE and actual
CONST_I32 zero font/align. TEXT=3 requires valid text and CONST_I32 zero w/h.
TEXT_BOX=4 requires valid text and CONST_I32 zero align. Computed zeros refuse
in structural fields. Sum of worst template lengths over all text draws must
be <=1024, including repetitions and invisible/mutually exclusive draws.

The bounded scan consumes wire sections once forward; only bounded inferred
types, structural-zero flags and text budgets are revisited. Canonical literal
ownership is representation canonicality, not uniqueness of equivalent DAGs.
