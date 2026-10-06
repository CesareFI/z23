<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Wallet C upstream publication — 2026-10-03

Primary mission: port useful wallet safety work into the existing Z23 owners as
small independently validated PRs. Do not import this entire Android application.
This ledger stays on the wallet development branch; PRs carry concise provenance.

## Captured identities

- Upstream/origin: https://github.com/z23c/z23.git, main
  `3a93e60ebf922af3d119b9facc1d95803f42844b` (fetched under both remote-tracking names).
- Android branch: `agent/android-jni-secret-retirement-20260915`; latest
  validated C checkpoint before this ledger update is
  `daeba12ae6e8a3e2ba6a3b2ac9840e078e2d9342`, signed locally. The last
  exact backup-SHA verification predates the later local publication-evidence
  commits; no current remote-equality claim is made.
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
| Base58 bounded conversion and scratch retirement (`e805c8a06`, `32e58fad9`, `89926c0bd`, `a16546e79`, `03654b250`) | A/B | The selected prefix candidate ports the existing native encoder/decoder optimizations with current-base tests, measured speedup and reference comparisons. The newer portable-C retirement commit adds deterministic wiping for private-capable checksum, magnitude and staging scratch and requires a separate current-base adaptation/proof before publication. |
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
| Base58 JNI scratch retirement (`daeba12ae`) | C | Android/JNI ownership only. It is useful evidence for consumed-input-before-publication ordering but has no upstream JNI consumer; do not mix it into the portable Base58 C slice. |
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
| `agent/upstream-wallet-secret-lifetime-20261003` | `994070f632e382123f14464a7c39e50bcbd6d408` | Mnemonic decoded-index retirement plus the shared isolated-proof prerequisite; exact native proof PASS with all215 lint gates and all20 selected groups, zero skips. Sealed receipt and review artifacts are recorded below. |
| `agent/upstream-wallet-key-scratch-20261003` | `951f4cefb80c0acec81b329c1872ad3648c0ff1c` | Master digest/HMAC-context and signing entropy retirement plus the shared isolated-proof prerequisite; exact native proof PASS with all215 lint gates and all22 selected groups, zero skips. Sealed receipt and review artifacts are recorded below. |
| `agent/upstream-wallet-base58-prefix-20261003` | `be78a2bf9f530f08d57e6bb1a2be62d83c9c8f75` | Existing native consumed-prefix encoder and active-byte decoder;16,788 reference cases, compiler sanitizers and mutations,168,047 fuzz runs, measured speedup. Exact native proof PASS:35/35 selected groups, all215 lint gates, no skips.4files,+211/-37. |
| `agent/upstream-wallet-storage-descriptors-20261003` | `b25e546bab0658f9d283fb487674f592d1dd15e7` | Test-only six-opener CLOEXEC/close contract; mutation RED then restored GREEN, both compiler sanitizers, backup groups2/2; all215 lint gates PASS; PR scanner PASS.2files,+43/-1. |
| `agent/upstream-wallet-xprv-retirement-20261003` | `57f7df67d3ed18aa5bcb6a7d8eae1611b83af7b5` | Existing native scratch-retirement port plus the shared isolated-proof prerequisite; old functions RED5, port GREEN0 and zero-length-wipe mutants RED7 on both compilers. Exact native proof PASS with all215 lint gates and all1,236 selected groups, zero final skips or unobserved results. Sealed receipt and review artifacts are recorded below. |

Prepared descriptions are ignored local files `pr-mnemonic.md`, `pr-key.md`,
and `pr-storage.md` in the first publication checkout; `pr-base58-prefix.md` and
`pr-xprv.md` are in the second. Each uses `.cache/wallet-publication/`.
They include WHY/WHAT/REUSE/SAFETY/
EVIDENCE/PORTABILITY/SCOPE/COMMITS. No new third-party dependency or consensus edit.

The normal fork-branch push of the first signed candidate was refused by
`tools/dev/z23_git_hook.c:644`: `remote-ref-not-main`. The hook admits only
`refs/heads/main`, while this mission forbids main pushes and check bypasses.
No hook or policy was changed and no alternate transport bypass was attempted.
Later compliant publication evidence is recorded below. Under the current
no-external-action boundary, remaining local candidates stay sealed and no
remote-equality, CI or reviewer-request claim is made for them. Do not treat
local security scans or validated commits as submitted PRs.

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

### Keystore plaintext-retirement exact qualification complete

Head `e5a1d446c6b29da8d5544acecdabb88f38f17d99`, upstream
`3a93e60ebf922af3d119b9facc1d95803f42844b`: native proof PASS, independently
verified with exact-pair status. Receipt SHA256:
`561c0e246e402fdfdd7c9e458454a7f96e8f4749f8041b676c8e14fec8e13fe7`.
The keystore root locator identifies the preserved copy; receipt name:
`e5a1d446c6b29da8d5544acecdabb88f38f17d99-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`.

All215 lint gates PASS (182.734s umbrella;192.449s native step). Exact impacted
runtime228/228 PASS (293.961s), zero failures, skips, unobserved or flaky outcomes.
Total proof540.184s. GCC14.2.0 default / Clang20.1.2 secondary; prior focused
sanitizer/mutation and final compiler evidence applies to the same signed source.
No common proof-prerequisite commit is needed in this slice. PR body, patch series,
commit bundle and SHA256 manifests are complete; publication remains subject to
the previously recorded hook boundary. The separate length candidate03bfa7e5
now runs exact proof. Its source tree and all prior qualified trees are preserved.

Additional storage/BIP44 caller audit: inspected POSIX directory opens already
use CLOEXEC and close retained descriptors; private-file close invalidates its
handle. Identity-retirement callers retain explicit close ownership. Inspected
wallet/recovery keypair callers wipe private-key scratch on failure as well as
success. No additional demonstrated lifetime defect was found in these paths;
no speculative patch or repeated completed regression was added. PR73/74 CI
remains queued for an unassigned self-hosted scanner at the latest checkpoint.

### Keystore encryption-length exact qualification complete

Head `03bfa7e51773b2a0284845563d4e391d03167f1a`, upstream
`3a93e60ebf922af3d119b9facc1d95803f42844b`: native proof PASS and independent
exact-pair status PASS. Receipt SHA256:
`3e244e02a8da87e35d9b6511fde257ad8fce525fb0680a11bdb98fb132b2eaf2`.
The range root locator identifies the preserved copy; receipt name:
`03bfa7e51773b2a0284845563d4e391d03167f1a-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`.

All215 lint gates PASS (184.668s umbrella;194.958s native step). Exact impacted
runtime228/228 PASS (279.8s), zero failures, skips, unobserved or flaky outcomes.
Total proof528.026s. GCC14.2.0 default / Clang20.1.2 secondary. The focused
RED/GREEN, sanitizer, mutation, final compiler and hazard-review evidence applies
to the same signed bytes. PR body, patch series, bundle and SHA256 manifests are
complete. No publication push was attempted under the recorded hook boundary.

A separate bounded decrypt-length probe now has dual-compiler RED2: oversized
envelope lengths reach the qualified failing KDF wrapper instead of refusing at
the API boundary. No huge allocation, payload access or external target is used.
This is a new candidate on upstream main; keep it separate from the two qualified
keystore slices.

### Decrypt signed-length candidate GREEN and signed

