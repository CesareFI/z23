<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Wallet C upstream publication — 2026-10-03

Primary mission: port useful wallet safety work into the existing Z23 owners as
small independently validated PRs. Do not import this entire Android application.
This ledger stays on the wallet development branch; PRs carry concise provenance.

## Captured identities

- Upstream/origin: https://github.com/z23c/z23.git, main
  `3a93e60ebf922af3d119b9facc1d95803f42844b` (fetched under both remote-tracking names).
- Android branch: `agent/android-jni-secret-retirement-20260915`, HEAD
  `6fdcb046825504684133b8cffe3fd952ae986183`; signed and exact backup-SHA verified.
- Backup: https://github.com/CesareFI/zclassic-android-wallet.git.
- Other development backup: https://github.com/CesareFI/zclassic-node-development.git.
- Publication fork: https://github.com/CesareFI/z23.git; verified writable and
  fork parent `z23c/z23`. Upstream remains read-only; no permission change attempted.
- Reviewer `RhettCreighton` verified from upstream commit metadata and GitHub's
  matching full-name profile. No open upstream PR at initial inspection.
- Unfinished file-size-limit test in `native/tests/test_storage_limits.c` remains
  uncommitted in Android checkout. It is outside the publication baseline.
- Publication branches start from upstream main in isolated worktrees; no Android
  branch history is rebased, reset or overwritten.

Date-filtered inventory uses `--since-as-filter` across all known refs because
commit dates are nonmonotonic. It finds53 relevant commits since2026-09-26.
The full preserved Android ancestry has256 application commits; older prerequisites
are included in suitability assessment instead of silently excluded by date.

## Reuse inventory and proposed disposition

A = directly reusable; B = reusable with adaptation to an existing upstream owner;
C = Android/JNI reference only; D = not appropriate for this upstream submission.
The reviewed disposition below separates publication candidates from reference-only
material. Candidate heads and the publication boundary are recorded below.

