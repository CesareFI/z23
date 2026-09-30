<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->
# Ledger Blue approved digest binding

## Question

Will the Wallet sign an input digest that changes after the Blue's final
SIGN ZCL tap, including the remaining input after a first valid signature?

## Experiment and correction

The device UI test prepared a valid final screen, tapped SIGN ZCL, changed
one byte in the first stored ZIP-243 input digest, then sent a valid signing
APDU. Before correction, the signer ran and the test failed its required
`0x6985` rejection assertion. The same regression changes the second input
digest after a valid first signature. Both cases now refuse the APDU before
the signer runs, erase the APDU reply and all input records, and end approval.

The touchscreen approval callback hashes all 16 fixed-size input records
into the final screen's address field after checking the displayed payment
facts. The approved screen no longer uses that field. Each signing APDU
checks the record hash before passing a digest to the signer. A successful
nonfinal signature erases its consumed record and refreshes the hash for the
remaining ordered inputs. Hash failure aborts the payment. The record hash
and the approval state are erased by the existing abort path.

This detects changed device memory between approval and signing; the test
injects the change directly into the simulated device state. It does not
show that a USB host can modify that memory. The test for a malformed
signing command now reaches approval through the actual touchscreen route,
so it exercises the record snapshot as well as its prior frame rejection.

## Measurements

On 2026-09-29T03:07:03-04:00 (2026-09-29T07:07:03+00:00), Clang 22.1.6 on
an AMD Ryzen 7 PRO 8840U built Wallet 0.3.41 against pinned Blue SDK commit
`3c710b4c62ad847599a2deb0932a50dd1ae4bdff` with required patch SHA-256
`4919fd81ba7a3a80880065aaf7898c2edd603b2586f6086a88570c73996019f6`.
The ARM image used 51,968 bytes of `.text`, zero `.data`, and 5,120 bytes of
`.bss` including the 2,048-byte reserved stack. The stack checker reported
880 bytes for approval touch and 1,144 bytes for the largest payment upload
path, with its required 512-byte margin. BOLOS frames are excluded.

Release and sanitized Debug CTest passed 60/60 each. Debug used
`ASAN_OPTIONS=detect_leaks=0 LSAN_OPTIONS=detect_leaks=0` because the
environment's LeakSanitizer fails before test execution under process
tracing. Address and undefined-behavior instrumentation remained active.
No physical Blue was available for this build.

After signing commit `e4c03edeecdfe81e338f019d99040cfd45575d35`, a
fresh source checkout built Wallet 0.3.41 against a separately patched copy
of the pinned SDK. Both builds produced `.text` SHA-256
`4bb94d1f81314e27df25ac35d7ed5d2abfabb7408fd0d7ef466433236b881afe`
and Intel HEX SHA-256
`c6f6b3f9003cc9e35d666df0987642a6a2ba2959e4fa1425f461f1517d32fdf8`.
Each measured 51,968 `.text` bytes, zero `.data` bytes, and 5,120 `.bss`
bytes. The independent ARM stack check reported the same 880-byte approval
and 1,144-byte maximum payment paths. The 33 fast lint gates, unchanged core
seal, cyclomatic cap 15, and both Markdown gates passed on the source.
