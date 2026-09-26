<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Ledger Blue installer profile selection

Recorded: 2026-09-26T19:50:03-04:00 (2026-09-26T23:50:03Z)

Adding the pinned ZCL Review 0.3.0 profile moved ZCL Sign Test to a later
array position. Both `--delete-sign-test` and `--ca-delete-sign-test` still
selected the previous position, which held ZCL Review 0.3.0. No deletion
was sent to the Blue after this change. Delete options now resolve their
target by app name and reject a missing profile, so an added image version
cannot silently change the deletion target.

Clang Debug with address and undefined behavior sanitizers and GCC Release
each built and passed 11 of 11 local Ledger tests. The repository cyclomatic
complexity gate passed at its limit of 15. The Blue was at BOLOS 2.1.1 home;
an unauthenticated, read-only `E0 DE` app-list request returned `6d00`, with
no payload. It did not establish whether ZCL Review 0.2.0 is installed.
