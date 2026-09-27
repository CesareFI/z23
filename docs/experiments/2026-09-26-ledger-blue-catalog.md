<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Authenticated Ledger Blue app catalog

Recorded: 2026-09-26T20:00:43-04:00 (2026-09-27T00:00:43Z)

Ledger's open-source Blue loader sends secure manager commands `0e` and `0f`
to start and continue an app-list read. Z23 now sends those commands through
its existing owner-CA-authenticated channel and bounds each response before
printing names. The parser rejects malformed format-1 records and control
characters in names. A previous unauthenticated `E0 DE` request returned
`6d00` on this BOLOS 2.1.1 device.

The dedicated Blue answered the new read-only command without a button press:

```text
Verified Ledger Blue target 31010004 over the secure channel.
ZCL Sign Test
ZCL Review
ZCL Probe
3 application(s) listed by Ledger Blue.
```

The catalog confirms that a ZCL Review app record exists despite its icon
being absent from the owner's visible home screen. It does not return an app
version or prove that the app opens or handles a transaction. No app was
installed, deleted, opened, or asked to sign during this query.

Clang Debug with address and undefined behavior sanitizers and GCC Release
each passed 12 of 12 local Ledger tests after adding malformed-page tests.
The repository cyclomatic complexity gate passed at its cap of 15.
