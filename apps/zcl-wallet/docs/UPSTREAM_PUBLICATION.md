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
commit dates are nonmonotonic. It finds53 relevant Android-path commits since
2026-09-26 plus three native-owner commits recorded in the expanded audit below.
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
| Base58 bounded conversion (`e805c8a06`, `32e58fad9`, `89926c0bd`, `a16546e79`) | A/B | The selected prefix candidate ports the existing native encoder/decoder optimizations with current-base tests, measured speedup, reference comparisons and unchanged cleanup. |
| Native xprv scratch retirement (`01e045fc9`) | A/B | Port existing serializer copy elimination and decoder refusal cleanup into the existing HD-key owner; adapt regression registration and required inventory to current main. |
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
request status here. Five independent upstream candidates are signed and have
focused validation; exact publication-proof status is separate below. Publication
also remains subject to the explicit hook conflict described below.

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
| `agent/upstream-wallet-base58-prefix-20261003` | `be78a2bf9f530f08d57e6bb1a2be62d83c9c8f75` | Existing native consumed-prefix encoder and active-byte decoder;16,788 reference cases, compiler sanitizers and mutations,168,047 fuzz runs, measured speedup. Exact native proof PASS:35/35 selected groups, all215 lint gates, no skips.4files,+211/-37. |
| `agent/upstream-wallet-storage-descriptors-20261003` | `b25e546bab0658f9d283fb487674f592d1dd15e7` | Test-only six-opener CLOEXEC/close contract; mutation RED then restored GREEN, both compiler sanitizers, backup groups2/2; all215 lint gates PASS; PR scanner PASS.2files,+43/-1. |
| `agent/upstream-wallet-xprv-retirement-20261003` | `d95e540be3f67352a0bbc602ff57f0537075157a` | Existing native scratch-retirement port; old functions RED5, port GREEN0, zero-length-wipe mutants RED7 on both compilers; fast/ASan, both compiler lanes and all215 lint gates PASS. Exact native proof pending.6files,+209/-67. |

Prepared descriptions are ignored local files `pr-mnemonic.md`, `pr-key.md`,
and `pr-storage.md` in the first publication checkout; `pr-base58-prefix.md` and
`pr-xprv.md` are in the second. Each uses `.cache/wallet-publication/`.
They include WHY/WHAT/REUSE/SAFETY/
EVIDENCE/PORTABILITY/SCOPE/COMMITS. No new third-party dependency or consensus edit.

The normal fork-branch push of the first signed candidate was refused by
`tools/dev/z23_git_hook.c:644`: `remote-ref-not-main`. The hook admits only
`refs/heads/main`, while this mission forbids main pushes and check bypasses.
No hook or policy was changed and no alternate transport bypass was attempted.
An explicit user decision on a per-command exception for validated fork branches
is pending. No PR exists yet; CI and reviewer requests therefore have not run.
Do not treat local security scans or validated commits as submitted PRs.

All five selected adaptations are prepared. Their parent is the same fetched
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

## Expanded owner audit and exact-proof qualification

The first53-entry date filter covered Android application paths. A subsequent
all-ref audit of the existing upstream wallet/encoding owners found three further
relevant development commits: `89926c0bdb2e5c98fb25431c07d67442fe11584f`,
`a16546e797bd4c1a75df27f03e05ee89d8e852b1`, and
`01e045fc9e14c58c2688df3a93cf48096e103266`. These are existing native Z23 work,
not new Android research. Their unchanged production algorithms are directly
reusable (A), with current-base test/inventory integration (B).

- Supersede the unsubmitted Base58 candidate `987d226c3` with signed
  `be78a2bf9f530f08d57e6bb1a2be62d83c9c8f75`, branch
  `agent/upstream-wallet-base58-prefix-20261003`. It ports both existing native
  commits. The encoder uses consumed-input bounds instead of adding a
  value-dependent inner-loop bound. Original checkpoint remains preserved.
  Four files,+211/-37. Canonical fast/ASan groups, both compiler sanitizer
  controls, four runtime mutations,16,788 reference cases,168,047 fuzz runs,
  repeated benchmarks and structural/compiler gates pass. Native publication
  proof remains pending; do not transfer the superseded candidate's full-lint
  result to this different source.