| Item / source surface | Class | Upstream destination or concrete constraint |
| --- | --- | --- |
| C wallet core as one library / status APIs | D | Upstream already owns wallet, crypto, codecs and persistence. Importing another complete stack duplicates authorities and is not a small reviewable change. Port individual invariants below. |
| Optimizer-resistant secret wiping and last-use retirement | B | Mnemonic-index and master-key/context scratch candidates use the existing support/cleanse and wallet owners. Keep upstream primitive, provider and caller ownership. |
| Derived scalar, chain-code and failure cleanup (`bip32`, `receive_key`) | B | Existing keys/key and domain/wallet/key_derivation. Reuse failure-atomic/retirement regression methods, preserve exact BIP32 derivation. |
| Entropy/random-source failure handling | D | Upstream GetRandBytes already aborts on entropy failure, and random_secret rejects/clears failed secret output. Its stale header narrative does not establish a production defect. No duplicate RNG or provider is appropriate. |
| Signing-context retirement (`6fdcb0468`) | C | Android owns a context per call; upstream deliberately owns a process-wide signing context. Do not transplant per-call destruction into that lifecycle. Failure/publication tests can inform appropriate existing APIs. |
| Consuming creation entropy (`4c6c41cfb`) | C | Applies to private JNI scratch and Android authenticated initial journal creation; no equivalent upstream record/API. Keep reference evidence. |
| Transaction review/signing identity, change and monetary bounds | C | Upstream vault-intent payloads already bind effects/fees and rebuild against current inputs. Android offline review tokens and unsigned-only codec have no equivalent consumer; importing them would create a second authority. JNI scalar admission is not an upstream C API. No consensus or fee-policy change is proposed. |
| JNI fee/draft early admission (`91db31c76`, `98ea5c2fe`, `356fb2586`) | C | No JNI API upstream. Preserve tests as reference for scalar-before-copy and ownership admission; upstream C monetary checks stay authoritative. |
| Fixed-record storage durability, no-overwrite, restart/recovery | D | Upstream backup uses private-file authority barriers, atomic replacement and parent flush; no-clobber publication already exists separately. Android fixed-record files have a different format and overwrite contract from SQLite/AR backup. Do not port the format or replace the owner. |
| Authenticated change journal and bounded suffix repair | D | App-specific format/key derivation and caller custody prerequisites; no compatible upstream consumer. Import would require new migration/recovery policy rather than small adaptation. |
| Descriptor retirement / CLOEXEC / kernel pressure | B | The test-only candidate verifies all six upstream private-file openers. Their flags were already correct; Android kernel-pressure and journal fixtures remain format-specific. |
| Crash/process interruption and partial IO fixtures | C | Android injected write/rename checkpoints name its authenticated journal and exact record layout. Upstream wallet_backup and wallet_backup_port already test backup encryption, restoration and authority barriers. Transfer descriptor invariants separately; the format-specific crash harness is not an independent upstream component. |
| BIP39/12-word recovery / normalization / deterministic seeds | B | Existing domain/wallet/mnemonic and wallet wrapper. Preserve upstream accepted formats and canonical bytes; never change12-word creation/recovery guarantees. |
| Prepared HMAC/PBKDF2 reuse | D | Upstream already has its own crypto implementation in sealed core. No duplicate provider or unseal authority; only report existing parity and owner constraints. |
| Base58 bounded significant-digit work (`e805c8a06`, `32e58fad9`) | B | The significant-span candidate adapts both optimizations to platform/domain/encoding/base58 with measured speedup, exact reference comparisons and unchanged cleanup. |
| Payment URI/text parser and RPC string fast paths | C | These optimizations target Android payment metadata and the read-only mobile Electrum JSON schema. Replacing upstream native command/RPC policy or adding an unused duplicate parser is outside a small adaptation. |
| Platform-neutral QR/camera stride/pixel bounds | C | Upstream owns QR generation through qrcodegen, not Android camera-plane decoding. There is no matching camera consumer for stride/pixel APIs; retain the validated bounds as reference rather than introduce an unused decoder stack. |
| JNI camera minimum copy / VM references/exceptions | C | Android transport only; documents required admitted-span/caller-ownership semantics. |
| Shielded address envelopes | D | Upstream already owns Sapling address serialization/deserialization and deterministic seed/address vectors. Android envelope-only handling does not add spending/proof support. Do not create a second codec or change sealed validity rules. |
| Read-only Electrum / freshness / owner replay logic | D | Unverified mobile presentation state is not a full-node authenticated-chain authority. No alternate freshness or spending policy. |
| Native fuzz harnesses, mutations and deterministic regressions | B | Port relevant cases with each production slice using canonical upstream test registration. Do not bring whole CMake registry or duplicate gate runner. |
| ASan/UBSan/LSan, qualified MSan/TSan and compiler evidence | B | Rerun on each port. Android observations support source provenance only; no claim they validate adapted upstream bytes. |
| ARM64 Linux/QEMU and Android runtime evidence | C | Exact environment claims only; no physical-device proof and no transfer of evidence to changed upstream code. |
| Android UI, lifecycle, Kotlin/JNI build and APK fixtures | C | Preserve on Android branch; exclude from C PRs. |
| TLS candidate/provider experiments | D | Quarantine stays intact; no publication implying security acceptance. |
| Evidence and documentation | B | Small PR-specific problem/behavior/safety/tests/provenance descriptions. This ledger and long app progress history stay outside upstream PRs. |

## Publication queue

1. Secret-bearing temporary retirement in existing wallet C owners, with live-span
   observation and mutation controls derived from Android native safety fixtures.
2. Corresponding storage/backup descriptor, durability and recovery invariants.
3. Measured reusable codec improvement and its deterministic/fuzz coverage.

Transaction/JNI and format-specific recovery APIs remain reference-only for the
concrete ownership reasons in the inventory, rather than becoming a duplicate stack.

