<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Sapling review screen pixel checks

Local time: 2026-09-28T04:22:27-04:00

UTC: 2026-09-28T08:22:27Z

The C23 Blue screen simulator now runs both the 4,118-byte ZIP-243 vector
and the 1,425-byte consensus-accepted simnet Sapling spend fixture. Each run
creates 41 PNGs. The test checks every image's PNG header and 320 × 480
dimensions. For the consensus fixture, it decodes the summary, dark summary,
large-text shielded count, and digest PNGs to RGB pixels and checks their
SHA-256 values. Decoding before hashing makes the check independent of PNG
compression bytes.

The four decoded RGB SHA-256 values are:

| Page | SHA-256 |
| --- | --- |
| Summary | `16135e256da8af13db033defa4c19611685cc7eb47bbfe9eade634c227005385` |
| Dark summary | `73c75e24ef515e732744d045bef9d9605566834db6c6bba9dc3becee56af980a` |
| Large-text shielded count | `9faaf6574bbbd724c2e72d98a42e35d31ba1f4f9455ded8c29df634a2787b1f5` |
| Digest | `7b22444dee23fbf63cf49cb758480830b70f0c4e706c99ab2f87648d98c0d0fa` |

The four pages were inspected at native 320 × 480 resolution. They display
the 1-spend/1-output count, `FEE UNKNOWN`, `SHIELDED HIDDEN; NO SIGNING`,
the digest, readable large text, distinct themes, and visible EXIT controls
without overlap. The targeted case passed under Clang 22.1.6 Release and
AddressSanitizer/UndefinedBehaviorSanitizer Debug on an AMD Ryzen 7 PRO
8840U. Debug used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot
start in this runner's ptrace environment.

The host renderer approximates the Blue's BAGL glyphs. These checks detect
screen regressions in the simulator; they do not establish physical Blue
pixels, touchscreen behavior, shielded recipient amounts, or a shielded
signing path. The review app remains read-only.