The bounded decrypt finding is now a small signed follow-up on the existing
plaintext-retirement commit: branch
`agent/upstream-wallet-keystore-decrypt-safety-20261003`, head
`c78e1483e74061b6208063aec76e9ecde2e1782d`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`. The second commit changes four
files,+32/-2; the combined two-commit review changes five files,+92/-27.

Canonical RED shows two oversized lengths reach KDF work. GREEN refuses both
before KDF/output, while a valid small span and exact `INT_MAX` boundary still
reach the qualified KDF fault control once. Strict optimized Clang20/GCC14
ASan/UBSan/LSan RED2→GREEN0; removing the bound yields mutant2 each. Focused
fast/ASan pass with zero skips. Final GCC sweep2,433 translation units, all5
combined paths clean. All215 lint gates PASS (123.085s), complexity cap15 and
security scan CLEAN. The existing failed-auth plaintext retirement and its
exact receipt remain unchanged in the first signed commit.

The exact combined-head native proof independently reports PASS from preserved
copy `/tmp/wd.g8GgrV/repo`: all215 lint gates pass (219.793s native lint;
230.453s proof lint phase), exact impacted runtime228/228 passes (318.0s), and
there are zero failures, skips, unobserved outcomes or load-flaky outcomes.
Total proof time is564.421s. The signed receipt
`c78e1483e74061b6208063aec76e9ecde2e1782d-3a93e60ebf922af3d119b9facc1d95803f42844b.receipt`
has SHA256
`f8ea7015fb63d2c9d761f9ce7a5b808166f1801bab2f3f0544f38bb148ed918f`.
An independent status check confirms branch
`agent/wallet-keystore-decrypt-proof-20261004`, exact HEAD c78e1483 and no
tracked differences.

This combined decrypt-safety series supersedes the single-commit retirement
branch for eventual review, without deleting its branch, bundle, receipt or
qualified source tree. PR body and offline artifacts are complete. The
independently qualified encryption-length branch03bfa7e5 stays a separate
API-admission proposal. No publication push was attempted under the recorded
hook boundary.

### Extended-key refusal retirement candidates signed

Branch `agent/upstream-wallet-extkey-failure-retirement-20261004`, signed head
`72546d92ee4e1d92e20aaf5160f4c7cb65975be4`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, places failure cleanup in the
portable BIP32 owner. Failed child derivation and an invalid master scalar now
wipe the complete extended-key output. The registered actual-source observer is
canonical RED before the fix and GREEN afterward; distinct child/master
cleanup-removal mutants exit1 under strict optimized Clang20/GCC14
ASan/UBSan/LSan while controls exit0. Focused fast/ASan, cap15 complexity,
architecture, generated inventory, core seals, consensus parity,2,433-TU GCC
scope and sensitive-pattern scans pass. Its preserved exact proof copy
`/tmp/we.mspu1F/repo` passes all 215 lint gates and all 1,236 impacted groups
with zero skips or final unobserved outcomes. One onion-bootstrap group was
load-flaky under the shared pool and passed clean alone. Receipt SHA256 is
`9b9044d691bed0e29e0ae1981584457b4a1510793d1e79b77d052b17c6ec781d`.
Two earlier generation-prerequisite refusals are retained as environment
evidence and were repaired only with their documented build targets.

Signed follow-up `d796d73ae29f04cd06ecc16b7d500d5900e2aade` on branch
`agent/upstream-wallet-extkey-refusal-retirement-20261004` also wipes the
complete decoded extended-key output when secp256k1 rejects its scalar.
Canonical invalid-decode RED becomes GREEN; removing only that cleanup yields
mutant1 under both compilers. The test helpers were split after the complexity
gate rejected M=18; final functions pass the normal cap without a suppression.
The combined two-commit review is five files,+170/-11. Its preserved exact
proof passes all 215 lint gates and all 1,236 impacted groups with zero
failures, skips, unobserved or load-flaky outcomes in 937.773s. Receipt SHA256
is `2e30a6b682e51ea47b80904cad00cb6b1fb6dda0861dad935404aeb4532fa092`.
Its PR body, signed patch series, complete bundle and SHA256 manifest are
prepared. The single-commit branch/tree/artifacts stay preserved.

### Signing-key scratch completion candidate signed

Branch `agent/upstream-wallet-key-scratch-complete-20261004`, signed head
`c5daeca1c25334fd7dca37943378308b683ecf89`, base3a93e60e, extends the
qualified key-scratch series by retiring the generated private key used by the
startup ECC sanity check after its final verification use. The new production
commit changes only `key.c`, its actual-source retirement regression and the
generated capability inventory. Canonical RED becomes focused and ASan GREEN;
strict optimized Clang20/GCC14 sanitizer controls exit0 and removal of the new
cleanse exits1 under both compilers. GCC2,433-TU scope, cap15 complexity,
architecture, inventory, seals, consensus parity, no-Python and sensitive-data
gates pass. The independently preserved combined proof copy
`/tmp/wk.Bq1CX4/repo` passes all 215 lint gates and all 1,236 impacted groups
with zero failures, skips, unobserved or load-flaky outcomes in 822.162s.
Receipt SHA256 is
`6377828bf1c3e9ff2a43031aa71508da209ad2a96f2c08ec3fdbbf1b6e1358ba`.
Signed bundle, patch series and PR body are prepared.

### Exported WIF scratch-retirement candidate signed

Branch `agent/upstream-wallet-wif-retirement-20261004`, signed head
`ddb074dd743d195782d4308bd0b84967ab267998`, base3a93e60e, retires the
128-byte `dumpprivkey` WIF stack buffer immediately after `json_set_str` makes
its independently owned response copy. The returned WIF and key-export rules
are unchanged. A behavior-preserving helper extraction produces canonical RED;
the single cleanse makes the registered simnet dump/import/rescan/spend plus
backup/restore group GREEN. Its ASan/UBSan lane also passes with zero skips,
unobserved or load-flaky outcomes. All215 lint gates, GCC14 across2,433 TUs,
cap15 complexity, architecture, generated inventory, core seals, consensus
parity, no-Python and credential scans pass. Bundle, patch, PR draft and SHA256
manifest verify. Exact proof copy `/tmp/ww.q4fYTp/repo` passes all 215 lint
gates and all 38 impacted groups with zero failures, skips, unobserved or
load-flaky outcomes in 503.951s. Receipt SHA256 is
`217d26822b5f1a522eeb2771681cfdf35fb85731d8b9ba94b7d4a84ae18fe0fd`.

### Private-key HTTP response-copy retirement qualified

Branch `agent/upstream-wallet-rpc-secret-retirement-20261004`, signed head
`020f58a96c5cab877a7e6603735e1b8c03261476`, base3a93e60e, retires the
three native heap copies that carry an authorized `dumpprivkey` result: the
RPC result string, response-envelope copy and exact serialized HTTP allocation.
Retirement occurs only after the synchronous send returns and before JSON/free
cleanup. Ordinary RPC methods retain their prior lifetime and behavior.

Canonical RED fails only the new private-response retirement assertion while
its ordinary-response control passes. Final focused and ASan/UBSan groups pass
20/20 with zero skips or unobserved outcomes. All215 lint gates pass in
197.758s. Strict Clang20 and GCC14 each check2,433 translation units with no
new diagnostic sites. Complexity cap15, architecture, generated inventory,
core seals, consensus parity, no-Python and credential gates pass.

The preserved proof-owned copy `/tmp/wr.G71rRn/repo` independently passes the
exact commit/base proof: all215 lint gates and87/87 impacted groups, zero
failures or skips; lint203.869s, tests215.070s, total447.402s. Native status
re-verifies the exact pair. Receipt SHA256 is
`43663e435c472cd473567a47f7e99a5010c4c0f265964dea628fcb375720b455`.
Complete signed bundle, patch series, PR body and SHA256 manifest are prepared
locally. No external publication action or protection bypass was attempted.

### Sapling key-export response retirement qualified

Signed follow-up `1d3f8f26c05e5fb4e426fa4c9f13d4023e49e15a` on branch
`agent/upstream-wallet-shielded-rpc-secret-retirement-20261004` extends the
same post-send retirement boundary to `z_exportkey` spending-key results and
`z_exportviewingkey` viewing-key results. Canonical RED fails exactly those two
new assertions while transparent and ordinary-response controls pass. Final
focused and ASan/UBSan groups pass22/22 with zero skips, unobserved or
load-flaky outcomes. All215 lint gates pass in98.892s; strict Clang20/GCC14
each check2,433 TUs; complexity, architecture and generated inventory pass.

The preserved combined-head proof copy `/tmp/ws.GRk4hI/repo` passes all215
lint gates and87/87 impacted groups with zero failures or skips. Lint took
210.001s, tests211.971s and total proof455.607s. Native status independently
re-verifies head1d3f8f26/base3a93e60e. Receipt SHA256 is
`12ae9da0a8cdf6f36486443f079faf3422fd2e4b5855963fd9e9d6caae6a00a8`.
The two-commit combined review is five files,+125/-6 and supersedes the
unpublished transparent-only PR draft without deleting its branch, receipt or
artifacts. Complete bundle, patch series, updated PR body and SHA256 manifest
verify locally. No external publication action was attempted.

### Transparent and Sapling encoder-scratch retirement qualified

Branch `agent/upstream-wallet-shielded-export-scratch-20261004`, signed head
`07655a15e585efbf2d2782d0ec15924a72346ac9`, combines the qualified WIF
retirement parent with one focused Sapling follow-up. The shared controller
helper now copies only a successful encoding and always wipes the complete
caller-owned buffer. This retires transparent WIF scratch after success,
Sapling spending-key scratch after encoder refusal, and Sapling viewing-key
scratch after success or refusal. Returned bytes, encodings and refusal behavior
are unchanged. Combined review against base
`3a93e60ebf922af3d119b9facc1d95803f42844b`: five files,+193/-14.

Canonical Sapling RED fails the viewing-key success and refusal retirement
assertions while the real wallet and backup cases remain green. Final focused
and ASan/UBSan runs observe WIF success, viewing-key success/refusal and
spending-key refusal retirement; the registered group passes with zero skips,
unobserved results or load flakiness. Strict GCC14 and Clang20 each accept all
2,433 translation units with no new diagnostic sites. All215 focused lint
gates pass in99.166s; cap15 complexity scans68,763 functions and architecture
and generated-inventory checks pass.

The preserved unprivileged proof copy `/tmp/wx.zOnj60/repo` independently
passes the exact head/base proof: all215 lint gates in183.050s and114/114
impacted groups in260.879s, with zero failures, skips, unobserved results or
load flakiness. Total proof time is533.211s. Native status verifies the exact
pair. Receipt SHA256 is
`00c1ae9ac34857e1975a86e967497f1fb86b1213776230c0700e4da5b4ea3803`.
The first two proof attempts refused absent local ignored prerequisites
(`vendor/sqlite3.c` and `build/bin/gen_capability_inventory`); both were
restored from or built inside the local proof checkout, then the documented
retry path ran the complete proof. No gate, hook or policy was bypassed.

Local review artifacts `key-export-scratch.bundle`,
`key-export-scratch.mbox`, `key-export-scratch.receipt`,
`pr-key-export-scratch.md` and `key-export-scratch-final.sha256` verify. The
combined candidate supersedes the unpublished WIF-only draft for review while
preserving its branch, proof and artifacts. No external publication action was
attempted in this slice.

### Native command key-export copy retirement qualified

Branch `agent/upstream-wallet-native-command-secret-retirement-20261004`,
signed head `8027ce839681e1864d7ddd43d08bc1559fb3f903`, extends the qualified
transparent and Sapling HTTP response-retirement series through the native
command boundary. The key-export command declares an explicit secret-output
trait; the registry, serializer, field-selection, prose and final CLI owners
wipe their mutable copies after last use. The successful caller-owned output
remains unchanged and under caller retirement ownership. Combined review from
base `3a93e60ebf922af3d119b9facc1d95803f42844b`: 24 files,+558/-255,
including generated registry and capability inventory updates.

Deterministic RED independently catches the absent recursive JSON cleanser and
absent key-export trait. Final JSON/JSONQ, native API and 22-case RPC response
groups pass; their registered ASan/UBSan lanes pass with zero skips. Strict
GCC14 and Clang20 each accept all 2,433 translation units with no new diagnostic
sites. Focused lint passes all 215 gates, including complexity, architecture,
generated inventory, Windows cross-link, seals and consensus parity.

The preserved unprivileged proof copy `/tmp/wn.mmgOK7/repo` passes the exact
head/base proof: all 215 lint gates in 213.857s and all 1,236 selected runtime
groups in 624.112s, with zero failures, skips, unobserved or load-flaky
outcomes. The foreground step returned after 803.221s. Native status
independently re-verifies the exact pair. Receipt SHA256 is
`8ab232ae3a2296c021a02bc526629bde98f5db370bb5a895457af0f1c9fa3a73`.

All three commit signatures verify. Local artifacts
`native-command-secret.bundle`, `native-command-secret.mbox`,
`native-command-secret.receipt`, `pr-native-command-secret-retirement.md` and
`native-command-secret-final.sha256` verify. This cumulative candidate
supersedes the unpublished HTTP-only response-copy draft while preserving its
branch, receipt and artifacts. No external publication action was attempted.

### Standalone JSON parse-failure retirement qualified

The cumulative command-boundary follow-up
`5fd85ade93a7994b7ec3e18e36e4d6cf746145fa` first demonstrated that the
shared JSON reader freed partial decoded strings, keys and nested trees without
retiring their initialized bytes after malformed input or allocation failure.
Its preserved unprivileged proof against base
`3a93e60ebf922af3d119b9facc1d95803f42844b` passed all 215 lint gates and
all 1,236 selected runtime groups with zero failures, skips, unobserved or
load-flaky outcomes. Lint took 199.739s, tests 611.365s and the foreground step
791.400s. Receipt SHA256 is
`ef3f403bec61a9a4a30e3dd999e7567eacbce1221d877a31798953eaba2ccf1f`.
That cumulative head spans four commits and 27 files, so it is retained as
proof evidence rather than proposed as a small independent review.

Branch `agent/upstream-json-failure-retirement-standalone-20261004`, signed
head `dfed177a602d3c0ccb5bb780f9870ac3f2346909`, isolates the invariant
directly on the same upstream base. It uses a private failure-cleanup helper in
the existing JSON owner instead of depending on the unpublished cumulative
JSON API. Failure cleanup wipes the initialized partial-string span, completed
keys including their terminators, and owned strings/keys in partial child
trees before ordinary free. Successful parse ownership and the allocation-free
`json_valid` path are unchanged. The standalone review is 10 files,+260/-35;
most non-parser changes are direct-link dependency lists and generated package
roots/inventory.

The prior production source is deterministic RED: the actual-source observer
compiles and exits 1 with `cleanse=0 marked=0 zeroed=0`. Removing final root
cleanup from the candidate produces the same mutation RED. Canonical JSON
fast and ASan lanes pass 2/2 groups each. Strict optimized GCC14 and Clang20
ASan/UBSan/LSan controls pass injected string, child-array and key-array
allocation failures. Package registry 1/1, swarm-network 5/5 executed shards
and score-receipt 7/7 pass with zero skips. Both compiler lanes accept all
2,433 translation units with no new diagnostics, and the exact candidate tree
passes all 215 lint gates, including cap15 complexity, architecture, generated
inventory, Windows cross-link, sealed-core and consensus-parity checks.

The retained proof copy `/tmp/wn.mmgOK7/repo` independently passes the exact
head/base pair: all 215 lint gates in 200.691s and all 1,236 selected runtime
groups in 617.224s, with zero failures, skips, unobserved or load-flaky
outcomes. Total foreground time is 798.180s and native status re-verifies the
fresh receipt. Receipt SHA256 is
`0d9d1246626e56555ca864523208d82752375f26dc9c18da87a0ea93cd53a945`.

Local review artifacts `json-failure-standalone.bundle`,
`json-failure-standalone.mbox`, `json-failure-standalone.receipt`,
`pr-json-failure-retirement.md` and
`json-failure-standalone-final.sha256` verify. The cumulative branch and all
prior artifacts remain preserved. No external publication, hook bypass or
protection change was attempted.

### Four-candidate review artifacts sealed

The originally requested mnemonic, key-scratch, Base58 active-span and storage
descriptor candidates now each have a candidate-specific thin bundle anchored
to upstream `3a93e60ebf922af3d119b9facc1d95803f42844b`, a signed patch series, the
sealed exact-proof receipt, a maintainer-facing PR body and a SHA256 manifest.
Every commit signature and bundle prerequisite was re-verified; the manifests
verify all four review artifacts. No source or proof result changed and no
completed expensive test was repeated.

- Mnemonic head `994070f632e382123f14464a7c39e50bcbd6d408`:
  `mnemonic-qualified.bundle`, `mnemonic-qualified.mbox`,
  `mnemonic-qualified.receipt`, `pr-mnemonic-qualified.md`, and
  `mnemonic-qualified-final.sha256`. Receipt SHA256
  `6b64b2fde12950baae95ec72f69138ec80e860e9e8b15ed8db959cd5bc847651`.
- Key-scratch head `951f4cefb80c0acec81b329c1872ad3648c0ff1c`:
  `key-qualified.bundle`, `key-qualified.mbox`, `key-qualified.receipt`,
  `pr-key-qualified.md`, and `key-qualified-final.sha256`. Receipt SHA256
  `cd20309b5250777a187a63f88b91987d284728c5fab12be9e368c3fea2b8e194`.
- Base58 head `be78a2bf9f530f08d57e6bb1a2be62d83c9c8f75`:
  `base58-prefix.bundle`, `base58-prefix.mbox`, `base58-prefix.receipt`,
  `pr-base58-prefix.md`, and `base58-prefix-final.sha256`. Receipt SHA256
  `7dfc04f8aa509e98ee19f907543632e2a2089974b2e8e001a1109a2291c10f46`.
- Storage descriptor head `b25e546bab0658f9d283fb487674f592d1dd15e7`:
  `storage-descriptors.bundle`, `storage-descriptors.mbox`,
  `storage-descriptors.receipt`, `pr-storage.md`, and
  `storage-descriptors-final.sha256`. Receipt SHA256
  `1e66459c1ed70e3b3c78a0f50a36db7c7c238cbf82ab32308dfee77830ac9a21`.

Mnemonic, key-scratch and Base58 artifacts are under the ignored evidence
folder in `z23-wallet-reuse-publication-20261003`; storage artifacts are in the
corresponding folder in `z23-wallet-publication-20261003`. PR73/74 remain the
already-published storage/Base58 submissions recorded above. The two secret-
retirement candidates remain locally prepared under the current no-external-
action boundary.

### Additional qualified review artifacts sealed

Five more already-qualified wallet C proposals now have manifests covering the
review bundle, patch series, exact-proof receipt and final PR body. Existing
bundles and proof evidence were preserved; only missing artifact copies and
manifests were added. Every included commit signature, bundle prerequisite and
receipt hash was re-verified. No source changed and no completed proof or test
was repeated.

- Xprv scratch retirement head `57f7df67d3ed18aa5bcb6a7d8eae1611b83af7b5`:
  `xprv-qualified-final.sha256`; receipt SHA256
  `8e5f7dff2eb1e7be359c284be9180d97d61ac6d88bf0e635e9855d50e44f41f9`.
- Keystore failed-auth retirement head
  `e5a1d446c6b29da8d5544acecdabb88f38f17d99`:
  `keystore-wipe-final.sha256`; receipt SHA256
  `561c0e246e402fdfdd7c9e458454a7f96e8f4749f8041b676c8e14fec8e13fe7`.
- Keystore encryption length admission head
  `03bfa7e51773b2a0284845563d4e391d03167f1a`:
  `keystore-range-final.sha256`; receipt SHA256
  `3e244e02a8da87e35d9b6511fde257ad8fce525fb0680a11bdb98fb132b2eaf2`.
- Combined keystore decrypt safety head
  `c78e1483e74061b6208063aec76e9ecde2e1782d`:
  `artifacts/keystore-decrypt-safety-final.sha256`; receipt SHA256
  `f8ea7015fb63d2c9d761f9ce7a5b808166f1801bab2f3f0544f38bb148ed918f`.
- Combined extended-key failure/refusal retirement head
  `d796d73ae29f04cd06ecc16b7d500d5900e2aade`:
  `extkey-refusal-retirement-final.sha256`; receipt SHA256
  `2e30a6b682e51ea47b80904cad00cb6b1fb6dda0861dad935404aeb4532fa092`.

The xprv artifacts reside in `z23-wallet-reuse-publication-20261003`; the
keystore and extended-key artifacts reside in
`z23-wallet-publication-20261003`. All remain local under the current
no-external-action boundary.

### Keystore reserved-field admission qualified

Branch `agent/upstream-wallet-keystore-reserved-field-20261004`, signed head
`53bd0693c5d85cbb6e93bfa1ff46cef42a1b647b`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, enforces the documented WKS1
requirement that reserved header bytes 12..15 are zero. Decryption refuses a
nonzero value before KDF/provider work or caller-output mutation, and the
header-inspection helper reports the envelope as malformed. Valid version-1
envelopes and cryptographic operations are unchanged. The review is three
files, +36/-5.

Canonical RED shows that the prior implementation decrypted a nonzero-reserved
envelope. Removing only the new reserved-field comparison reproduces mutation
RED; the restored focused and ASan/UBSan groups pass with zero skips. Strict
GCC14 and Clang20 each check all 2,433 translation units with no new diagnostic
sites. MinGW checks 2,399 translation units with no new failures after the
isolated worktree's missing generated zlib headers are restored from the local
validated dependency tree. Architecture, generated inventory, documentation,
consensus-parity and complexity gates pass; the helper score is 2 and the
pre-existing decrypt score remains 22.

The preserved unprivileged proof copy `/tmp/wn.mmgOK7/repo` passes the exact
head/base pair through the qualified private-tmpfs/user-manager scope: all 215
lint gates in 192.336s and all 99 selected impacted groups in 279.693s, with
zero failures, skips, unobserved results, load-flaky outcomes or cached groups.
Total foreground time is 469.031s. Native status independently re-verifies the
pair. Receipt SHA256 is
`baceec198bf831b018ff182fd2be0c4e8da36edc5568d469495f212f9fe4a513`.
The optional coverage manifest is absent, so no coverage claim is made; the
policy-5 exact receipt remains valid.

Local artifacts `keystore-reserved-field.bundle`,
`keystore-reserved-field.mbox`, `keystore-reserved-field.receipt`,
`pr-keystore-reserved-field.md`, `keystore-reserved-exact-status.log` and
`keystore-reserved-field-final.sha256` verify in the candidate worktree's
ignored evidence directory. An earlier proof attempt is retained as environment
RED: the host's global `/tmp` mode 0755 made unrelated unprivileged fixtures
fail. The successful retry used the already-qualified private tmpfs wrapper;
global permissions, gates and repository policy were unchanged. No external
publication action was attempted.

### Keystore KDF-iteration parsing qualified

Branch `agent/upstream-wallet-kdf-iteration-parse-20261004`, signed head
`3000cb9ca6280d3dcf8f6fb1429b587b03b1d2b3`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, makes the platform-neutral C23
wallet-keystore parser require complete consumption of
`ZCL_WALLET_KDF_ITERS`. The prior parser admitted `10000oops` as 10,000
iterations; malformed text now retains the existing 200,000-iteration default.
Fully numeric values preserve the existing minimum and maximum clamping. The
review is three files, +5/-2; cryptography, envelope format, monetary behavior
and consensus are unchanged.

The registered wallet-keystore group provides deterministic RED for the old
behavior and passes all 19 cases after the one-predicate fix. Removing only the
complete-consumption predicate reproduces RED; restoring it returns GREEN.
The focused ASan/UBSan group also passes. GCC 14.2.0 checks all 2,433
translation units with zero diagnostic sites; Clang 20.1.2 checks the same
2,433 units with the eight existing baseline sites and no new sites.
Architecture, generated inventory, documentation counts, consensus parity and
cyclomatic complexity gates pass.

The preserved unprivileged proof copy `/tmp/wn.mmgOK7/repo` independently
passes the exact head/base pair through the private-tmpfs/user-manager scope:
all 215 lint gates in 193.264s and all 99 exact impacted groups in 280.654s,
with zero failures, skips, unobserved results, load-flaky outcomes or cached
groups. Total foreground time is 519.207s and native status independently
reports `passed`. Receipt SHA256 is
`901cb75ab475713aa1673dc46459283805217fa27ab0e56c0638b6e54fad1810`.

Local artifacts `kdf-iteration-parse.bundle`, `kdf-iteration-parse.mbox`,
`kdf-iteration-parse.receipt`, `pr-kdf-iteration-parse.md`,
`kdf-iteration-exact-status.log` and `kdf-iteration-parse-final.sha256` verify
in the candidate worktree's ignored evidence directory. Two prerequisite
refusals are retained as environment evidence: direct wrapper execution was
not permitted, and the preserved proof checkout initially held the previous
candidate's producer. Invoking the unchanged wrapper through its Bash
interpreter, rebuilding the exact candidate-bound producer, and following the
native retry path resolved those conditions without weakening any gate or host
permission. No external publication action was attempted.

### Combined WKS1 header validation qualified

Branch `agent/upstream-wallet-keystore-header-iterations-20261004`, signed head
`aaf00c1471892008e2142dfd72781afb9f4cc970`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, combines the reserved-field
admission at `53bd0693c5d85cbb6e93bfa1ff46cef42a1b647b` with a second small commit
that makes `wks_envelope_iterations()` reject counts outside the same interval
accepted by decryption and a third test-only commit proving both comparisons.
The three-commit diff is three files, +53/-6. This is the preferred publication
package for WKS1 header validation; the earlier candidate heads and receipts
remain preserved for provenance.

Deterministic RED shows the prior inspection helper returned
`WKS_MIN_ITERS - 1` and `WKS_MAX_ITERS + 1` from malformed headers instead of
its documented zero sentinel. Removing only the lower comparison reproduces
the below-minimum failure; removing only the upper comparison reproduces the
excessive-value failure. The restored focused group passes all 20 cases and its
ASan/UBSan profile also passes. Uncached GCC 14.2.0 and Clang 20.1.2 sweeps each
re-read all 2,433 translation units: GCC reports zero diagnostic sites and
Clang reports the eight existing baseline sites with no new sites.
Architecture, generated inventory, documentation counts, consensus parity and
cyclomatic complexity gates pass.

The unprivileged private-tmpfs proof passes the exact combined head/base pair:
all 215 lint gates in 229.832s and all 99 exact impacted groups in 344.435s,
with zero failures, skips, unobserved results, load-flaky outcomes or cached
groups. Total foreground time is 530.013s; native status independently reports
`passed`. Receipt SHA256 is
`22beca0ac231b0cbedad8d81091b6e1c9b651a422625411002f96659db900f20`.

Local artifacts `keystore-header-validation.bundle`,
`keystore-header-validation.mbox`, `keystore-header-validation.receipt`,
`pr-keystore-header-validation-final.md`,
`header-validation-exact-status.log` and
`keystore-header-validation-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### Passphrase snapshot lifetime qualified