A portable source file alone is not sufficient reason to submit it. Each B item
must gain an exact upstream owner and bounded diff or receive a concrete final
non-publication reason. Every submitted PR records base/head, tests, CI and review
request status here. Four independent upstream candidates are signed and validated;
publication remains subject to the explicit hook conflict described below.

## Date-filtered source provenance

6fdcb046825504684133b8cffe3fd952ae986183 2026-10-03 fix(wallet): retire signing context before public verification
4c6c41cfbc6a59a49ba98c9c6a39d1e455c1e6c1 2026-10-03 fix(wallet): retire creation entropy before public storage work
3d76b3aab589c18c5391230bb62196896dd15bfc 2026-10-03 test(android): cover zero and minimum signed review fees
91db31c76e836675426a9f522bb42bae4c26bf0e 2026-10-03 fix(android): reject out-of-range review fees before input copies
d9dbb710ab0948039c79846497616933441943a2 2026-10-03 test(wallet): bound compile-manifest mutation work
98ea5c2fe17f58f36e9f27f72f7c6c0f848f7aab 2026-10-03 fix(android): admit full-review draft before source allocation
ec789d089fe7efa89c545058b1fee03378751073 2026-10-03 docs(wallet): qualify instrumented MemorySanitizer fuzzing
fe8fb311ba8e85ccedc3acdc6c0ffb898f0194aa 2026-10-03 test(android): intercept fortified change-journal crash writes
733b5cd41d4623bb061d39b66d5d222a9f5919da 2026-10-03 test(wallet): exercise kernel descriptor exhaustion and recovery
04d8d71e057e5e6383dc572d99162d0cb056901b 2026-10-03 docs(wallet): qualify native MemorySanitizer profile
1c0075d71454e422986d0bad3446aec36036684c 2026-10-03 test(wallet): verify pixel retirement during decoder resize
aa3e113c2745ed22cdb666754b771839c43d9a63 2026-10-03 test(wallet): enable explicit headers for ARM64 JNI fixtures
58c8b8d8105cb8ce2bfb3db7cea544e5cca581e1 2026-10-03 perf(android): copy only the validated QR image span
06c779d20fb3fe50e9c059667b973fa9724b8425 2026-10-03 test(wallet): observe storage descriptor retirement across exec
356fb2586df2d80dc8cbcfa03a47fd121e34ca43 2026-10-03 fix(android): admit review parameters before source copies
f2b6b22b405a387f618360d59b4dbf79e0434884 2026-10-03 test(wallet): preserve fortification under storage fault injection
c82ce87482e82cd01d20cee9ea9eff83ec88ba8e 2026-10-03 test(android): inject faults through fortified storage IO
b11e2e9fc04fe90186e377c795cccf067a7e0739 2026-10-03 test(wallet): interrupt recovery promotion durability stages
b090a545ba7b5b0ad6b6e22723f8f0cbbf51c29f 2026-10-03 fix(wallet): retire owned seed before public address conversion
ffe0a7b6b5c246f736442519a03c3a20ee660fbc 2026-10-03 test(android): qualify late sync snapshot failures
e44cef40a2dadc27ce15f2e793d71859e9119ede 2026-10-03 fix(android): publish status-only sync snapshot errors
351fe3a88ce4ba02d214581e1501fd6089a76f9c 2026-10-03 test(wallet): fuzz sync freshness and clock overflow boundaries
39a620aa36cc395e0e8ee0692a5ebbd454a08581 2026-10-03 test(wallet): measure verified native storage operations
b2fd5db33fd2c106be11d6fa67c6994e7ce69568 2026-10-03 fix(test): retire storage fixture directories after open failure
d68a024dbf30dfac74fa3996f58bd57f22e5ca84 2026-10-03 fix(android): retire sealed entropy before public storage readback
6b893dd819ba6e3fb9b6605c056e0b5bcb10cb1c 2026-10-03 test(android): race retired replies against sync replacement
928274961fd821916d50a3ba5612a5be3aa65648 2026-10-03 fix(android): reject retired sync replies before frame allocation
c6dc1f9121678f27dd4cef21fcb9bc837e1364a8 2026-10-02 Validate JNI payment metadata flags
aad367752b9a88071687ac814aac08068a68476a 2026-10-02 Refuse null JNI payment records
0fb8c68f48119873d46ce645440399a469fc0bb8 2026-10-02 Simplify atomic payment URI decoding
a2b292966960216033f1e2bdff6d4d81be5b1f0f 2026-10-02 wallet: stop public text range scans once intervals cannot match
101f5ce7f8b3b5d4b6374b19a03c98542c9219b1 2026-10-02 wallet: compare unescaped RPC strings without decoding
32e58fad9473d1f6dc0db07384d03a197e844db7 2026-10-02 wallet: limit Base58 decoding to the significant magnitude
1830d272702901b9d5f602ca2ba21711bd976dcf 2026-10-02 wallet: batch bounded recovery address derivation
70f93be0af29a2d28e8f044b12a208d21b0aef91 2026-10-02 wallet: reuse validated camera sampling strides
fba8d3b090fe06cf6a9c96a541ed7c010d033211 2026-10-02 wallet: render receive QR into the validated output span
c159b2b141548df1f3a71cd49c461084fb7ffd52 2026-10-01 wallet: rerun JVM tests when loaded native libraries change
e805c8a06401a2b392b34291a0415e1b75792037 2026-10-01 wallet: skip zero prefixes during Base58 encoding
a3ac7f9c0aced86ee9c0840d4f3db22b36b996f0 2026-10-01 wallet: qualify twelve-word recovery and bounded native fixtures
9b09172befe3338b3b2a8f7e021d557e157a6c41 2026-09-28 docs(wallet): qualify current platform fixes on 16 KiB Android
3947237090521b2c5f4f2c1f5a408a711d53a73d 2026-09-28 fix(android): erase recovery input on failed UI handoff
4281a24db41f486f3d19f1794141a0167c3ba95b 2026-09-28 fix(android): retire idle workers after shutdown failure
2dd363750602afd3c75b50642dc175fbddb9b913 2026-09-28 docs(wallet): qualify minified camera flow on 16 KiB Android
7f7fb5d625d046703af9d9d936134d71139f178a 2026-09-28 docs(wallet): qualify current Android 16 KiB runtime
11877db263ace0d1b7334799ce5290d1a48d2d11 2026-09-28 fix(android): keep wallet controls outside system bars
8d5202de5dd7194226ca156431f337d9da7ca12b 2026-09-28 fix(android): clear secret views before restarting the wallet
d5c6550d41425e15855a392625156c5e34ca7817 2026-09-28 docs(wallet): qualify native MemorySanitizer coverage
7906e3f0400f303de7605f2161f0c6e60be8cf87 2026-09-26 docs(wallet): record current camera lifecycle qualification
cd897a41cb480930548aaafaaa6e856823d7ef61 2026-09-26 fix(android): retire secret views when session cleanup fails
f4c27fcf580e45f7e078c04c61fcb7153b4a7725 2026-09-26 test(wallet): cover direct JNI reply and replacement races
4a4b5dad1b271c2d6a9fb1b35e74a349579ee002 2026-09-26 perf(wallet): size active reply buffers to bounded input
454199d24850e25c7a87e6158354226c23159233 2026-09-26 perf(wallet): reject retired sync replies before copying frames
666e84a57883060117d7ebd56be140e8ce39cd0c 2026-09-26 test(wallet): enforce header acceptance and corruption contracts

