<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue installer image-file boundary

Local time: 2026-09-27T10:30:45-04:00

UTC: 2026-09-27T14:30:45Z

## Result

The C23 Blue installer now opens an app image without following a final
symlink and with nonblocking file access, then requires a regular file of
1,024 to 65,536 bytes whose length is divisible by 64. It reads exactly
those bytes before comparing SHA-256 against the installer's reviewed image
profiles. A changed file size or read failure stops before USB access.

The CLI regression passed an unknown but correctly shaped image, a symlink
to it, a named pipe, and an oversized sparse file. Each was refused before
opening the nonexistent device path; the named pipe returned before its
five-second child timeout. The installer and regression compiled with C23
and passed under Clang 22.1.6 Debug/ASan/UBSan and GCC 16.1.1 Release on an
AMD Ryzen 7 PRO 8840U. The full Blue/ZCL host suites passed 31/31 tests on
both builds before the final function split; the focused regression passed
again after it. The repository cyclomatic-complexity ratchet then passed
60,799 functions at cap 15.

## Limit

This checks refusal of unrecognized and unsuitable files. A known pinned
image was not installed in this test, and no Blue hardware or secure-channel
operation was performed. The SHA-256 allowlist still requires an explicit
review and pin for each future device image. An intermediate directory may
still be a symlink; the image's actual bytes are read into memory and hashed
before installation.