- Add signed `d95e540be3f67352a0bbc602ff57f0537075157a`, branch
  `agent/upstream-wallet-xprv-retirement-20261003`, ported from `01e045fc9`.
  Six files,+209/-67. Both compilers reproduce five failures in the original
  functions; ported controls return0, zero-length-wipe mutants return7. Canonical
  HD-key fast/ASan and structural/compiler gates pass. Native publication proof
  remains pending. The redundant serializer copy and three decoder refusal
  leaks are independent of the master-key scratch candidate.

The selected publication set is now five slices: mnemonic, master-key scratch,
Base58 prefix conversion, storage descriptor tests, and xprv retirement.
The additional ports and descriptions live in sibling checkout
`z23-wallet-reuse-publication-20261003`; its ignored evidence uses the same
`.cache/wallet-publication/` convention.

An exact native `dev proof step` is a separate publication requirement from
focused tests and lint. The storage candidate `b25e546ba` now has a verified
PASS receipt against base `3a93e60e`: all215 lint gates and20/20 impacted test
groups, no skips. The mnemonic candidate's root execution passed lint but failed
17/1236 runtime groups. This is NOT a passing proof. Diagnostics establish an
identity mismatch for multiple tests: chainlog requires `geteuid()!=0`, and
verifier custody refuses signer UID0 even in its test fixture. DAC-capability
removal qualifies permission-negative lint probes but does not make UID0 an
unprivileged runtime identity.

The existing `worldstreamproof` service identity was verified as UID1000. A fresh
isolated copy contains the five exact candidate refs; its location is recorded
locally in publication `unprivileged-proof-root.txt`. Its setup is complete, and
canonical chainlog/verifier-store controls are running before broader native
proof retries. No root-directory permissions, test assertions, policy files or
publication hooks were weakened. Remaining failures must be diagnosed from that
qualified environment; no claim that every root failure has already been resolved.
The explicit fork-hook decision is still pending; no PR has been opened.

### Qualified proof progress

All five selected candidates now have passing full215-gate lint results. The
new xprv port passed in467.047s. Base58 prefix `be78a2bf9` additionally has its
exact native PASS receipt:35/35 selected groups, zero skips/unobserved results,
all215 lint gates (230.329s), total480.868s. Native status independently verified
both this receipt and the earlier storage20/20 receipt.

Non-root qualification requires more than changing UID. The existing service
identity's chainlog and verifier-store controls passed. The proof runner also
requires a writable delegated memory scope; an existing user-manager scope
provides it with the scheduler's CPU/memory limits retained. A private mount
namespace supplies writable temporary storage without changing host permissions.
The SQLite source was checked against its pinned hash and the required docs-proof
tools were built before proof admission. No gate was disabled.

The first full UID1000 mnemonic attempt passed all215 lint gates and1234/1236
runtime groups. Its two failures were missing reflex-runner fixture images in
the isolated generation and a109-byte socket path exceeding the108-byte limit.
Both focused groups pass on unchanged upstream main through a short isolated
path. The short-path mnemonic retry has the reflex fixture present and lint
passing; its terminal runtime result is still pending. This is not yet a PASS
receipt. The xprv UID0 attempt was stopped after its planner widened to the
universal suite, whose non-root requirement was already established.

Independent non-root key-scratch and xprv copies are prepared for exact proofs.
Local locations are recorded in `key-proof-root.txt` in the first publication
checkout and `xprv-proof-root.txt` in the second. The first checkout's ignored
`run-proof-scope.sh` records the bounded namespace/user-manager invocation.
Initialize the pinned Tor submodule before copying verified archives into a
fresh clone; the source-identity guard correctly refuses a populated gitlink
without its Git identity. This setup refusal was corrected without source edits.

