<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Worldstream: bounded native startup progress reads

The fresh-sync benchmark polled startup progress by spawning `tail -1` every
ten seconds. This observer does not advance synchronization, yet each sample
paid process and shell-command overhead. Buffered tail reads could also pull a
preceding filesystem block when only the final 256 bytes were needed.

Baseline measurement on this Linux x86_64 host used the same isolated
67,112,959-byte sparse log for 3,000 reads. The buffered native baseline
returned 12,285,103 / 12,285,108 / 12,285,108 bytes from the kernel and took
20.643 / 20.532 / 20.545 ms. The unbuffered candidate returned 768,102 /
768,106 / 768,107 bytes and took 18.401 / 18.253 / 18.262 ms. Thus the
low-level read amplification fell about 94% and median reader wall time fell
about 11%, before also accounting for the removed `tail` subprocess. These are
warm-cache observer measurements, not an end-to-end IBD throughput claim.

The benchmark now reads a snapshot of at most 256 final bytes directly from
its child log, without a subprocess. The single-use stream disables stdio
read-ahead. Missing, empty, unterminated and overlong final lines remain
non-authoritative display observations; they do not affect readiness.

The hermetic regression checks nine extents around buffer and page boundaries,
compiles with warnings as errors and GCC static analysis, and performs three
3,000-read trials. On Linux, `/proc/self/io` additionally proves that each
sample returns no more than the requested 256 bytes plus a small accounting
allowance. Other hosts retain the value and boundary checks.

This changes benchmark instrumentation only. Node execution, independent
validation, consensus, optional acceleration, peer scheduling, block requests,
recovery and database behavior are unchanged.
