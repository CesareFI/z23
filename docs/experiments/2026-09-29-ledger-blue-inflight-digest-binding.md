<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue in-flight digest binding

## Question

Can the signer callback change a later input record after the first signing
APDU passes its pre-sign check, causing the Wallet to accept the altered
record for a subsequent signature?

## Experiment and correction

The device UI test approved a two-input payment, then changed the second
input digest inside the first signing callback. Wallet 0.3.41 returned the
first signature and refreshed its input-record hash to the changed state;
the new test failed its required `0x6985` rejection assertion. This is an
injected device-state fault, not a demonstrated USB-host write.

Wallet 0.3.42 retains the check before each signing APDU. Its signer wrapper
hashes the remaining input records immediately after the current record is
consumed, checks them after the signer callback, and refuses to release a
signature if they differ. The APDU completion path checks the same hash
after the callback. On rejection, the shared APDU reply and input records
are erased and approval ends. The two-input callback mutation regression
now passes; an unchanged two-input signing flow remains covered by the
device UI test.

The ARM stack checker now requires the signer wrapper, record matcher, and
SHA-256 wrapper frames. It includes their largest modeled call path and the
hash path on the approval callback. Removing the signer-wrapper frame from
a copy of its `.su` input makes the checker fail with
`Missing stack frame: sign_bound_digest`.

## Measurements and limits

At 2026-09-29T03:17:59-04:00 (2026-09-29T07:17:59+00:00), Clang 22.1.6
on an AMD Ryzen 7 PRO 8840U built Wallet 0.3.42 against pinned Blue SDK
commit `3c710b4c62ad847599a2deb0932a50dd1ae4bdff` and reviewed SDK
patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The image uses 51,968 bytes of `.text`, zero `.data`, and 5,120 bytes of
`.bss` including its 2,048-byte stack reservation. The stack checker
reported a 920-byte approval touch path, 1,088-byte signer derivation path,
1,048-byte record-check path, and a 1,120-byte largest modeled payment
path, with a required 512-byte margin. BOLOS frames are excluded. The
`.text` SHA-256 was
`e10204649a59b4dffca82b24975aa395cc88ef331709c486aa23aa8f12b5f1a7`;
the Intel HEX SHA-256 was
`fb9356971a03457654ed564a713e7d9614d35e654595d8949cbf9795cd157f79`.
The image has not been installed on a physical Blue.

Release and sanitized Debug CTest each passed 60/60 cases. Debug used
`ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` because this
environment's LeakSanitizer fails under process tracing before test
execution; address and undefined-behavior checks remained active.

A fresh checkout of signed commit
`4183464ba23bb89687c1a887432f20895539d789` built against a second
separately patched copy of the pinned SDK produced the same `.text` and HEX
hashes and section sizes shown above. Its stack checker reported the same
paths. On the source, all 33 fast lint gates, the unchanged consensus-core
seal, cyclomatic complexity cap 15, and both Markdown gates passed.