## Prepared upstream candidates and publication boundary

The isolated sibling publication checkout is `z23-wallet-publication-20261003`.
These are independent ports from upstream `3a93e60ebf922af3d119b9facc1d95803f42844b`,
not a stack of Android history:

| Branch | Signed head | Scope / evidence |
| --- | --- | --- |
| `agent/upstream-wallet-secret-lifetime-20261003` | `58619c85baf74c4a8a1c2c49573a89396291b9e5` | Mnemonic decoded-index retirement; focused RED/GREEN, Clang/GCC wipe-removal mutations and ASan/UBSan/LSan; all215 lint gates PASS; PR security scan PASS.4files,+116/-8. |
| `agent/upstream-wallet-key-scratch-20261003` | `c9eb0270abfaf787703f3fb794a3a0f709c61f27` | Master digest/HMAC-context and signing entropy retirement; focused RED/GREEN, six compiler/mutant failures, Clang/GCC ASan/UBSan/LSan; all215 lint gates PASS; PR security scan PASS.4files,+107/-9. |
| `agent/upstream-wallet-base58-span-20261003` | `987d226c3cc937c28ad6a2a0f374782332749153` | Bounded significant-digit conversion; independent16,788-case corpus, Clang/GCC sanitizers and mutations,268,723 fuzz runs, measured speedup; all215 lint gates PASS; PR scanner PASS.5files,+122/-22. |
| `agent/upstream-wallet-storage-descriptors-20261003` | `b25e546bab0658f9d283fb487674f592d1dd15e7` | Test-only six-opener CLOEXEC/close contract; mutation RED then restored GREEN, both compiler sanitizers, backup groups2/2; all215 lint gates PASS; PR scanner PASS.2files,+43/-1. |