Branch `agent/upstream-wallet-passphrase-snapshot-20261004`, signed head
`df4f1ee9c4913f549f94334dba2138a33d5aa7e9`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, removes the public borrowed
pointer into the process-global wallet passphrase. Callers now either query
availability or copy the passphrase under the lock into a bounded,
operation-owned buffer. Legacy WKS1 SQLite encrypt/decrypt paths cleanse the
snapshot on every return and cleanse tentative output on failure. The review
is eight files, +68/-56; formats, algorithms and locking admission are
unchanged.

Deterministic RED retains the old borrowed pointer across the existing
one-second auto-lock and fails at the lifetime assertion while the other 18
wallet-keystore cases pass. Final `wallet_keystore` passes 19/19,
`wallet_sqlite_enc` passes 14/14 and `wallet_metadata_encryption` passes; all
three pass ASan/UBSan. The timer-bearing keystore group passes TSan after the
test joins the completed registry-owned timer. GCC 14.2.0 and Clang 20.1.2
each check all 2,433 translation units with no new diagnostic sites.
Architecture, generated inventory, documentation counts, file-size and
cap-15 cyclomatic-complexity gates pass.

The unprivileged private-tmpfs proof passes the exact head/base pair: the
receipt records the full lint dimension and all 126 selected impacted groups,
with zero failures, skips or reused results. Native status independently
reports `passed`; lint took 192.405s, tests 284.500s, the receipt records
532.938s total and the foreground step returned after 536.788s. Receipt SHA256
is `9b401c74c39cb80d6199f95d75e07d36ab56d7c0f60112a5e44e2e201ed81e11`.
The optional coverage manifest is absent, so no coverage claim is made.