No branch has been pushed through a hook exception, and no PR, CI run or reviewer
request exists. The only open publication authority decision remains the
main-only push hook versus this mission's fork-branch-only requirement.

### Remaining integration findings

The namespace-only short-path attempt finished with four gateway-shard failures:
detached user-manager workers could not use that private pathname. All three
copies now use short, actual host paths, restored at the same path inside each
private mount namespace. The gateway, broker-socket and reflex controls pass in
each. The ignored root-locator files and runner helper were updated accordingly.

Fresh native generations still omit `build/fixtures/reflex_runner` after their
prefork and test-needs stages. The required image exists in each submitting
checkout and its canonical reflex group passes. The first full non-root attempt
already reproduced the missing-image failure; later attempts were stopped after
the same missing-input preflight, without repeating the whole suite. This is an
open proof-preparation limitation, not a wallet regression or a PASS receipt.

Canonical strict `test-parallel` also refuses two files unchanged from upstream:
`test_addrman_integrity.c:191` ignores a checked `truncate` result, and
`test_agent_posture_trylock.c:214` contains a nested comment. GCC's existing
`-Werror` rejects both. No warning policy or unrelated source was changed.

Additional full runtime evidence uses canonical `t-fast-exact`, the complete
exact ID list from `make t-list`, and `--no-cache --activate-proof-contracts`.
This is explicitly fast-profile evidence; it cannot be promoted to a strict
build or native receipt. Serialize these full suites under the shared test UID:
gateway fixtures use fixed task names, and detached systemd unit names derive
from the task name, attempt and queue sequence. The overlapping key/xprv runs
were stopped; no detached gateway fixture units remained. The mnemonic full
run completed:1241 groups,1 failed,5 self-skips,631.4s. Its only failing group was
`test_semantic_facts_fuzz`: the fallback Clang18 compiler rejected generated
C23 `constexpr` inputs. The existing `test_group_host_needs.def` explicitly
declares that unavailable capability and the native universal selector gates it.
The five self-skips name heavy/live fixtures; no production fixture was supplied
to force them to run. This is not an all-green suite result. The queued key/xprv
runs did not start after that failure; repeating the same unavailable toolchain
would add no evidence. All owned validation jobs are now stopped or complete.

Current publication state: two exact native PASS receipts (storage and Base58),
three focused-green candidates with the integration limitations above, and no
publication push or PR. All five signed candidate heads remain unchanged and
all five have passing215-gate lint plus their recorded compiler/sanitizer/mutation
evidence. Resolve the fork-only hook exception before publication; it must not be
interpreted as permission to waive the remaining validation limitations.

### Fork publication authorized and first two PRs submitted

The user authorized a command-local exception solely to the local hook's
main-only routing rule. The ignored adapter allowlists exact fork/branch/head
pairs, refuses main/master and existing remote branches, verifies commit
signatures and current upstream identity, and calls the unchanged native hook
on the exact candidate/upstream proof pair. Native signed-receipt, ancestry and
child-receipt validation remain active. No tracked hook, GitHub permission,
remote protection, sealed-core rule or acceptance gate changed.

Both PRs target `z23c/z23:main` at
`3a93e60ebf922af3d119b9facc1d95803f42844b`, with maintainer modification enabled:

- [PR73](https://github.com/z23c/z23/pull/73): branch
  `agent/upstream-wallet-storage-descriptors-20261003`, local and remote head
  `b25e546bab0658f9d283fb487674f592d1dd15e7`. Two files,43 additions/1 deletion:
  `test_wallet_backup.c` and generated capability inventory. Native proof20/20
  groups and215 lint gates PASS; focused Clang/GCC sanitizer and flag-removal
  mutation evidence is in the PR description.
- [PR74](https://github.com/z23c/z23/pull/74): branch
  `agent/upstream-wallet-base58-prefix-20261003`, local and remote head
  `be78a2bf9f530f08d57e6bb1a2be62d83c9c8f75`. Four files,211 additions/37
  deletions: Base58 codec, its regression, generated inventory and removal of
  the obsolete complexity-baseline row. Native proof35/35 groups and215 lint
  gates PASS, with focused dual-compiler sanitizer/mutation, fuzz and measured
  performance evidence in the PR description.

Existing PRs were checked before creation. At submission both security-review
CI jobs were queued, not passing. GitHub returned HTTP404 to one formal
`RhettCreighton` reviewer request per PR; neither review was successfully
requested. The verified identity was not repeatedly tagged; both descriptions
state they are prepared for maintainer review. The three secret-retirement
candidates remain local pending the documented integration prerequisites.

### Missing proof prerequisites repaired; combined wallet heads pending proof

The reproducible missing-reflex-image limitation has a narrow source fix:
`e926b75dfc6bd173969c99869f9570683f5d9945` on
`agent/upstream-wallet-proof-fixtures-20261003`. Existing BUILD needs now declare
15 Linux fixtures plus2 x86_64 pure fixtures. The unchanged proof lifecycle
builds and hashes them from its own source. A shared declaration-derived bound
also replaces the local runner's insufficient16-entry buffer. No selection,
assertion, signature or admission threshold is weakened.

RED: canonical impact-composition regression fails because zero reflex helpers
are declared; the initial declaration-only fix then reproduces the local
runner's capacity refusal. GREEN: both impact-composition and real reflex
runner groups pass,2/2 with zero skips (83.0s). Strict Clang20/GCC14 ASan/UBSan/
LSan probes pass; deleting one declaration makes both fail (expected17,
observed16). GCC checks2,433 translation units with changed paths diagnostic-free.
All215 lint gates pass (143.206s), including Clang, complexity, architecture and
doc-counts; the diff security scanner is CLEAN. Six files,+97/-17, including
one regenerated inventory row. Native exact proof is running under the already
qualified non-root identity; its generation now builds the missing fixtures.
A receipt PASS has not yet been established for this new source.

Each remaining wallet branch retains its original signed fix and adds the
prerequisite in a separate signed commit; production wallet code is unchanged:

- Mnemonic: `994070f632e382123f14464a7c39e50bcbd6d408` (original58619c85).
- Key scratch: `951f4cefb80c0acec81b329c1872ad3648c0ff1c` (originalc9eb0270).
- Xprv: `57f7df67d3ed18aa5bcb6a7d8eae1611b83af7b5` (originald95e540b).

The isolated copies retain their existing root-locator files. The mnemonic copy
currently runs the prerequisite-only proof; xprv/key copies carry their combined
heads. Serialize universal runtime proofs under the shared test identity;
independent builds in separate copies may run in parallel. All three combined
wallet proofs remain pending. No extra branch or PR has been published yet.

Prerequisite-only native result:195/196 groups pass; the sole failure is the
previously documented Clang18 semantic-fuzz capability (402.2s runtime). Reflex
execution passes with its source-built images. The exact plan deliberately does
not apply universal host gating, so no standalone PASS or publication is claimed.
Do not repeat that unsupported run or change selection rules to obtain green.
The original wallet additions already select the universal plan; their combined
heads must qualify under that unchanged plan and report its explicit host gates.
Xprv57f7df67 is now running its native proof; mnemonic994070f6 and key951f4cef
have rebuilt their development binaries and await serialized runtime proofs.
The shared prerequisite remains a separate commit in each proposed wallet PR.

### Xprv exact qualification complete

Candidate `57f7df67d3ed18aa5bcb6a7d8eae1611b83af7b5`, upstream base
`3a93e60ebf922af3d119b9facc1d95803f42844b`: native `dev proof step` PASS,
independently verified with `dev proof status` on the same exact pair. Receipt
SHA256: `8e5f7dff2eb1e7be359c284be9180d97d61ac6d88bf0e635e9855d50e44f41f9`.
The xprv root locator identifies the qualified copy; its receipt filename is
`57f7df67d3ed18aa5bcb6a7d8eae1611b83af7b5-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`.

All215 lint gates pass (188.475s lint umbrella;198.428s native lint step).
Runtime1236/1236 selected groups pass (642.746s), zero final skips/unobserved,
with one explicitly recorded load-flaky Tor bootstrap observation: its initial
network window was unobserved and the canonical isolated retry passed. Total
native proof860.404s. Universal selection reports the unchanged heavy-anchor,
live-onion-pair and bound-C23-toolchain host exclusions; no policy was edited.

Default compiler is GCC14.2.0; secondary Clang20.1.2. Final combined GCC scope
checks2,434 translation units with all11 changed paths diagnostic-free. Original
xprv Clang/GCC ASan/UBSan/LSan RED5→GREEN0 and cleanup-length mutant7 evidence
still applies to the byte-unchanged wallet production/regression code. The final
diff security scan is CLEAN. The final two-commit patch series and exact signed
commit bundle have SHA256 manifests in the ignored publication artifacts folder.

Mnemonic994070f6 now runs its exact proof; key951f4cef follows. Their final
combined GCC sweeps also pass (2,433 translation units each). Preserve each
source tree during proof; do not repeat already-green runs without a new reason.

The latest user instruction prohibits hook bypass. Standard pre-push still
refuses every non-main ref at `tools/dev/z23_git_hook.c:644`; the documented
normal loop supplies no fork route. No further publication push was attempted
under that instruction. PR73/74 remain published from the earlier explicitly
authorized routing exception. Preserve qualified candidates and complete PR
artifacts while continuing independent qualification and reuse work.

### Mnemonic exact qualification complete

Candidate `994070f632e382123f14464a7c39e50bcbd6d408`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`: native `dev proof step` PASS,
independently verified by `dev proof status` for that exact pair. Receipt SHA256:
`6b64b2fde12950baae95ec72f69138ec80e860e9e8b15ed8db959cd5bc847651`.
The mnemonic root locator identifies the preserved qualified copy; receipt name:
`994070f632e382123f14464a7c39e50bcbd6d408-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`.

All215 lint gates pass (239.147s umbrella;248.792s native lint step), including
architecture, documentation counts, strict Clang and complexity. Runtime1236/1236
selected groups pass (696.805s body), zero failures, skips, unobserved or
load-flaky outcomes. The unchanged universal host gates still apply; this is not
execution evidence for excluded host capabilities. Total native proof976.467s.
Compiler GCC14.2.0; secondary Clang20.1.2. Final GCC sweep2,433 translation units,
all9 changed paths diagnostic-free. The prior actual-source Clang/GCC
ASan/UBSan/LSan control0 and remove-cleanse mutant2 evidence applies unchanged.

The two signed commits, bundle, patch series and PR draft remain prepared and
unpublished under the recorded main-only hook boundary. Key-scratch951f4cef now
runs its exact proof after a fresh fetch confirmed the same upstream base.
Do not mutate that proof tree or repeat the completed mnemonic/xprv proofs.

Additional read-only reuse review: the wallet-owned child-derivation digest is
already retired on all returns. Its remaining HMAC-context lifetime belongs to
sealed `core/math/src/hash.c`; no core edit or duplicate caller-side algorithm
was introduced. This is ownership evidence, not a demonstrated runtime defect.

### Additional reusable candidate: refused-keystore plaintext retirement

Class A/B: Android failure-retirement invariant applied to Z23's existing WKS1
owner. Branch `agent/upstream-wallet-keystore-refusal-wipe-20261003`, signed head
`e5a1d446c6b29da8d5544acecdabb88f38f17d99`, upstream
`3a93e60ebf922af3d119b9facc1d95803f42844b`. Five files,+61/-26: keystore C/header,
existing regression, generated inventory and removal of the old complexity pin.

RED: canonical keystore group fails exactly three output-retirement assertions
for wrong passphrase, altered ciphertext and altered tag. GREEN: final canonical
fast and ASan groups pass with zero skips. Output canaries and length sentinels
prove exact-span retirement; malformed-header/capacity refusal preserves output.
Optimized strict Clang20/GCC14 ASan/UBSan/LSan control0 and wipe-length mutant3
per compiler. Final GCC sweep2,433 translation units, all5 paths clean. All215
lint gates PASS (128.645s), complexity cap15 and diff security scan CLEAN.

Existing wallet_decrypt_blob frees on refusal without its own wipe, confirming
why this belongs in the lowest shared owner. API format, crypto and accepted
successes are unchanged. Hazard review is recorded in C_SAFETY_REVIEW.md.
The exact native proof and PR artifacts are being prepared independently of the
preserved mnemonic/key/xprv trees. No additional fork push has been attempted.

The follow-on bounds hypothesis now has isolated sanitizer RED: an overflowing
plaintext length passes the encryptor's envelope-capacity check even with zero
output capacity. No real wallet data was used. Keep this as a separate candidate
and establish a safe bounded regression before fixing its admission owner.

### Four priority candidates qualification-complete; key receipt verified

Key-scratch candidate `951f4cefb80c0acec81b329c1872ad3648c0ff1c`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`: native `dev proof step` PASS and
independent `dev proof status` PASS. Receipt SHA256:
`cd20309b5250777a187a63f88b91987d284728c5fab12be9e368c3fea2b8e194`.
The key root locator identifies the preserved qualified copy; receipt name:
`951f4cefb80c0acec81b329c1872ad3648c0ff1c-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`.

All215 lint gates PASS (210.441s umbrella;221.678s native lint step).
Runtime1236/1236 selected groups PASS (651.113s body), zero failures, skips,
unobserved or load-flaky outcomes. Total proof874.933s. Existing universal host
gates remain explicit; no excluded capability is claimed. GCC14.2.0 default,
Clang20.1.2 secondary. Final GCC sweep2,433 translation units, all9 paths clean.
Original optimized dual-compiler sanitizer controls0 and six digest/HMAC/entropy
mutants1 remain valid for the byte-unchanged wallet production/regression code.

Storage and Base58 are published as PR73/74. Mnemonic, key-scratch and the
additional xprv candidate have complete exact qualification and finalized PR
bodies; their signed bundles verify against the recorded base. They remain local
under the previously recorded normal-hook publication boundary. No routine
permission question or repeated refused push is needed. The new keystore
retirement candidate has its own isolated proof copy; all earlier source trees
and receipts remain preserved. Continue with its proof and separate range fix.

### Additional reusable candidate: keystore encryption length admission

Class A/B: bounded-admission invariant applied to the existing Z23 WKS1 owner.
Branch `agent/upstream-wallet-keystore-length-bounds-20261003`, signed head
`03bfa7e51773b2a0284845563d4e391d03167f1a`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`. Five files,+54/-11: C/header,
existing regression, generated inventory and obsolete encryptor complexity pin.
The envelope-size helper now refuses payloads outside EVP's representable range;
encryption refuses its zero sentinel before provider work or output writes.

Canonical RED at the new size assertion; final fast/ASan GREEN, zero skips.
Optimized strict Clang20/GCC14 ASan/UBSan/LSan bounded provider probes: RED5,
GREEN0, zero-sentinel mutants8 and integer-bound mutants3 each. A valid-call
control qualifies the provider fault wrapper. Zero/INT_MAX boundaries are
checked without claiming multi-gigabyte runtime encryption. Final GCC sweep2,433
translation units, all5 paths clean. All215 lint gates PASS (135.834s), complexity
cap15 and security diff CLEAN. Hazard review, PR body and signed patch/bundle
artifacts are prepared. Exact proof awaits its separate copy's build.

Keystore retirement e5a1d446 now runs its exact proof. The first copy preparation
refused unavailable alternate Git objects inside the private namespace, then an
unconfigured hooks path. Self-contained object repacking and canonical
`make install-hooks` repaired those prerequisites without changing source or
policy. Those setup refusals are not product failures or PASS receipts.
