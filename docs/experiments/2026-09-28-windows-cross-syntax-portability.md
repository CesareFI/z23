<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Windows cross-syntax portability (2026-09-28)

On an x86_64 AMD Ryzen 7 PRO 8840U laptop with GCC 16.1.1 and
x86_64-w64-mingw32-gcc 16.1.0, the complete local lint run at source commit
`544ca32ae34e72e1d1b26a6c4d464556edc39c60` found undeclared `lstat`
calls in `codeindex_impact.c` and `devloop_facts_consumer.c` under the Windows
cross-syntax gate. The POSIX call had been used to avoid following a final
symlink. A plain `stat` substitution would have lost that property.

The include query now requires `PLATFORM_FILE_SHAPE_REGULAR`. The facts scan
uses the same no-follow classifier for regular files and
`platform_directory_probe_real` before descending into a directory. Missing,
unreadable, or unclassifiable non-file entries refuse the scan. Symlink entries
remain skipped. These are existing platform APIs, including Windows reparse
handling.

After the change, `make check-windows-cross-syntax` passed 2,376 compiled
translation units with zero new failures, skips, or baselined failures.
`make -j12 t-fast ONLY=test_semantic_consumer` passed both registered groups
with zero failures or skips; the live group exercised 36 conservative TU
predictions with unchanged objects. `make -j12 lint-fast` passed all 33 gates.
The first complete lint run after the change passed 211 of 213 gates. The
capability-closure gate found that `codeindex_impact.c` still declared a direct
filesystem-read import after its `lstat` call was replaced by the platform
classifier. Its module row was narrowed to `CAP_FS_WRITE`; the platform module
owns the file read. The code-index coverage gate refused a rebuild while a
parallel compile gate changed source or depfile metadata. Run alone, it passed
with all 6,332 tracked maintained sources indexed. The lint driver now runs
that gate after the parallel pool, preserving its 100% threshold.

The next complete `make -j12 lint` run on signed commit
`74dea2bf4596dacf85ec69e7f9e0d3daddc98747` passed all 213 gates. The
serial code-index gate measured 6,332 of 6,332 tracked sources; Windows
cross-syntax and capability closure also passed. The lint driver reported
1,636,258 ms wall time with eight gate workers.