Local artifacts `passphrase-snapshot.bundle`, `passphrase-snapshot.mbox`,
`passphrase-snapshot-exact-proof.receipt`,
`passphrase-snapshot-exact-status.log`, `pr-passphrase-snapshot.md` and
`passphrase-snapshot-final.sha256` verify in the candidate worktree's ignored
evidence directory. No external publication action was attempted.

### Metadata row-identity AAD bound qualified

Branch `agent/upstream-wallet-metadata-aad-bound-20261004`, signed head
`3fc2f0dd5aa4ee76a6f88f8fc73cdca8cb5c1365`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, makes the platform-neutral C23
metadata API require the exact 32-byte plan identity already used by every
production caller. Invalid lengths are refused before key access, randomness
or output writes; reported output length becomes zero and caller output stays
unchanged. Production callers share the public width constant. The review is
six files, +56/-13, including the required generated inventory update.

Deterministic RED shows the prior API admitted a 31-byte AAD. The final test
covers 31 bytes, 33 bytes and `SIZE_MAX` for encrypt and decrypt. Removing the
exact encrypt predicate reproduces RED; the restored focused and ASan/UBSan
groups pass. GCC 14.2.0 and Clang 20.1.2 each check all 2,433 translation
units with no new diagnostic sites. Architecture, documentation counts,
generated inventory, file-size, core-seal, consensus-parity and cap-15
complexity gates pass. The sealed AEAD implementation is unchanged.

The unprivileged private-tmpfs proof passes the exact head/base pair: all 215
lint gates and all 55 selected impacted groups complete with zero failures,
skips or reused results. Lint took 316.686s, tests 368.772s, the receipt records
632.988s total and the foreground step returned after 636.196s. Native status
independently reports `passed`; receipt SHA256 is
`644cf51e98a843b0249e2c7f587bd5b77a06e1827df0f85f3d301e5ecfc6cd5c`.

Local artifacts `metadata-aad-bound.bundle`, `metadata-aad-bound.mbox`,
`metadata-aad-bound.receipt`, `metadata-aad-exact-status.log`,
`pr-metadata-aad-bound.md` and `metadata-aad-bound-final.sha256` verify in the
candidate worktree's ignored evidence directory. No external publication
action was attempted.

### WKD1 failure output-length atomicity qualified

Branch `agent/upstream-wallet-key-envelope-failure-atomic-20261004`, signed
head `23a5b38e60463da2d2e9eb0f87515b1f45dbe691`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, makes both WKD1 encrypt and
decrypt clear the caller's reported output length before every failure. The
existing authenticated-decrypt cleanup still wipes tentative plaintext. A
small shared helper avoids raising the established complexity scores. The
review is three files, +58/-5; the envelope format, algorithms, key bytes and
successful outputs are unchanged.

Canonical RED preserves the old behavior and observes `SIZE_MAX` after a
locked-key failure. Removing the shared reset reproduces encrypt RED; bypassing
it only in decrypt reproduces decrypt RED. The final focused group passes all
15 cases, including row-swap authentication failure, and ASan/UBSan passes the
same cases. Uncached GCC 14.2.0 and Clang 20.1.2 each check all 2,433
translation units with no new diagnostic sites. Architecture, documentation,
generated inventory, file-size, sealed-core, consensus-parity and cap-15
complexity gates pass without a baseline increase.

The unprivileged private-tmpfs proof passes the exact head/base pair: all 215
lint gates and all 62 selected impacted groups complete with zero failures,
skips or reused results. Lint took 202.834s, tests 240.633s, the receipt records
442.861s total and the foreground step returned after 445.574s. Native status
independently reports `passed`; receipt SHA256 is
`c873e4bebfe5ab5b01c2655d6d458bf4cded73b68748c955a45759bbd86078dd`.

Local artifacts `key-envelope-failure-atomic.bundle`,
`key-envelope-failure-atomic.mbox`,
`key-envelope-failure-atomic-exact-proof.receipt`,
`key-envelope-failure-atomic-exact-status.log`,
`pr-key-envelope-failure-atomic.md` and
`key-envelope-failure-atomic-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### Wallet intent monetary bounds qualified

Branch `agent/upstream-wallet-intent-money-range-20261004`, signed head
`438cfb15e7e987453fcc1b43f73b7f6f02e24b16`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, applies ZClassic's existing
`MoneyRange` and `MAX_MONEY` contract to transparent, private and fanout
planning, the pure decision core and the durable reservation model. Zero and
exactly `MAX_MONEY` remain valid when their sum is valid. Negative values,
either field above `MAX_MONEY`, and target plus fee above `MAX_MONEY` fail
before addition. The review is seven files, +62/-12, including the generated
capability inventory; consensus and the sealed core are unchanged.

Canonical RED shows the old decision core returning `ALLOW` for target
`MAX_MONEY` plus fee 1. Reversing only the new sum cap reproduces that RED;
reversing the durable model predicate makes direct row validation accept the
same oversized reservation and reproduces model RED. The final registered
test explicitly covers zero, `MAX_MONEY`, `MAX_MONEY + 1`, negative values,
`INT64_MIN` and `INT64_MAX`, and passes under the focused and ASan/UBSan
profiles. Uncached GCC 14.2.0 and Clang 20.1.2 each check all 2,433 translation
units with no new diagnostic sites. Architecture, generated inventory,
documentation, file-size, core-seal, consensus-parity and cap-15 complexity
gates pass without a baseline increase.

The unprivileged private-tmpfs proof passes the exact head/base pair: all 215
lint gates and all 58 selected impacted groups complete with zero failures,
skips or reused results. Lint took 211.223s, tests 307.237s, the receipt records
513.236s total and the foreground step returned after 516.535s. Native status
independently reports `passed`; receipt SHA256 is
`9c8ca349c6ee1a60e507dec4c239ca02963f0eaedf03dc17aa9c089db6b18fb5`.

Local artifacts `intent-money-range.bundle`, `intent-money-range.mbox`,
`intent-money-range.receipt`, `intent-money-range-exact-status.log`,
`pr-intent-money-range.md` and `intent-money-range-final.sha256` verify in the
candidate worktree's ignored evidence directory. The direct proof-account
producer build was refused because that shell lacked its user-manager bus;
the unchanged qualified private-tmpfs/user-manager wrapper then rebuilt and
proved the exact candidate. No permission, gate or host policy changed. No
external publication action was attempted.

### Raw intent load failure atomicity qualified

Branch `agent/upstream-wallet-raw-load-failure-atomic-20261004`, signed head
`2c23eba90f94d56b9d67bb5dd6372436e0aeb8cc`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, clears the reported raw
transaction length before every load failure where the caller supplied the
length pointer. Successful loads still copy and report the exact stored bytes;
missing-row and undersized-buffer refusals leave the output buffer unchanged.
The review is three files, +21/-4, including the generated inventory. Raw
transaction storage, parsing, signing, reconciliation and consensus are
unchanged.

Canonical RED observes an undersized-buffer refusal leaving `out_len` at
`SIZE_MAX`. The final regression proves zero length and unchanged output on
both undersized-buffer and missing-row refusals, followed by an exact successful
load. Removing only the new zero assignment reproduces RED and restoration
returns the complete diff to the same hash. The focused and ASan/UBSan
`transaction_intent` groups pass. Uncached GCC 14.2.0 and Clang 20.1.2 each
check all 2,433 translation units with no new diagnostic sites. Architecture,
generated inventory, documentation, file-size, core-seal, consensus-parity and
cap-15 complexity gates pass without a baseline increase.

The unprivileged private-tmpfs proof passes the exact head/base pair: all 215
lint gates and all 45 selected impacted groups complete with zero failures,
skips or reused results. Lint took 237.866s, tests 303.324s, the receipt records
512.316s total and the foreground step returned after 514.996s. Native status
independently reports `passed`; receipt SHA256 is
`5336d82a279424ef069f50ecc9adf41aabdfa8d5bfa6a97149cdccff6324396f`.

Local artifacts `raw-load-failure-atomic.bundle`,
`raw-load-failure-atomic.mbox`, `raw-load-failure-atomic.receipt`,
`raw-load-failure-exact-status.log`, `pr-raw-load-failure-atomic.md` and
`raw-load-failure-atomic-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

The follow-up WKS1 output-length hypothesis is NO-DEFECT. The qualified
combined decrypt-safety branch explicitly documents and tests that a failed
decrypt leaves `*out_len` unchanged while authentication failures wipe the
tentative plaintext span. Changing that contract would conflict with existing
review-ready evidence, so no source change or repeated qualification was made.

### Wallet block-scan descriptor inheritance qualified

Branch `agent/upstream-wallet-scan-cloexec-20261004`, signed head
`12344c4c24bf92e798926cbd0ec99a5426d28803`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, routes transparent scan pass 1,
transparent scan pass 2 and Sapling witness replay through one read-only block
file opener. The opener requests `O_CLOEXEC` on POSIX and `_O_NOINHERIT` on
Windows. Scan ranges, file offsets, parsing and wallet state are unchanged.
The review is six files, +55/-17, including the generated inventory.

Canonical RED observes the descriptor without `FD_CLOEXEC`. The final Linux
runtime regression inspects the admitted descriptor with `F_GETFD` and passes.
Removing only the `O_CLOEXEC` flag reproduces RED; restoration returns the
complete diff to SHA256
`cb2e8295235bbd43a4c0ad6092394e4c78957e03f5dfaaca123c8f6630e59719`.
The focused and ASan/UBSan `block_scan` groups pass. Uncached GCC 14.2.0 and
Clang 20.1.2 each check all 2,433 translation units with no new diagnostic
sites. The Linux-hosted MinGW lane passes its strict C23 seam compile, all
2,399 Windows cross-syntax translation units and 74 acceptance cross-links;
native Windows runtime remains unobserved. Architecture, generated inventory,
documentation, file-size, core-seal, consensus-parity and cap-15 complexity
gates pass without a baseline increase.

The unprivileged private-tmpfs proof passes the exact head/base pair: all 215
lint gates and all 26 selected impacted groups complete with zero failures,
skips, cached results or reused receipt. Lint took 189.367s, tests 203.592s,
the receipt records 507.127s total and the foreground step returned after
509.958s. Native status independently reports `passed`; receipt SHA256 is
`ad1320441d2871bdc381bb459bcac848ead553b44f0d5efc35f911cfa35b0bd1`.

Local artifacts `wallet-scan-cloexec.bundle`, `wallet-scan-cloexec.mbox`,
`wallet-scan-cloexec.receipt`, `wallet-scan-cloexec-exact-status.log`,
`pr-wallet-scan-cloexec.md` and `wallet-scan-cloexec-final.sha256` verify in
the candidate worktree's ignored evidence directory. No external publication
action was attempted.

### Sensitive wallet read-stream inheritance qualified

Branch `agent/upstream-wallet-sensitive-read-cloexec-20261004`, signed head
`4a71914b1fdf9847e9c58e389bde964485f14147`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, adds one platform C23 read-stream
primitive and routes six wallet credential, configuration and Tor
onion-hostname reads through it. The primitive requests `O_CLOEXEC` before
`fdopen` on POSIX and `_O_NOINHERIT | _O_BINARY` before `_fdopen` on Windows;
it closes its owned descriptor if stream conversion fails. File contents,
parsing, path selection, failure returns, wallet custody semantics and process
launch policy are unchanged. The review is eight files, +98/-14, including the
generated inventory and the explicit `CAP_FS_READ` declaration.

Canonical RED removes only `O_CLOEXEC`: the new Linux runtime assertion then
observes the admitted stream descriptor without `FD_CLOEXEC`, while the
independent wallet-view port group remains green. The restored candidate passes
both focused groups and both groups under ASan/UBSan with zero skips. Uncached
GCC 14.2.0 and Clang 20.1.2 each check all 2,434 translation units with no new
diagnostic sites. The Linux-hosted MinGW lane passes the focused helper compile,
all 2,400 Windows cross-syntax translation units and 74 strict C23 acceptance
cross-links; native Windows runtime remains unobserved. Architecture,
documentation-count, generated-inventory, file-size, core-seal,
consensus-parity and cap-15 complexity gates pass without a baseline increase.

