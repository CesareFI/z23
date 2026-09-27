<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Bounded Blue text layout

Local time: 2026-09-26T23:30:33-04:00

UTC: 2026-09-27T03:30:33Z

## Question

Can a reusable host preview wrap large-font Blue text using the reviewed
BAGL glyph widths while rejecting content that exceeds its display budget?

## Method and result

`blue_bagl_wrap_ascii` accepts an explicit input length, output capacity,
maximum pixel width, maximum line count, and font. It prefers spaces and
splits long words when necessary. Tests cover a fitting two-line label,
hard word split, an overflowing trailing space, output capacity, line
budget, non-ASCII input, and a 512-byte input that requires 128 lines at
52 pixels per line. A 127-line limit rejects that input.

Clang 22.1.6 Debug with AddressSanitizer and UndefinedBehaviorSanitizer:
15/15 tests passed. GCC 16.1.1 Release: 15/15 tests passed. CPU: AMD Ryzen 7
PRO 8840U with Radeon 780M Graphics. The cyclomatic complexity ratchet
passed at cap 15.

## Limit

The API handles printable ASCII only and runs on the host. It does not
validate ZCL memo encoding, paginate Blue screens, or prove device font
identity. Shielded memo display requires those separate steps and a
transaction digest binding before signing is enabled.
