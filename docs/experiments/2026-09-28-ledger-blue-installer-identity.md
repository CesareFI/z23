<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Blue development channel identity claim

Local time: 2026-09-28T02:51:10-04:00

UTC: 2026-09-28T06:51:10Z

The installer verifies the session device certificate against an issuer key
provided by the selected device. It checks USB IDs and an encrypted target-ID
reply, but it does not pin a manufacturer root. These checks establish a
working development-channel protocol with the selected device; they do not
establish genuine Ledger Blue identity. The previous success text said
"Verified Ledger Blue target," which exceeded the evidence.

The installer now states that device identity is unverified after the target
reply and when the development channel is established. It reports command
acceptance and catalog results from the selected device without claiming that
the device is a genuine Blue. The image byte pins, certificate verification,
USB checks, target-ID check, and install behavior are unchanged.

With Clang 22.1.6, Release and sanitized Debug builds of `zcl-blue-install`
completed. In each build, `blue-install-params` and
`blue-install-image-cli` passed 2 of 2 tests. The Debug run used
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot start under this
runner's ptrace environment; AddressSanitizer and UndefinedBehaviorSanitizer
remained active. These CLI tests check image rejection before USB, and do not
exercise the new success text with a physical device.