The first exact proof ran all 1,236 selected groups successfully but correctly
refused the candidate on two lint gates: the new regression used a bare `/tmp`
fixture and the new source lacked its filesystem-read capability declaration.
Signed follow-up `4a71914b1fdf9847e9c58e389bde964485f14147` uses the repository-scoped
fixture helper and declares the authority. The final unprivileged private-tmpfs
proof passes all 215 lint gates and all 1,236 selected groups with zero failures,
skips, cached results or reused receipt. Lint took 192.857s, tests 602.372s and
the proof command returned after 833.729s. Native status independently reports
`passed`; receipt SHA256 is
`9f7dbb34b64ce8665e786b5ed7eaad9537a6c0c0e71a7f2e941b567ed0661992`.

Local artifacts `wallet-sensitive-read-cloexec.bundle`,
`wallet-sensitive-read-cloexec.mbox`, `wallet-sensitive-read-cloexec.receipt`,
the exact lint/test/phase/status logs, `pr-wallet-sensitive-read-cloexec.md`,
`wallet-sensitive-read-cloexec-qualification.txt` and
`wallet-sensitive-read-cloexec-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### LevelDB Sapling-seed allocation retirement qualified

Branch `agent/upstream-wallet-leveldb-seed-retirement-20261004`, signed head
`ec8a020c1d8b6d9860efd6fc85717f31e57f0807`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, cleanses the exact database-owned
heap span before releasing a valid or malformed Sapling-seed record. Accepted
32-byte records still copy the same bytes into caller-owned output; malformed
records still fail and leave caller output unchanged. The review is four files,
+115/-8, including the deterministic production-source harness and generated
capability inventory. LevelDB format, recovery, derivation, wallet encryption,
consensus and sealed-core code are unchanged.

Canonical RED observes nonzero seed bytes at the old release boundary. Replacing
only the new cleanse call with a compile-valid no-op reproduces the
`secret_all_zero` failure on the shared-pool attempt and isolated retry. The
restored focused group passes with zero skips and passes ASan/UBSan. Uncached
GCC 14.2.0 and Clang 20.1.2 each check all 2,433 production translation units
with no new diagnostic sites. Architecture, documentation-count, generated-
inventory, file-size, core-seal, consensus-parity and cap-15 complexity gates
pass; the complexity scan covers 68,755 functions in 4,681 files.

The first exact-proof attempt stopped during bundle construction after disk
fallback exhausted free space; it reached neither lint nor tests and makes no
product claim. The prescribed retry used the documented private RAM-scratch
root with the 6 GiB reservation, 8 GiB remaining-free floor, 24 GiB process
limit and every proof gate retained. The policy-5 receipt passes all 215 lint
gates and all 1,236 selected test groups: 1,236 run, zero failures, skips or
reused results. Lint took 199.096s, tests 564.469s and the receipt records
741.334s total. Native status independently reports `passed`; receipt SHA256 is
`19173a1b9a6d7f66092e587eab0907064527b9696143035b144749d6a7e5ccb7`.

Local artifacts `wallet-leveldb-seed-retirement.bundle`,
`wallet-leveldb-seed-retirement.mbox`,
`wallet-leveldb-seed-retirement.receipt`, exact phase/status evidence,
`pr-wallet-leveldb-seed-retirement.md`,
`wallet-leveldb-seed-retirement-qualification.txt` and
`wallet-leveldb-seed-retirement-final.sha256` verify in the candidate
worktree's ignored evidence directory. No external publication action was
attempted.

### LevelDB iterator plaintext-buffer retirement qualified

Branch `agent/upstream-db-iterator-scratch-retirement-20261004`, signed head
`99f0e7bb87307a9a86651d03ea7f8deb4bb38238`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, cleanses the shared storage
iterator's complete deobfuscation allocation before buffer growth and teardown.
Wallet private keys and Sapling expanded spending keys traverse this generic
C23 boundary. Database bytes, XOR deobfuscation, iterator ordering, admitted
lengths, allocation sizing, wallet formats, consensus and sealed-core code are
unchanged. The review is four files, +81/-10, including the generated inventory
and a test-only per-iterator observer.

Canonical RED uses real LevelDB values under a one-byte obfuscation key, grows
the plaintext buffer from a 2-byte value to a 300-byte value and then destroys
the iterator. Without the production cleanse, `test_rpc` fails on the shared-
pool attempt and isolated retry while the other six selected groups pass.
Adding only the existing optimizer-resistant cleanse include and call makes all
seven groups pass with zero skips; ASan/UBSan passes the same seven. Uncached
GCC 14.2.0 and Clang 20.1.2 each check all 2,433 production translation units
with no new diagnostic sites. The MinGW lane checks 2,399 Windows translation
units with zero new failures and cross-links all 74 strict C23 acceptance
programs; native Windows runtime remains unobserved. Architecture,
documentation-count, generated-inventory, file-size, core-seal,
consensus-parity, PR-security and cap-15 complexity gates pass.

The first exact-proof request refused because the isolated checkout still had
the preceding candidate at HEAD. After checking out the exact commit, the next
request refused a `z23-dev` producer built from the preceding source. Both are
fail-closed prerequisite evidence and make no product claim. The prescribed
exact-source `make dev-bin`, `dev proof retry` and `dev proof step` then pass all
215 lint gates and all 258 impact-selected groups: 258 run, zero failures,
skips or reused results. Lint took 198.504s, tests 277.319s and the receipt
records 526.945s total. Native status independently reports `passed`; receipt
SHA256 is
`275ef299da366f5ca0917cbed8ec4fef057005a8f731d319cfbd2ba65ef30c7f`.

Local artifacts `db-iterator-scratch-retirement.bundle`,
`db-iterator-scratch-retirement.mbox`,
`db-iterator-scratch-retirement.receipt`, exact prerequisite/phase/status
evidence, `pr-db-iterator-scratch-retirement.md`,
`db-iterator-scratch-retirement-qualification.txt` and
`db-iterator-scratch-retirement-final.sha256` verify in the candidate
worktree's ignored evidence directory. No external publication action was
attempted.

### LevelDB iterator allocation-growth bound qualified

Branch `agent/upstream-db-iterator-growth-bound-20261004`, signed head
`0fbae3bf859ace48422a95939a91f9b425b61f00`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, checks the shared storage
iterator's `value_len + 256` growth before allocation. Lengths through
`SIZE_MAX - 256` preserve the existing capacity rule; larger lengths now log
and fail before allocation while retaining the iterator's current reusable
buffer. Database bytes, XOR deobfuscation, representable allocation sizes,
wallet formats, consensus and sealed-core code are unchanged. The review is
four files, +54/-5, including the generated inventory and a test-only boundary
hook.

Canonical RED routes the old unchecked addition through the testable helper:
`SIZE_MAX - 255` and `SIZE_MAX` wrap and `test_rpc` fails on both the shared-
pool attempt and isolated retry while the other six selected groups pass. The
minimal range check makes all seven groups pass with zero skips; ASan/UBSan
passes the same seven. GCC 14.2.0 and Clang 20.1.2 each check all 2,433
translation units with no new diagnostic sites. The initial root-capable
compiler invocations correctly failed their unreadable-file control probe;
running under the established DAC capability drop made that probe meaningful.
The MinGW lane checks 2,399 Windows translation units with zero new failures
and cross-links all 74 strict C23 acceptance programs; native Windows runtime
remains unobserved. Architecture, documentation-count, generated-inventory,
file-size, core-seal, consensus-parity, PR-security and cap-15 complexity gates
pass; the complexity scan covers 68,757 functions in 4,681 files.

The policy-5 private-RAM-scratch proof passes the exact head/base pair: all 215
lint gates and all 258 impact-selected groups complete, with 258 tests run and
zero failures, skips or reused test results. Lint took 202.825s, tests 279.134s
and the signed receipt records 522.346s total. Native status independently
reports `passed` with `receipt_reused=false`; receipt SHA256 is
`29cf9cf356889bc21bd5a6986f939cbdfaf51093eb532233017af29c83c34533`.

Local artifacts `db-iterator-growth-bound.bundle`,
`db-iterator-growth-bound.mbox`, `db-iterator-growth-bound.receipt`, exact
phase/status evidence, `pr-db-iterator-growth-bound.md` and
`db-iterator-growth-bound-qualification.txt` verify in the candidate
worktree's ignored evidence directory. No external publication action was
attempted.

### LevelDB internal-key capacity bound qualified

Branch `agent/upstream-ldb-key-capacity-bound-20261004`, signed head
`68546b2d3e63c7b5f278348e6a7ab7f5c984a5e5`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, checks the eight-byte
sequence/type trailer before sizing any internal LevelDB key. Point lookup,
WAL replay and iterator seek use the checked size; iterator growth selects the
exact representable requirement before doubling could overflow. Existing
lookup, seek and build refusals remain the public failure behavior. Internal-
key bytes, comparison order, WAL replay, wallet formats, consensus and sealed-
core code are unchanged. The review is seven files, +117/-12, including the
generated inventory and deterministic boundary tests.

Canonical RED observes the old `user_key_length + 8` calculation admit
overflowing values while the 5,135-record differential corpus, WAL replay and
five damaged-input refusals remain green. Removing only the new guard makes the
boundary assertion fail and the public extreme point/seek test terminate with
signal 11. The restored focused and ASan/UBSan groups pass with zero skips.
GCC 14.2.0 and Clang 20.1.2 each check all 2,433 translation units with no new
diagnostic sites. The MinGW lane checks 2,399 Windows translation units and
cross-links all 74 strict C23 acceptance programs; native Windows runtime
remains unobserved. Architecture, documentation-count, generated-inventory,
file-size, core-seal, consensus-parity, PR-security and cap-15 complexity gates
pass; the complexity scan covers 68,758 functions in 4,681 files.

Three attempts that lacked the explicit private RAM-scratch environment fell
back to a nearly full disk. The first failed while writing unrelated test
fixtures; the next two were stopped after the fallback was observed. They make
no product claim. The C23 reservation control then acquired and released the
private 6 GiB lease, and the proof ran with the root explicitly inherited by
the bounded scope. The exact policy-5 receipt passes all 215 lint gates and all
17 impact-selected groups: 17 run, zero failures, skips or reused results. Lint
took 189.639s, tests 197.239s, the receipt records 450.697s and the foreground
step returned after 452.306s. The RAM generation retired normally; receipt
SHA256 is
`736760414408ad8c315ad50ec1cd29d1852e4c66cab18f3299f9d2dcfaad4101`.

Local artifacts `ldb-key-capacity-bound.bundle`,
`ldb-key-capacity-bound.mbox`, `ldb-key-capacity-bound.receipt`, exact phase
evidence, `pr-ldb-key-capacity-bound.md` and
`ldb-key-capacity-bound-qualification.txt` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### LevelDB log arithmetic bounds qualified

Branch `agent/upstream-ldb-log-bounds-20261004`, signed head
`35e595f6a353d737acee2286b29da03db6cef8ab`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, checks physical-record header,
payload and trailer spans before advancing the LevelDB log cursor. Fragmented-
record accumulation validates its current length/capacity invariant, checks
the append addition and selects the exact representable requirement before
capacity doubling could overflow. Existing framing, CRC, torn-tail and clean-
EOF behavior is preserved. WAL bytes, database records, wallet formats,
consensus and sealed-core code are unchanged. The review is four files,
+153/-13, including the generated inventory and deterministic boundary tests.

Canonical RED routes the old unchecked arithmetic through the test helpers:
`test_ldb_reader` fails only the arithmetic-bound case while its 5,135-record
differential corpus, 1,850-entry WAL replay and five damaged-input refusals
remain green. Separately removing the span guard and scratch-addition guard
makes the canonical group fail; restoring both makes the focused and
ASan/UBSan lanes pass with zero skips. GCC 14.2.0 and Clang 20.1.2 each check
all 2,433 production translation units with no new diagnostic sites. The
MinGW lane checks 2,399 Windows translation units and cross-links all 74
strict C23 acceptance programs; native Windows runtime remains unobserved.
Architecture, documentation-count, generated-inventory, file-size, core-seal,
consensus-parity, PR-security and cap-15 complexity gates pass; the complexity
scan covers 68,764 functions in 4,681 files.

The exact policy-5 private-RAM-scratch proof passes the exact head/base pair:
all 215 lint gates and all 17 impact-selected groups complete, with 17 tests
run and zero failures, skips or reused test results. Lint took 196.965s, tests
204.472s and the signed receipt records 456.248s total. The foreground step
returned after 458.092s. Native status under the producing identity reports
`passed` with `receipt_reused=false`; the RAM generation retired normally and
receipt SHA256 is
`e89bbb5bbca15e38eeb44df940f9aca2f2c9ee632b954f406727c18f73e9e1a8`.

Local artifacts `ldb-log-bounds.bundle`, `ldb-log-bounds.mbox`,
`ldb-log-bounds.receipt`, exact phase/test/lint/status evidence,
`pr-ldb-log-bounds.md` and `ldb-log-bounds-qualification.txt` verify in the
candidate worktree's ignored evidence directory. No external publication
action was attempted.

### LevelDB empty-fragment null-copy fix qualified

Branch `agent/upstream-ldb-empty-fragment-20261004`, signed head
`bf7a60ee79005ed24f8728962260202e831ab84a`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, avoids passing the initially null
fragment scratch destination to `memcpy` when an admitted LevelDB FIRST, MIDDLE
or LAST fragment has length zero. Empty fragments continue to succeed without
allocation and leave scratch length/capacity zero; every nonempty fragment is
copied exactly as before. Framing, CRC, fragmented-record assembly, torn-tail
handling, WAL replay, database bytes, wallet formats, consensus and sealed-core
code are unchanged. The review is four files, +41/-3, including the generated
inventory and a narrow test seam.

A standalone Clang 20 ASan/UBSan control first detects the exact zero-length
null-destination call. Canonical sanitized RED then fails `test_ldb_reader`
with the same `null pointer passed as argument 1` diagnostic while exercising
an empty FIRST fragment. Removing only the new nonzero-length guard reproduces
the failure. The restored focused and ASan/UBSan groups pass 1/1 with zero
skips, including the 5,135-record differential corpus, 1,850 WAL entries,
missing-CURRENT behavior and five corruption refusals. GCC 14.2.0 and Clang
20.1.2 each check all 2,433 production translation units with no new diagnostic
sites. The MinGW lane checks all 2,399 Windows translation units clean and
cross-links all 74 strict C23 acceptance programs; native Windows runtime
remains unobserved. Architecture, documentation-count, generated-inventory,
file-size, core-seal, consensus-parity, PR-security and cap-15 complexity gates
pass; the complexity scan covers 68,756 functions in 4,681 files.

The exact policy-5 private-RAM-scratch proof passes the exact head/base pair:
all 215 lint gates and all 17 impact-selected groups complete, with 17 tests
run and zero failures, skips or reused test results. Lint took 198.516s, tests
206.830s and the signed receipt records 450.638s total. The foreground step
returned after 452.272s. Native status under the producing identity reports
`passed` with `receipt_reused=false`; the RAM generation retired normally and
receipt SHA256 is
`96bd71db6bb8244ed5274847b0a89a3b7c1d24c78f53984eb90dd55bab0f7077`.

Local artifacts `ldb-empty-fragment.bundle`, `ldb-empty-fragment.mbox`,
`ldb-empty-fragment.receipt`, exact phase/test/lint/status evidence,
`pr-ldb-empty-fragment.md` and
`ldb-empty-fragment-qualification.txt` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### Wallet-backup plaintext retirement qualified

Branch `agent/upstream-wallet-backup-plain-retirement-20261004`, signed head
`66f45311df40379cc4dd5962b4bc34d0f6208976`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, wipes the exact admitted SQLite
wallet plaintext span before freeing it after encrypted-output allocation
fails. The same helper owns normal plaintext retirement, so both exits share
one auditable cleanup path. Backup format, PBKDF2 parameters, password
ownership, storage behavior, recovery policy, consensus and sealed-core code
are unchanged. The review is three files, +87/-7, including the generated
inventory and a deterministic test-only retirement observer.

Canonical RED injects failure at `wallet_backup encrypt_buf` after reading a
257-byte nonzero plaintext fixture and fails only the new retirement
assertion; the platform-port group remains green. Removing only the cleanse
call reproduces that failure. The restored focused and ASan/UBSan lanes pass
2/2 groups with zero skips. GCC 14.2.0 and Clang 20.1.2 each check all 2,433
production translation units with no new diagnostic sites. The MinGW lane
checks all 2,399 Windows translation units clean and cross-links all 74 strict
C23 acceptance programs; native Windows runtime remains unobserved.
Architecture, documentation-count, generated-inventory, file-size, core-seal,
consensus-parity, PR-security and cap-15 complexity gates pass; the complexity
scan covers 68,759 functions in 4,681 files.

The exact policy-5 private-RAM-scratch proof passes the exact head/base pair:
all 215 lint gates and all 43 impact-selected groups complete, with 43 tests
run and zero failures, skips or reused test results. Lint took 190.801s, tests
210.971s and the signed receipt records 423.774s total. The foreground step
returned after 425.410s. Native status under the producing identity reports
`passed` with `receipt_reused=false`; the RAM generation retired normally and
receipt SHA256 is
`7fda44eaea420b212c759dd1423246a86bd1eeae0b1ebe65eb48e609e303d374`.

Local artifacts `wallet-backup-plain-retirement.bundle`,
`wallet-backup-plain-retirement.mbox`,
`wallet-backup-plain-retirement.receipt`, exact phase/test/lint/status
evidence, `pr-wallet-backup-plain-retirement.md` and
`wallet-backup-plain-retirement-qualification.txt` verify in the candidate
worktree's ignored evidence directory. No external publication action was
attempted.

### Wallet-backup service-password retirement qualified

Branch `agent/upstream-wallet-backup-password-retirement-20261004`, signed head
`148902ca223231b83c35502f183bbd1b1bc2606f`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, replaces the backup service's
borrowed password pointer and process-lifetime environment-password cache with
one service-owned allocation. Startup copies the enabled password before the
worker can run and refuses an allocation failure. Stop and spawn-failure exits
wipe the exact allocation, including its terminator, before release; disabled
encryption retains no password. Backup bytes, format, scheduling, restore,
cryptography, consensus and sealed-core code are unchanged. The review is seven
files, +386/-93, including generated inventory and deterministic retirement
tests. Extracting start preflight reduced `wallet_backup_start()` complexity
from its legacy pin of 19 to 11 and removed the pin.

Canonical RED injects the existing `wallet_backup_password` allocation failure:
the old service ignored it, started the worker and failed the new assertion;
the platform-port group remained green. Removing only the new cleanse makes the
full-span retirement assertion fail while the port group passes. The restored
focused and ASan/UBSan lanes pass 2/2 groups with zero skips. GCC 14.2.0 and
Clang 20.1.2 each check all 2,433 production translation units uncached with no
new diagnostic sites. The MinGW lane checks all 2,399 Windows translation units
clean and cross-links all 74 strict C23 acceptance programs; native Windows
runtime remains unobserved. Architecture, documentation-count, generated-
inventory, file-size, core-seal, consensus-parity, PR-security and cap-15
complexity gates pass; the complexity scan covers 68,765 functions in 4,681
files.

The exact policy-5 private-RAM-scratch proof passes the exact head/base pair:
all 215 lint gates and all 108 impact-selected groups complete cold, with 108
tests run and zero failures, skips or reused test results. Lint took 181.361s,
tests 253.112s and the signed receipt records 504.664s total. The RAM generation
retired normally; receipt SHA256 is
`c8ea9fcef73d838e8386114df72fe06009802aa47a35f3c9dc4446b338f883a0`.
The local `origin/main` reference advanced after this proof, so this evidence is
explicitly limited to the pinned base above.

Local artifacts `wallet-backup-password-retirement.bundle`,
`wallet-backup-password-retirement.mbox`,
`wallet-backup-password-retirement.receipt`, exact phase/test/lint evidence,
`pr-wallet-backup-password-retirement.md` and
`wallet-backup-password-retirement-qualification.txt` verify in the candidate
worktree's ignored evidence directory. No external publication action was
attempted.

### Wallet-restore target-path admission qualified

Branch `agent/upstream-wallet-restore-path-bounds-20261004`, signed head
`36f90858696e8532777d7f352804078dfbc7fd0c`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, refuses a restore target unless
`<datadir>/node.db` fits the fixed report field exactly. The report path is also
the pathname consumed by the restore, so the old unchecked `snprintf` could
create or modify a different truncated file before the operation eventually
failed. The new preflight runs before every filesystem action and clears the
target field on refusal. Representable paths, backup bytes, encryption, merge
and keep-existing behavior, wallet formats, consensus and sealed-core code are
unchanged. The review is five files, +91/-15; helper extraction reduces
`wallet_restore_run()` complexity from 26 to 22.

Canonical RED on the prior production source fails the no-touch assertion
after observing the truncated target file. Changing only the final capacity
guard from `>=` to `==` makes both exact-refusal and no-touch assertions fail.
The restored focused and ASan/UBSan/LSan lanes pass 1/1 with zero skips and
leak detection enabled. Uncached GCC 14.2.0 and Clang 20.1.2 each check all
2,433 translation units with no new diagnostics. MinGW considers 2,399
translation units and cross-links all 74 strict C23 acceptance programs;
native Windows runtime remains unobserved. Architecture, documentation-count,
generated-inventory, file-size, core-seal, consensus-parity, PR-security and
cap-15 complexity gates pass.

The first exact request refused before lint or tests because its retained
`z23-dev` producer belonged to the preceding candidate. Building `dev-bin`
from the exact source and using the prescribed retry path repaired only that
prerequisite. The fresh policy-5 proof then passes all 215 lint gates and all
66 impact-selected groups cold, with zero failures, skips, reused, unobserved
or load-flaky outcomes. Lint takes 194.800s, tests 262.342s and the receipt
records 511.536s total. The private-RAM generation retires normally; receipt
SHA256 is
`95a79ce29f79bbb82ae4ad3d50a430f12b898a373fdda1ef239351dc3aa405d6`.
The optional coverage manifest is absent, so no coverage claim is made.

Local artifacts `wallet-restore-path-bounds.bundle`,
`wallet-restore-path-bounds.mbox`, `wallet-restore-path-bounds.receipt`,
`pr-wallet-restore-path-bounds.md`, exact phase/lint/test evidence,
`wallet-restore-path-bounds-qualification.txt` and
`wallet-restore-path-bounds-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### Wallet-restore node-lock pathname admission qualified