Prepared descriptions are ignored local files `pr-mnemonic.md`, `pr-key.md`,
`pr-base58.md` and `pr-storage.md`
under publication `.cache/wallet-publication/`. They include WHY/WHAT/REUSE/SAFETY/
EVIDENCE/PORTABILITY/SCOPE/COMMITS. No new third-party dependency or consensus edit.

The normal fork-branch push of the first signed candidate was refused by
`tools/dev/z23_git_hook.c:644`: `remote-ref-not-main`. The hook admits only
`refs/heads/main`, while this mission forbids main pushes and check bypasses.
No hook or policy was changed and no alternate transport bypass was attempted.
An explicit user decision on a per-command exception for validated fork branches
is pending. No PR exists yet; CI and reviewer requests therefore have not run.
Do not treat local security scans or validated commits as submitted PRs.

All four selected adaptations are prepared. Their parent is the same fetched
upstream main; none depends on another candidate. Android dirty storage-limit
work remains preserved and outside every publication candidate.

### Final mapping of adaptable work

- Secret wiping and deterministic recovery: mnemonic-index candidate and key-
  scratch candidate. Upstream `domain_wallet_derive_path` already clears both
  current and next derived private-key/chain-code structures on failure and
  clears intermediates on success; do not duplicate that completed behavior.
- Signing-context ownership stays unchanged. Per-call Android teardown and JNI
  consuming-entropy handoffs remain C/reference because the node has different
  caller ownership and context lifetime.
- Storage descriptors: test-only candidate covers all six private-file openers,
  observed CLOEXEC flags, kernel close results and retired wrapper state. This
  does not claim execution of Android's post-exec observer in upstream.
- Base58: measured significant-span candidate ports both source optimizations,
  preserves full scratch cleansing and all admitted/output behavior, and adds
  an independent reference corpus. No PBKDF2/HMAC core rewrite is proposed.
- Fuzz/mutation/sanitizer/compiler evidence: adapted per candidate and rerun on
  the actual port. Do not import the Android CMake registry, generated fixtures,
  APK runtime infrastructure, or its long progress documents into upstream.
- No item is classified A merely because it compiles as C. These selected B
  adaptations use existing upstream owners; the remaining C/D rows name the
  missing consumer, different policy/format, completed upstream invariant, or
  sealed authority that makes wholesale transfer inappropriate.
