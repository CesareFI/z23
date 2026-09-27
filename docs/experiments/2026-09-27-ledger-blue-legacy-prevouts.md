<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Legacy previous transactions in Blue host preflight

Date: 2026-09-27. The physical Blue review CLI began requiring exact
previous-transaction bytes for each transparent input, but its host
preflight accepted only Sapling v4 previous wires. ZCL's transaction decoder
also accepts legacy v1/v2 and Overwinter v3. A valid v4 spend can refer to a
transparent output created by any of those older versions.

The C23 host preflight now selects the indexed output from exact v1, v2,
Overwinter v3, or Sapling v4 wire bytes. It enforces the corresponding header
and version group, canonical CompactSize lengths, bounded scripts and vector
counts, money range, version-specific JoinSplit and Sapling tail lengths, and
end of file. The selected script pointer remains borrowed from the supplied
wire. The preflight computes SHA-256d over those same complete bytes and
compares the input outpoint before accepting a standard P2PKH script and
amount. The *spending* transaction remains unsigned, all-transparent v4;
there is no device signing or expanded payment type.

Synthetic fixtures cover all four versions, nonstandard unselected outputs,
v2/v3/v4 JoinSplit tails, a v4 Sapling spend/output tail, exact fee, and
equality between direct and hash-bound ZIP-243 input digests. Tests reject a
selected OP_RETURN output, wrong version group, noncanonical input count,
out-of-range amount, truncated script, missing JoinSplit body, and trailing
bytes. A deterministic 40,000-case byte-mutation run checks failure
atomicity and accepted script bounds under the host test toolchains.
The CLI integration test writes each previous version to a separate file,
constructs a v4 spend referencing its SHA-256d txid, and verifies that all
four pairs pass the provenance gate before the inaccessible-device check.
No physical USB interface is opened in that test. These
fixtures establish structural parsing and binding; they do not verify
JoinSplit proofs, Sapling proofs, signatures, chain inclusion, unspent
status, maturity, or account ownership.

Clang 22.1.6 Debug with address and undefined-behavior sanitizers passed
23/23 host tests; GCC 16.1.1 Release passed 23/23. The repository cyclomatic
ratchet passed at cap 15 across 60,425 functions in 4,409 files. The
selection call is used by host preflight. Its source file is also linked by
the separate legacy Review app. Two independent patched-SDK Review builds
matched `.text` SHA-256
`05317ce40e2aa8747b2e2f8d41ccab5f81eaa18537f0f2f115949672d565a149`;
the image has 32,256 bytes of `.text`, 6,000 bytes of `.bss`, zero `.data`,
and only 144 bytes outside its reserved stack. It was not installed or added
to the installer. Wallet 0.2.4 does not link this parser file; its prior ARM
image hash remains
`fb138d05d3c5c9a3b0850f02d00572779dafc8f2aab292d44bc54634c98a8abb`.

The new selection result
must remain a preflight fact until a future device protocol independently
binds prevout amounts and scripts to all inputs and a verified node view.