Branch `agent/upstream-wallet-lock-path-bounds-20261004`, signed head
`3816edfc784c9e4fe7d3f54232db67925f94ba8f`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, refuses
`wallet_restore_datadir_free()` when the exact `<datadir>/zclassic23.pid`
pathname does not fit its fixed probe buffer. The refusal occurs before any
filesystem call, so an overlong valid datadir cannot be truncated into a
different lock path and reported free while the real node pidfile remains
locked. Representable paths, restore merge behavior, wallet formats,
consensus, monetary rules and sealed-core code are unchanged. The review is
four files, +70/-5, including the generated capability inventory and one
deterministic long-path regression.

Canonical RED on the prior production code locks the exact long pidfile and
fails only `long datadir never bypasses the held pidfile`. Changing the final
capacity guard from `>=` to `>` reproduces that exact failure. The restored
focused and ASan/UBSan/LSan lanes pass 1/1 with zero skips and leak detection
enabled. Uncached GCC 14.2.0 and Clang 20.1.2 each check all 2,433 translation
units with no new diagnostics. MinGW checks all 2,399 Windows translation
units clean and cross-links all 74 strict C23 acceptance programs; native
Windows runtime remains unobserved. Architecture, documentation-count,
generated-inventory, PR-security and cap-15 complexity gates pass; the
complexity scan covers 68,755 functions in 4,681 files.

Two exact-proof prerequisite attempts refused before lint or tests: the first
was outside a delegated user-manager memory scope, and the second selected the
default RAM root owned by another uid. The prescribed retry used the qualified
user scope and explicit private RAM root. The fresh policy-5 proof then passes
all 215 lint gates and all 66 impact-selected groups cold, with zero failures,
skips, reused results, unobserved cases or load-flaky outcomes. Lint takes
196.495s, tests 261.175s and the receipt records 510.633s total. The RAM
generation retires normally; receipt SHA256 is
`d0199dd64b36f03d30c825ce06399d856a71d70cf108d2037d849d627693cd5c`.
The optional coverage manifest is absent, so no coverage claim is made.

A fresh compiler-clone setup attempted public dependency fetches before the
offline flag was applied and was stopped immediately; it is retained only as
environment evidence and makes no qualification claim. The successful
compiler lanes used checksum-matched local caches with offline mode enabled.

Local artifacts `wallet-lock-path-bounds.bundle`,
`wallet-lock-path-bounds.mbox`, `wallet-lock-path-bounds.receipt`, exact
phase/lint/test evidence, `pr-wallet-lock-path-bounds.md`,
`wallet-lock-path-bounds-qualification.txt` and
`wallet-lock-path-bounds-final.sha256` verify in the candidate worktree's
ignored evidence directory. No external publication action was attempted.

### Wallet-restore writer serialization qualified

Branch `agent/upstream-wallet-restore-writer-lock-20261004`, signed head
`9e9e0b428ab4e81f705231c7d75ed5dc9774dd6d`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, makes the public restore service
hold the existing per-datadir `wallet-recovery.lock` before any restore-owned
target access. Two restores, or restore and mnemonic recovery, can no longer
pass their initial probes together and write the same target wallet
concurrently. The guard covers commit and current dry-run behavior and is
released after every target handle closes. Consensus, monetary rules, wallet
formats, backup bytes, derivation and collision policy are unchanged. The
review is five files, +135/-65; helper extraction reduces
`wallet_restore_run()` complexity from 26 to the repository cap of 15.

Canonical RED on the prior production source held the recovery lock while the
public restore API still created `node.db`, failing exactly the lock-refusal
and no-target-creation assertions. Replacing only lock acquisition with
`ZCL_OK` reproduced those two failures. The restored focused and
ASan/UBSan/LSan lanes pass 1/1 with zero skips and leak detection enabled.
Uncached GCC 14.2.0 and Clang 20.1.2 each check all 2,433 translation units
with no new diagnostics. MinGW checks all 2,399 Windows translation units and
cross-links 74 strict C23 acceptance programs; native Windows runtime remains
unobserved. Architecture, documentation-count, generated-inventory,
PR-security and cap-15 complexity gates pass.

Two exact-proof prerequisite attempts refused before lint or tests: one was
outside a delegated user-manager memory scope and one lacked that user's
runtime bus environment. The prescribed user scope, runtime bus and private
RAM root repaired only the prerequisite. The fresh policy-5 proof then passes
all 215 lint gates and all 66 impact-selected groups cold, with zero failures,
skips, reused results, unobserved cases or load-flaky outcomes. The receipt
records 508.356s total, `receipt_reused=false`, and normal retirement of the
private RAM generation. Receipt SHA256 is
`c0ee1248e8f6d85e63a00e38573a5f406fc58220f3829c0705965c5a752feacb`.
The optional coverage manifest is absent, so no coverage claim is made.

Local artifacts `wallet-restore-writer-lock.bundle`,
`wallet-restore-writer-lock.mbox`, `wallet-restore-writer-lock.receipt`, exact
phase/lint/test evidence, `pr-wallet-restore-writer-lock.md`,
`wallet-restore-writer-lock-qualification.txt` and
`wallet-restore-writer-lock-final.sha256` verify in the candidate worktree's
ignored evidence directory. Node-start coordination with this lock is a
separate follow-up. No external publication action was attempted.

### Node-start and wallet-writer coordination qualified

Branch `agent/upstream-node-recovery-lock-20261004`, signed head
`2fbf4fb5e1a070bd222b50c55c1c5b170b4b4163`, base
`3a93e60ebf922af3d119b9facc1d95803f42844b`, makes POSIX node startup acquire
the existing `wallet-recovery.lock` before the pidfile lock and retain both
until shutdown. Mnemonic recovery, the separately prepared restore writer
guard, and node startup therefore share one datadir writer admission point.
All failures after recovery-guard acquisition release it. The established
second-node diagnostic is preserved by a read-only pidfile lock probe; that
probe does not decide admission. Consensus, monetary rules, wallet contents,
database formats, recovery derivation and the Windows lock implementation are
unchanged. The review is five files, +430/-78; helper extraction reduces the
main POSIX acquisition function's complexity from 42 to 35.

Canonical RED on the upstream base held `wallet-recovery.lock` and observed
the node still start, failing the new exclusion assertion. Replacing only the
recovery `flock()` with descriptor success makes both mutual-exclusion
assertions fail while legacy lock cases remain green. Focused, ASan/UBSan/LSan
and TSan lanes each pass 1/1 groups with zero skips, including runtime checks
for recovery-to-node and node-to-recovery exclusion, `O_CLOEXEC` retirement of
both lock descriptors, symlink refusal and hard-link refusal. The adjacent
`wallet_recovery_safety` group also passes 1/1 with zero skips.

Uncached unprivileged GCC 14.2.0 and Clang 20.1.2 each check all 2,433
translation units with no new diagnostics. MinGW checks all 2,399
Windows-visible translation units clean; native Windows runtime remains
unobserved. All 33 fast lint gates, architecture, documentation counts,
generated inventory and the cap-15 complexity gate pass. The PR security scan
has one review-only medium finding for the test's fixed `/bin/cat` exec, which
takes no external input and exists to prove descriptor closure across exec.

The first exact-proof request refused before lint or tests because it inherited
a root-owned scheduler scope. The prescribed explicit retry followed by the
proof user's own delegated manager scope repaired only that prerequisite. The
fresh policy-5 proof then passes all 215 lint gates and all 29 impact-selected
groups cold, with zero failures, skips, reused results, unobserved cases or
load-flaky outcomes. Lint takes 198.195s, tests 206.769s and the receipt records
459.389s total. The private-RAM generation retires normally and reports
`receipt_reused=false`; receipt SHA256 is
`a280b4ab0a92a8c27d7b2296a69a2979efc7d0f3cccb1a3be5a04094f4f97e04`.

Local artifacts `node-recovery-lock.bundle`, `node-recovery-lock.mbox`,
`node-recovery-lock.receipt`, exact phase/lint/test/status evidence,
`pr-node-recovery-lock.md`, `node-recovery-lock-qualification.txt` and the
47-entry `node-recovery-lock-final.sha256` manifest verify in the candidate
worktree's ignored evidence directory. Manifest SHA256 is
`2e93d85faac2ef031ed3952d7753abbfaa6b62e96f633a827ca66253105a1c8d`.
No external publication or reviewer-request action was attempted.

### Public-key ownership verification failure-path candidate prepared

Branch `agent/upstream-wallet-pubkey-verify-failure-20261005`, signed head
`fcb496d3667751e61d3ab66fc93ffada6d7df441`, applies directly to upstream
base `3a93e60ebf922af3d119b9facc1d95803f42844b`. The wallet ownership check
previously ignored a signing refusal and could pass partially initialized
signature bytes to public verification. A random provider that partially
filled its challenge before refusing also left those bytes live on the stack.
The fix fails closed for both providers and retires the challenge, SHA state,
verification hash and signature scratch at their last use. Successful
verification semantics are unchanged. Consensus, monetary policy, PoW,
upgrade rules, serialization, and transparent and shielded validity are
untouched. Four files change, +209/-9, including generated inventory and an
actual-source deterministic observer.

Canonical signing-refusal RED and random-provider-failure RED both fail on the
prior production behavior. Removing the fail-closed signing branch or the
pre-verification challenge wipe independently makes the focused regression
fail. The restored candidate passes the focused group and ASan/UBSan/LSan with
zero skips. Uncached GCC 14 and Clang 20 each accept all 2,433 translation
units with no new diagnostics. MinGW checks all 2,399 Windows-visible
translation units and links 74 strict C23 programs. Architecture, generated
inventory and cap-15 complexity gates pass; the final complexity scan covers
68,763 functions. The already-run documentation gates cover 612 Markdown
documents and 152 bound claims.

Local review artifacts `wallet-pubkey-verify-failure-fcb496d36.bundle`,
`wallet-pubkey-verify-failure-fcb496d36.mbox`, the focused RED/GREEN,
mutation, sanitizer and compiler logs, the C hazard review, and
`wallet-pubkey-verify-failure-qualification.txt` are preserved in the
candidate's ignored evidence directory. Bundle SHA256 is
`3e37f31fbeaca8691bf24feb4335a1fd43d1a217bea4b94eb2045f99b9786c35`;
mbox SHA256 is
`05983f5e17a73ed5dd0a54f8b89538095dc01f816ea6d3110910e05d74ff940f`.
The commit contains an SSH signature object; identity verification is
unobserved because this checkout has no configured allowed-signers file, and
that trust policy was not changed.

The final cold exact-proof attempt passes all 215 lint gates (215.587s) and
1,235 of 1,236 selected runtime groups, with zero skips, cache hits, unobserved
cases or load-flaky outcomes. Its sole failure is `test_reflex_runner`: the
fresh proof generation omitted every declared reflex fixture image, so the
group refused them as missing before exercising candidate behavior. The same
focused group passes 1/1 with zero skips under the same qualified user-manager
and private-tmpfs wrapper when the standard build has materialized those
fixtures. An earlier cold attempt executed `test_reflex_runner` successfully
and failed only the then-stale Tor provenance/runtime control; rebuilding Tor
locally from the already-present vendored source made that focused control
pass. No test or proof policy was changed.

These complementary attempts are not combined into a PASS receipt. Exact
independent dev proof remains open until the cold proof generation owns its
reflex fixture prerequisite. The failed attempt status, phases, all-lint PASS,
runtime log and failure extraction are preserved with a manifest in the
candidate evidence directory. Their SHA256 values are respectively
`0d2ee81122b1ea0eead364b647e1747ff104f3e9fcf0790b3952d47924fd3488`,
`92232d592fe80703318f8c84e1c72fc643e875308b030742e9ca80b234dae069`,
`f8f9898fef14ed477030bcc56ce83acdc3c6b490a35a6380930789d1395bbecf`,
`a4b7f8c21e57379865919b2403559ec9ecdae5f7ac37c8757b013718917e9642`
and `9e009e3f4bf1279e9b7164d07c87b2d582675bb7ad2733677a9570cb8aa7adb3`.
The candidate is preserved and no external publication action was attempted.

### Whole-file descriptor candidate exact qualification complete

Branch `agent/upstream-file-io-cloexec-20261004`, signed head
`18b366fbb00c1f111359ec3ff05944741f67f504`, extends the independently
qualified sensitive-reader descriptor work through the shared whole-file
binary and text readers. Both acquire their streams through the portable
no-inherit owner; successful bytes and caller ownership remain unchanged.

The first exact proof passed all 215 lint gates but its runtime environment was
invalid in two independently reproduced ways: a stale root-owned long-path
fixture blocked the zcode package test, and `TMPDIR` was incorrectly placed
inside the proof user's HOME, which the development-land test correctly
refused. The stale fixture was moved intact to quarantine. With
`TMPDIR=/dev/shm/worldstreamproof-z23`, the two unchanged focused controls pass;
their log SHA256 values are respectively
`84926c52dcf7992f9df2f46cc1988220ce1d778b33729ead1e22291ca8872730`
and `52b5b6e4e115d91294583e33d98e313c53764f9b929b6173622f27eb0a17300e`.
No test, assertion or proof policy changed.

The documented retry then produced a fresh exact PASS against upstream
`3a93e60ebf922af3d119b9facc1d95803f42844b`: all 215 lint gates pass
(199.701s native lint step) and all 1,236 selected runtime groups pass
(551.087s body), with zero failures, skips, unobserved or load-flaky outcomes.
The foreground step returned after 756.383s. Native status independently
re-verifies the exact pair. Receipt SHA256 is
`bb9cd6741e6fe6523164ce8b21613b12a7da6af97fb1e3b84ee5607c9795f29a`.
The preserved proof checkout is
`/home/worldstreamproof/z23-wallet-file-io-cloexec-proof-20261004`.

Local review artifacts `file-io-cloexec-18b366f.bundle`,
`file-io-cloexec-18b366f.mbox`, `file-io-cloexec-18b366f.receipt`, exact
evidence, `pr-file-io-cloexec.md`, `file-io-cloexec-qualification.txt` and
`file-io-cloexec-18b366f-final.sha256` verify in the candidate's ignored
evidence directory. No external publication action was attempted.

### Whole-file short-read retirement exact qualification complete

Branch `agent/upstream-file-io-short-read-retirement-20261004`, signed head
`1f8deed4ee759e04ef9c96007e2d2d8ed0a67e98`, follows the descriptor
candidate with a separate lowest-owner fix. A short `fread` previously freed a
partially initialized binary or text allocation without retiring its bytes;
the binary reader is used for wallet-backup plaintext. The fix wipes the
complete owned allocation (`n` or the already-checked `n + 1`) immediately
before the existing free. Output initialization, close order, logging,
successful reads and caller ownership are unchanged. Three files change,
109 additions and 22 deletions, including the deterministic actual-source
observer and regenerated inventory.

Canonical RED fails the two injected 3-of-4-byte cleanup assertions. Final
focused and ASan/UBSan/LSan runs pass with zero skips; removing both wipes makes
exactly the new retirement assertion fail. Strict GCC 14 and Clang 20 each
accept all 2,434 translation units with no new diagnostics. The MinGW lane
cross-compiles all 2,400 Windows-visible translation units and links 74 strict
C23 programs; native Windows runtime remains unobserved. Architecture,
documentation-count/claim, generated-inventory and cap-15 complexity gates
pass; the complexity scan covers 68,764 functions.

A root-identity full-lint attempt is retained as a non-PASS environment record:
permission-negative selftests cannot prove unreadability under UID 0, and that
submitting checkout carries pre-existing hard-linked build outputs and stale
Tor archives. No gate was weakened or reclassified as product evidence. The
authoritative unprivileged exact proof required the three documented
generated-doc checker targets before any lint or runtime test began. After
building only those named prerequisites and using the required explicit
retry-to-step route, the cold proof passes all 215 lint gates (199.971s) and all
1,236 selected runtime groups (561.788s), with zero failures, skips, cached
groups, unobserved or load-flaky outcomes. The foreground step returns after
736.929s and independently queried native status reports PASS with
`receipt_reused=false`. Receipt SHA256 is
`af3fbbe12ac3d2bb4ee12112d3f7e9b28ac34497397a5970e0b135ef6099bd6c`.
The preserved proof checkout is
`/home/worldstreamproof/z23-wallet-file-io-short-read-proof-20261004`.

Local review artifacts `file-io-short-read-retirement-1f8deed.bundle`,
`file-io-short-read-retirement-1f8deed.mbox`,
`file-io-short-read-retirement-1f8deed.receipt`, exact phase/lint/test/status
evidence, the C safety review, `pr-file-io-short-read-retirement.md`,
`file-io-short-read-retirement-qualification.txt` and the 21-entry
`file-io-short-read-retirement-1f8deed-final.sha256` manifest verify in the
candidate's ignored evidence directory. Manifest SHA256 is
`a0e967125cfa5c512f291815f7d5c08cc0291fbc43ca05710d4fa43f21513d50`.
This candidate is stacked on descriptor candidate `18b366f...` and is prepared
for retargeting to `z23c/z23:main` after that prerequisite is present. No
external publication action was attempted.

### Standalone whole-file short-read retirement exact qualification complete

Branch `agent/upstream-file-io-short-read-standalone-20261005`, signed head
`57f0d83781793ebb8fb4e6ab360e1187b318d7f7`, applies the short-read cleanup
directly to upstream base `3a93e60ebf922af3d119b9facc1d95803f42844b`.
It has no descriptor-candidate prerequisite. On binary or text `fread`
shortfall, the shared platform-neutral C23 readers wipe their entire live
owned allocations (`n` or the already overflow-checked `n + 1`) before the
existing free. Output initialization, close order, logging, successful bytes,
caller ownership, wallet formats, consensus, monetary policy, PoW, upgrade
rules and transparent and shielded validity are unchanged. The review is four
files, +134/-5, including the generated capability inventory and deterministic
actual-source observer.

Canonical RED on the prior source fails the injected short-read retirement
assertion. Removing only both wipes from the final source reproduces that
failure, while restoring production source SHA256
`6164e3335d13c0b634abb579631a97528f48f3b28830ea940d25cb7b2163b8c4`
makes focused and ASan/UBSan/LSan lanes pass with zero skips. Uncached GCC 14
checks all 2,433 translation units with no diagnostics; Clang 20 checks the
same set with eight existing diagnostic sites and no new ones. MinGW checks
all 2,399 Windows-visible translation units and links 74 strict C23 programs;
native Windows runtime remains unobserved. Architecture, generated inventory,
80-package anatomy, 152 documentation claims and cap-15 complexity gates pass;
the complexity scan covers 68,761 functions.

The first proof request refused before lint or tests because it was outside a
qualified user-manager memory scope. The second reached exact-root and plan
preparation but refused the pre-existing root-owned `/dev/shm/z23p` pool. The
documented retry ran in the proof user's manager scope with that user's
existing private RAM root; neither prerequisite refusal changed a gate or
assertion. The final cold proof passes all 215 lint gates (197.325s proof
phase) and all 1,236 executed runtime groups (549.377s proof step), with five
runner-gated groups and zero failures, skips, cache hits, unobserved cases or
load-flaky outcomes. The foreground proof completes in 722.085s with no
eligible donor and `receipt_reused=false`. A root-identity status diagnostic
correctly rejects the proof account's signer; same-user native status then
independently verifies PASS without changing any signer allow-list. Receipt
SHA256 is
`8d045796e347a051f7874d7eb3a0cde1e78cc42bfd875cdbc29fbb7ada29d007`.
The preserved proof checkout is
`/home/worldstreamproof/z23-wallet-file-io-short-read-standalone-proof-full-20261005`.

Local review artifacts `file-io-short-read-standalone-57f0d8378.bundle`,
`file-io-short-read-standalone-57f0d8378.mbox`,
`file-io-short-read-standalone-proof.receipt`, exact proof evidence, the C
safety review, `pr-file-io-short-read-standalone.md`,
`file-io-short-read-standalone-qualification.txt` and the 33-entry
`file-io-short-read-standalone-57f0d8378-final.sha256` manifest verify in the
candidate's ignored evidence directory. Manifest SHA256 is
`2f75060f996033577caf466ba9bfad78b3dc3c11a971e2531cc65d37af40cc6c`.
No external publication or reviewer-request action was attempted.

### Isolated fleet-gateway proof fixture qualified

Branch `agent/upstream-fleet-gateway-direct-fixture-20261006`, signed head
`4e0055659de8ef3140b73442202652500add9696`, is based directly on upstream
`3a93e60ebf922af3d119b9facc1d95803f42844b`. The canonical queue tests already
forced direct fake workers because systemd-run is deployment behavior, but the
fleet-gateway fixture did not. Under isolated exact proof, later gateway shards
therefore detached their fake workers and never received fixture receipts. The
fixture now saves the caller's `ZCL_QUEUE_DIRECT`, forces direct workers for its
lifetime, and restores or unsets the value on teardown and setup failure.
Production queue and systemd behavior are unchanged. Two files change, +23/-1.

The seven-group gateway family passes 7/7 with zero skips. Changed-path
ASan/UBSan/LSan shards 03 through 06 pass. The complete sanitizer family is not
claimed: shard 01 deliberately applies an approximately 3 MiB address-space
limit that cannot map the ASan runtime. Architecture covers five authorities,
six contexts and 63 modules; documentation counts cover 612 Markdown files and
152 bound claims; package anatomy covers 80 packages; generated inventory and
the cap-15 complexity scan over 68,754 functions pass.

The first complete proof attempt passed all 23 selected runtime groups but its
six lint failures shared one invalid invocation input: a process-wide `CC=gcc`
export became the literal multiword zcc wrapper pathname inside shell gates.
With `CC` unset, the unchanged checkout passes Tor provenance and the prescribed
retry produces the authoritative cold PASS: all 215 lint gates in 187.084s and
23/23 selected groups in 152.969s, with zero skips, cache hits, unobserved cases
or load-flaky outcomes. The foreground proof takes 366.067s, has no eligible
donor and records `receipt_reused=false`. Receipt SHA256 is
`fde1bbe354ff5ac6fcdbcb7f47d2407a0920f23b492e3d1b8caa7a4174ed1fe4`.
The preserved proof checkout is
`/home/worldstreamproof/z23-fleet-gateway-direct-fixture-proof2-20261006`.

Local bundle, mbox, focused/sanitizer/gate evidence, qualification and PR draft
verify in the candidate's ignored `.cache/wallet-publication/` directory. Bundle
SHA256 is `ddb29b880996f7bb85190eb4f146c810894d6b82da30e9352984b7b5ee618afd`;
mbox SHA256 is `1295213442fdff3041bffb928dac5507930deffa11f08ffc7cd0c14d3b6e321c`.
No external publication or reviewer-request action was attempted.

### Compact wallet public-key verification candidate qualified

Branch `agent/upstream-wallet-pubkey-verify-fleet-qualified-20261006`, signed
head `11df5a3f134d03ea8cbd164ea2d064b0ab8a5ab4`, stacks the independently
qualified fleet fixture above on upstream base
`3a93e60ebf922af3d119b9facc1d95803f42844b`. Its wallet-only review is three
files, +196/-7. The wallet ownership check now fails closed when challenge
creation or signing refuses, and retires its challenge, SHA context,
verification hash and signature scratch at their final uses. Successful
verification, public-key encoding, derivation, consensus, monetary policy, PoW,
upgrade rules, transparent validity and shielded validity are unchanged.

The canonical deterministic RED observations reproduce the ignored signing
refusal and partially written random challenge. Removing the fail-closed signing
branch makes the regression fail; restoring it passes. The registered wallet
key-derivation group passes 1/1 with zero skips, and the identical wallet source
and test pass ASan/UBSan/LSan 1/1. Strict GCC 14 and Clang 20 translation-unit
lanes pass. MinGW checks every Windows-visible translation unit and links the
strict C23 acceptance programs; native Windows runtime remains unobserved.

The fresh unprivileged exact proof passes all 215 lint gates in 190.999s and all
317 impact-selected groups in 306.119s. It is cold and records zero failures,
skips, cache hits, unobserved cases or load-flaky outcomes, no eligible donor and
`receipt_reused=false`; foreground time is 501.408s. Receipt SHA256 is
`59f52ba2415859d25550c584c0f81bb64e09fd6ee294906d50783e3be63ec79e`.
The preserved proof checkout is
`/home/worldstreamproof/z23-wallet-pubkey-fleet-proof-20261006`.

Local bundle, wallet-only mbox, exact receipt/logs, qualification and PR draft
verify in the candidate's ignored `.cache/wallet-publication/` directory. Bundle
SHA256 is `7b9e81fdf4e0d5ca6ed6d5525d72160cac435cdac779b2828dee0430d44bb41d`;
wallet-only mbox SHA256 is
`fdbeda32fbc0eb1f8e87c6d62d13194862c1cf51a61f9d1202f68b4f89ac9c0d`.
The narrow generated-inventory target unexpectedly entered the repository's
parse-time vendor bootstrap and attempted pinned zlib/SQLite acquisition; that
build was terminated and not retried. Subsequent work used verified local donor
artifacts only. No external publication or reviewer-request action was attempted.
