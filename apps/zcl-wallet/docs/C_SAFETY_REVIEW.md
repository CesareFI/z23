# C parser foundation safety review

Scope: authored `native/src/` and `native/include/` amount, Base58Check,
transparent-address, payment-URI and JNI adapter code. This record must be
updated before subsequent C implementation commits. It is a source review and
bounded test record, not a claim that C is memory-safe or that custody is ready.

| Required hazard | Explicit review and control |
| --- | --- |
| Buffer overflow and underflow | Every buffer parameter carries length/capacity; caller storage is never written until capacity validation. No sentinel-length parsing. Reverse loops check their index before subtracting. Tests exercise canaries and undersized output. |
| Out-of-bounds reads/writes | Entry lengths capped before reads (17/184/35/1024 bytes); URI and UTF-8 advancement checks remaining bytes; copy lengths derive from validated bounds. ASan and malformed-input tests cover these paths. |
| Integer overflow/underflow | Decimal accumulation checks `(limit-digit)/10` before multiplication; amounts bounded before addition/subtraction; decode carry fits 32 bits; length subtraction follows ordering checks. SIZE_MAX/UINT64_MAX rejection tests execute. |
| Signed/unsigned conversion | Strict conversion warnings; JNI negative jsize/jlong rejected before casts; enum input validated; monetary results below INT64_MAX; byte/carry narrowing bounded explicitly. |
| Use-after-free, double-free and memory leaks | Authored core allocates/frees no heap memory, retains no borrowed pointer and exposes no native handles. JNI references remain invocation-local; JVM owns returned arrays. No pinned Java arrays. Leak checking runs with ASan. |
| NULL dereferences | Public entry points and JNI helpers validate pointer arguments before reads. Private helpers receive validated spans/local scalar outputs. Null cases are covered in C tests. JNI accepts only a valid VM-supplied environment. |
| Uninitialized memory | Local buffers/structures initialize to zero; variable results are read only after checked success. Native structs/padding are never serialized. GCC analyzer complements UBSan; neither is claimed to replace an uninitialized-memory detector. |
| Dangling pointers and invalid pointer arithmetic | No pointer escapes a call. Offset arithmetic follows `remaining` and capacity checks; at most a valid one-past pointer is formed for zero-byte copies. Borrowed input and output overlap is prohibited in the API contract. |
| Format strings | No formatted output in authored production core. Test diagnostics use fixed strings and line numbers, never scanned data. Forbidden unbounded format/scan calls are checked mechanically. |
| Stack exhaustion | No recursion or variable-length arrays; functions compile with a 4096-byte frame warning as error. Largest JNI input buffer is 1024 bytes; call graph is finite. This is a per-frame guard, not a universal device stack guarantee. |
| Excessive allocation | No authored C allocator calls. JNI checks input lengths before copying into bounded stack storage; output arrays have protocol-specific maximum sizes. Managed text encoding is capped before native entry. |
| Malformed serialization | Exact address prefixes/checksums/payload size; explicit JNI version and field lengths; no native layout serialization. Wrong-network, invalid-type and truncation tests reject input. |
| Malformed network input | Network transport is not implemented yet. Its future data is untrusted; the current parsers are bounded and tested independently. A URI scan cannot authorize a payment. |
| Race conditions | No mutable global core state, retained references, shared caches or background tasks. Constants are read-only; each call owns its scratch buffers. Future wallet/network state requires a separate concurrency review. |
| Resource exhaustion | Fixed input/output limits, at most three payment fields, bounded Base58 work, no recursive decode, no input-controlled allocation or subprocesses. Fuzz workers have explicit time/RSS/input limits. |
| Secret leakage | This slice processes public addresses/amounts/requests only and has no key generation/import/storage/signing API. No core diagnostic includes caller data. Public codec interfaces explicitly prohibit secret use until zeroization/ownership paths are separately reviewed. |

`tools/check-c-safety.sh` checks banned calls, pinned vendor file hashes, Clang
and GCC analysis, cyclomatic complexity (maximum 10, nonempty source count),
strict compilation and sanitizer tests. JVM tests use `-Xcheck:jni` and include
native source files in their Gradle inputs so a C change invalidates test results.

The known-answer, public fixture and fuzz results are recorded in
[PROGRESS.md](PROGRESS.md). Upstream hash sources retain their own implementation
and self-tests under the repository's third-party `vendor/` boundary. They are
not represented as authored low-complexity functions. Their copied bytes are
checked against the pinned release and tested with sanitizer instrumentation.

Unproven at this milestone: key custody, zeroization of private data, device
Keystore behavior, mobile UI lifecycle, network synchronization, transaction
construction/signing, full chain validation and shielded compatibility.

## C recovery and derivation review — 2026-09-11

Scope now additionally includes `secret_hash`, `mnemonic*`, `bip32`,
`ec_context`, `random`, `receive_key` and `jni_keys`. The parser review above
remains the record of its earlier checkpoint; its no-secret/no-allocation
statements describe that earlier scope, not the new key implementation.

| Required hazard | Explicit review of the new implementation |
| --- | --- |
| Buffer overflow/underflow and out-of-bounds access | Entropy 16..32, mnemonic 215, seed 64, salt 140 and public key 33 bytes are fixed bounds. Word bit offsets stay within 264 bits. HMAC caps key/data at 256/512. JNI caps UTF-16 input before copying and rejects non-ASCII. Undersized outputs and canaries are tested. |
| Integer overflow/underflow and signed/unsigned conversion | Entropy/word counts are validated before multiplication; all shifts are unsigned and bounded. BIP32 indices are uint32 with explicit big-endian serialization; receiving indices stay below the hardened boundary. JNI rejects negative indices/counts before conversion. RNG checks ssize_t for negativity before casting and verifies returned count against remaining bytes. |
| Use-after-free, double-free, leaks and dangling pointers | Each EC operation owns one context allocation. The owner object is initialized empty, never copied while active, and always reaches `zcl_ec_end`. Provider destruction precedes clearing and the single free; owner fields are then cleared. No native handle escapes JNI. Interposition tests observe balanced allocations/releases and verify cleared bytes before free, including failed creation/randomization. |
| NULL dereferences | Public arguments, allocation returns, context creation and JNI array creation are checked. Private helpers receive already validated pointers and explicit lengths. Provider failures are propagated; no secret output is published on failure. |
| Uninitialized memory | Secret scratch arrays/nodes start zeroed. The provider initializes its context allocation before use. Failed provider creation leaves allocated storage unread until it is explicitly cleared and freed. Hash init/free manage provider contexts. GCC and Clang analyzers cover authored core and JNI. |
| Invalid pointer arithmetic and effective type | Span arithmetic follows validated fixed bounds. The opaque EC context uses checked malloc storage with suitable alignment and no declared effective type, avoiding a cast from a declared character array into an opaque struct. Only the provider accesses that storage during its context lifetime. |
| Format-string vulnerabilities | Production key code has no formatted output. Test failure diagnostics use fixed format strings and line numbers. Newly generated test entropy is not included in assertion messages. |
| Stack exhaustion and excessive allocation | No recursion/VLAs. Authored frames remain below the 4096-byte compiler gate. Mnemonic/hash code allocates no heap. EC context allocation is checked nonzero and at most 1024 bytes, with no allocation-size arithmetic; the tested provider requests 216 bytes. Allocation failure is tested and returned as a resource error. |
| Malformed serialization and network input | Strict canonical English words/checksums, exact key sizes, standard BIP32 child data, valid scalars and network-specific paths. Zero master keys, tweaks at the group order, zero resulting children and valid zero tweaks are tested. Invalid children return an explicit error without changing the requested index. Networking is still absent and cannot invoke key operations. |
| Race conditions | No mutable global wallet/context state. Each context, secret span and JNI reference belongs to one invocation. Word tables/provider constants are read-only. Android ownership and lifecycle synchronization remain a separate acceptance requirement. |
| Resource exhaustion | RNG uses nonblocking OS calls with at most 128 attempts; short reads, EAGAIN, interruptions and impossible counts are tested. PBKDF2 has exactly 2048 iterations and a bounded passphrase. No recursive derivation or input-controlled allocation count. The receiving path has five fixed levels. |
| Secret leakage | Native secret scratch is cleared on success and failure; the caller retains responsibility for its own inputs/outputs. Fault tests instrument clearing and late KDF failures. The EC context uses independent OS-random blinding in JNI. Provider secret operations pass the upstream Valgrind constant-time check on the measured host. This does not prove all ARM64 timing behavior, eliminate register copies, or make mnemonic dictionary lookup constant-time. Managed UI/IME/provider/GC copies cannot all be erased. No secrets are logged, persisted or sent by this slice. |

Compatibility evidence: 24 published BIP39 fixtures, 17 published BIP32 paths
including leading-zero cases, RFC 4231 HMAC cases, 48 fully independent OpenSSL
seed/HD/public-key/hash/Base58Check address comparisons, provider-failure tests,
and the provider's separate 123-case suite. The latter uses the same portable
arithmetic/table configuration; unused modules stay disabled. The release
signature and exact copied provider bytes were verified. None of this is a
device custody or transaction authorization claim.

Still unproven: authenticated Android storage, device lifecycle/recovery,
network synchronization, transaction signing/broadcast and shielded handling.
These remain unavailable in the launcher pending their respective acceptance.

## C wallet record and storage review — 2026-09-11

Scope: `network`, `wallet_header`, `wallet_record`, `storage_*`,
`jni_wallet_record`, `jni_storage`, and their thin managed adapters. The
following review precedes this slice's implementation commit. It does not
promote the placeholder launcher to a custody-ready wallet.

| Required hazard | Explicit review of this slice |
| --- | --- |
| Buffer overflow/underflow | Fixed 80-byte header, 12-byte IV, 16-byte tag and 124..140-byte record. All input spans/capacities are explicit. Outputs publish only after successful validation and cleanup. JNI caps input before copying; read packets are at most 142 bytes. Canary and undersized-output tests exercise the boundaries. |
| Out-of-bounds reads/writes | Header offsets follow an exact 80-byte check. Ciphertext length derives from validated entropy sizes. File size is checked before conversion/read and EOF checked afterward. Paths are capped at 1024 bytes before inspecting components; the 1025-byte POSIX conversion has its own terminating byte. No input controls an unchecked offset. |
| Integer overflow/underflow | Record arithmetic adds only fixed validated bounds (maximum 140); path loops cannot approach SIZE_MAX. Read/write offsets advance only by positive counts no larger than remaining length. All subtraction follows ordering checks. SIZE_MAX entry lengths and impossible syscall counts are rejected in tests. |
| Signed/unsigned conversion | Negative stat/read/write/JNI values are rejected before conversion. Network and entropy narrowing follows validation. JNI status fits a byte and the managed adapter decodes it unsigned. Strict conversion warnings remain errors. |
| Use-after-free | No new native heap allocation or retained native handle. Each descriptor belongs to one invocation. Ownership is transferred immediately after open and consumed by a single cleanup path. No descriptor is used after close. |
| Double-free and leaks | No new C allocator/free calls. Directory/lock/record descriptors each close exactly once on every path; failed close is never retried on Linux/Android. JNI local references are VM-scoped and returned arrays VM-owned. Fault injection covers failed closes; Valgrind checks normal exits for only standard descriptors and no leaked allocations. Process-termination tests intentionally rely on OS process cleanup. |
| NULL dereferences | Public pointers and JNI arrays are checked before reading. Private helpers receive validated pointers and bounded stack storage. Every open/read/write/stat/lock/sync/rename and JNI creation result is checked. Pending Java exceptions stop adapter work; no Java array is pinned. |
| Uninitialized memory | Structures, path/text buffers and IO scratch initialize before use. Returned lengths are used only after successful operations. Padding is never serialized. Valgrind origin tracking complements both static analyzers and sanitizers. |
| Dangling pointers | No pointer or descriptor escapes a call. The Android caller owns managed arrays and must not mutate them concurrently. JNI snapshots each bounded input; result arrays are newly allocated and have no borrowed native backing. |
| Invalid pointer arithmetic | All offsets derive from validated fixed lengths; path component checks can form only a valid one-past pointer for an empty span and reject that span before dereference. No pointer casting into serialized structs, arithmetic on void pointers, or pointer-based ownership tricks. |
| Format-string vulnerabilities | No production formatted output. Error statuses contain no caller data. Test diagnostics use fixed strings and test line numbers; no key, phrase, record or provider exception text is logged by the core. |
| Stack exhaustion | No recursion or VLAs. All authored C/JNI functions retain the 4096-byte frame gate. Largest new frame contains a 1024-byte path plus small bounded record scratch; nested calls have a fixed call graph. Per-frame checks are not a proof for every possible device stack. |
| Excessive allocation | The new C record/storage code allocates no heap. Existing checked EC ownership handles header/recovery derivation. Managed path encoding is bounded before conversion; JNI outputs are bounded record parts or a 142-byte packet. No file-sized allocation or memory map is used. |
| Malformed serialization | Exact length/version/profile/network/genesis/P2PKH fields and reserved zeros are checked. Native structure layout is never persisted. Parsing is explicitly unauthenticated; Android must verify GCM and then C must re-derive the receiving address. Header edits, extra/truncated bytes and mismatched recovery are tested. |
| Malformed network input | No network transport is introduced. Storage paths must come from Android private storage, never network/QR input. Record parsers remain bounded even if files are attacker-modified. Networking must not acquire key or storage-write authority in later phases. |
| Race conditions | Descriptor-relative operations, exclusive pending creation, nonblocking flock and kernel `RENAME_NOREPLACE` protect cooperative writers and prevent destination overwrite. A test creates a conflicting destination between the absence check and rename. Twelve competing processes must yield exactly one creator. Trusted ancestors and an uncompromised app UID remain assumptions; GCM authentication is still required. |
| Resource exhaustion | File reads/writes have 256-attempt caps; EOF and fsync retries have 16-attempt caps. FIFO/device/symlink files are rejected and opens/locks are nonblocking where relevant. There are no unbounded queues, recursive traversal or attacker-sized allocations. Kernel filesystem latency itself is not bounded by retry counts, so storage must execute off the UI thread. |
| Secret leakage | Only public metadata and ciphertext enter storage. Header/recovery JNI clears entropy and independent blinding scratch on every path; callers clear managed entropy. The stored address is visible metadata but cannot be displayed as a verified receiving address before GCM authentication and re-derivation. No clipboard, log, network, backup, secret-string or funded-wallet behavior is added. |

Crash durability was separately reviewed: persist the parent directory, pending
file contents and pending entry before atomic non-replacing rename; then flush
the directory before acknowledging success. Recovery re-flushes the actual file
and compares the exact authenticated bytes under the lock. Interrupted writes
remain an explicit recovery condition and cannot trigger silent replacement.
An existing corrupt committed record never falls back to pending data. This
does not prove every device filesystem's power-loss behavior or protect against
rollback of an older valid record by a filesystem-controlling attacker.

An emulator test exposed `EACCES` on hard-link creation in the original candidate.
The implementation now uses the NDK's API-30 `renameat2` interface with
`RENAME_NOREPLACE`, with no replacing-rename fallback. The affected crash, fault,
native/JNI, Valgrind and Android checks pass on this revised implementation.
The API-35 emulator observed successful native storage and provider GCM/AAD
rejection tests; hardware custody remains unqualified. Final observations are
recorded in `PROGRESS.md`.

## C custody policy and backup confirmation review — 2026-09-11

Scope: `custody_policy`, `jni_custody`, `mnemonic_confirm`, the new confirmation
entry in `jni_keys`, and their tests. This review precedes the implementation
commit. Platform workflow/device acceptance is tracked separately and remains
in progress; this record does not assert hardware authenticity or complete
wallet readiness.

| Required hazard | Explicit review of this slice |
| --- | --- |
| Buffer overflow | Confirmation accepts explicit entropy/text lengths. Entropy is one of the five validated 16..32-byte lengths; the existing decoder bounds text to 215 bytes. JNI copies into 32/215-byte arrays only after size checks. No unchecked buffer write is added. |
| Buffer underflow | No reverse indexing or decrementing buffer offset is added. The confirmation loop starts at zero and advances only while below validated entropy length. |
| Out-of-bounds reads/writes | Decoded entropy is a zero-initialized 32-byte local. Its returned length must equal the validated input length before comparison. SIZE_MAX, empty/null input, mismatched lengths, and every single-bit entropy mutation are tested. Policy fields are read from a fixed typed struct, not cast from bytes. |
| Integer overflow/underflow | No allocation arithmetic, monetary arithmetic, or unbounded count multiplication. Comparison has at most 32 iterations; byte XOR/OR promotions cannot exceed 255. Policy evaluates fixed scalars and flags. |
| Signed/unsigned conversion | JNI rejects negative key sizes, hardware/flag/method values before uint32 conversion. Authentication duration remains signed int32 so only the documented/provider -1 and 0 per-use forms can pass. Unknown positive bits, positive timeouts, INT32_MIN and UINT32_MAX are rejected in tests. |
| Use-after-free | No allocation, deallocation, pinned managed array, retained native pointer, or native handle is introduced. Every borrowed input remains invocation-local. |
| Double-free | No new free operation. Native scratch has automatic lifetime and reaches exactly one clearing path after being populated. |
| Memory leaks | No new native heap ownership. JNI confirmation returns a scalar; policy normalization is a small stack struct. Existing decoder scratch is cleared through its previously reviewed cleanup path. |
| NULL dereferences | Public entropy/text/struct pointers are checked before dereference. Existing JNI input helpers check arrays and pending VM exceptions. The JNI environment itself is VM-owned. |
| Uninitialized memory | Recovered entropy, JNI entropy/text, lengths, result, and volatile comparison accumulator initialize before use. Decoder results are read only after success and a length check. Policy aggregate initializes every field explicitly. |
| Dangling pointers | No pointer escapes or outlives its call. Managed callers retain ownership of their input arrays and must not mutate them concurrently. JNI uses local snapshots. |
| Invalid pointer arithmetic | Only indexes in the validated entropy span are used. No serialized struct casts, pointer subtraction, ownership tagging or nonstandard arithmetic. |
| Format-string vulnerabilities | No production formatting or logging in either new C feature. Test messages contain fixed text/line numbers and never an entropy/phrase value. |
| Stack exhaustion | No recursion or VLA. Confirmation adds 32 bytes of secret scratch; JNI adds bounded 32/215-byte snapshots and uses the existing bounded UTF-16 conversion helper. The authored 4096-byte frame gate remains enabled. |
| Excessive allocation | No allocator calls or caller-sized arrays are introduced. The normalized metadata is fixed-size; backup checks cannot cause unbounded allocation. |
| Malformed serialization | The existing canonical English BIP39 checksum/word parser remains authoritative. Confirmation returns only success/failure, never decoded entropy. Metadata accepts exactly the required flags/methods and recognized hardware/per-use values. It is not a wire format or attestation. |
| Malformed network input | No transport or network-to-key path is added. The Android adapter obtains policy fields from actual KeyInfo, not preferences, server data, or scan payloads. Network validation remains a future separate scope. |
| Race conditions | New C functions are pure/local and retain no mutable global state. Platform alias creation has a short shared lock; the bounded single worker owns managed entropy, and cancellation clears queued inputs before finalizing session state. UI callbacks are gated by session closure. This does not prove behavior under a compromised OS. |
| Resource exhaustion | Confirmation performs one bounded decode and at most 32 comparison iterations; no KDF/EC context is needed. Policy performs fixed comparisons. Fuzz execution has time/RSS/input caps. Platform work uses one active worker and a queue of four, with explicit input cleanup on rejection. |
| Secret leakage | Confirmation clears decoded entropy and its volatile mismatch accumulator on all populated paths. JNI clears both snapshots even after copying/decoding failure; provider-fault tests observe cleanup. The byte comparison visits the entire validated span, but dictionary decoding and the whole backup workflow are not claimed constant-time. Caller arrays and platform/view copies have separate owners and documented erasure limits. No secret is returned by confirmation or included in errors. |

The normalized policy cannot authenticate a virtual or compromised platform.
Per-use authentication must bind the exact Android provider cipher; successful
decryption must precede C recovery verification and pending-file promotion.
Those platform requirements are not weakened to make an emulator test pass.

Evidence so far: the unchanged C source snapshot passes the full safety script
(15 native executables, Clang/GCC analysis, warnings/complexity/sanitizers),
native/JNI confirmation tests and 11,088,373 fuzz executions in 901 seconds.
`native/build/fuzz-confirmation/INPUTS.sha256` was checked against current native
sources after the separate beta6 diagnostic task. On-device workflow acceptance
and final application checks must finish before this slice's completion claim.

## Receiving QR review — 2026-09-12, before implementation commit

Scope: `receive_qr.c`, `jni_qr.c`, `zcl_qr.h`, tests and the unchanged pinned
Nayuki C provider. This is a public-address encoder, not a scanner or secret
export API. The provider is reviewed as third-party code; it is not exempted
from sanitizer execution by being outside the authored complexity gate.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow | Input must be exactly 35 bytes and pass the C address checksum/network parser. The provider receives two distinct fixed 138-byte buffers sized for version 4. Expansion uses a 1681-byte local and copies only the checked square into caller capacity. JNI snapshots at most 35 input bytes. |
| Buffer underflow | No decrementing unsigned offsets. The four-module border is added to nonnegative, bounded signed coordinates. Tests exercise undersized output capacities from 0 through 1680. |
| Out-of-bounds reads/writes | The provider's successful width must be 21..33 before expansion. Each index is `(y+4)*width+(x+4)` with x/y below symbol width, and width at most 41. Canaries, invalid input, zero/SIZE_MAX lengths, and every short output capacity are covered. |
| Integer overflow/underflow | All products use widths at most 41; maximum module count is 1681. No allocation arithmetic or caller-controlled version/scale is accepted in C. Provider arithmetic is reviewed for its fixed version/ECC range. |
| Signed/unsigned conversions | Network jint is checked by the existing JNI mapper. Symbol dimensions are range-checked before size_t conversion; coordinate additions are at most 36. The width-to-byte conversion is bounded by 41. No amount or secret scalar conversion. |
| Use-after-free | Neither wrapper nor provider allocates, frees, pins a managed array, or retains a pointer. JNI returns a fresh VM-owned public byte array through the checked existing helper. |
| Double-free | No native free operation. Local arrays end with their invocation. |
| Memory leaks | No native heap ownership or descriptors. The UI holds only immutable public rendering data and a Paint; no Bitmap allocation is needed for rendering. |
| NULL dereferences | Every caller pointer is checked before reading/writing. JNI helpers validate managed arrays and VM exceptions. Only valid local arrays reach provider APIs. |
| Uninitialized memory | All wrapper arrays, lengths and address fields initialize before use. The provider's encode success precedes reads; its ECC/divisor/work arrays are initialized before consumption. Output includes initialized white border and canonical 0/1 modules only. |
| Dangling pointers | No pointers escape. Input/output/side are borrowed for one call; their nonoverlap and lifetime are documented. Java input is snapshotted before native work. |
| Invalid pointer arithmetic | JNI's `record+1` points inside its fixed 1682-byte record. Native indexes are derived only from validated fixed-range coordinates. No pointer casts to serialized structs, ownership tags or subtraction. |
| Format strings | No wrapper/provider logging. Test diagnostics contain fixed labels and public fixture identifiers only. Provider functions never perform I/O. |
| Stack exhaustion | No recursion or VLA. Wrapper scratch is 138+138+1681 bytes plus small scalars; JNI has 35+1682 bytes plus scalars. The 4096-byte authored frame gate remains enabled. Provider stack is fixed and small; version and input lengths cannot increase it. |
| Excessive allocation | No native allocation, hidden native handles, image allocation or caller-sized arrays. The Kotlin rendering record is at most 1682 bytes. The test-only image oracle uses fixed bounded fixture dimensions. |
| Malformed serialization | Only canonical checked transparent addresses for the selected Zclassic network reach the binary QR encoder. No URI, recovery phrase, private key or arbitrary payload interface is exposed. JNI output dimensions and byte values are checked by the public Kotlin view. |
| Malformed network input | No transport added. Arbitrary/oversized bytes and wrong-network addresses are rejected before QR encoding. Future scanner/network data must use separate bounded validation interfaces. |
| Race conditions | Provider constants are read-only in operation, with no mutable runtime state. Each call owns separate stack scratch. Callers must not concurrently mutate borrowed buffers; JNI uses local snapshots. The UI draws an immutable public record. |
| Resource exhaustion | Input length, QR version, ECC work, eight mask candidates and output are fixed-bounded. No retry loop, network operation, recursion or allocation. Fuzzing is bounded by input/time/RSS limits. |
| Secret leakage | The API is restricted to validated public addresses. Secret creation/restoration is not called by QR generation; no recovery QR is supported. Native output/errors contain only public modules/status. No secret material, logging, telemetry or external QR service is introduced. |

The C safety script passes all 16 native executables with Clang/GCC analyzers,
warnings-as-errors, complexity at most 10, ASan/UBSan/LSan. The provider's separate
upstream C test executable passes 521 cases under sanitizers. Host JNI exports
are limited to the 23 intended Java entry points.

Independent ZXing module decoding verifies the exact addresses. Its simple
image detector can select a spurious finder pattern in a valid symbol: a pinned
regression preserves this failure, while its multi-candidate detector must find
exactly one correctly decoded QR at every tested scale/orientation. No failed
image or checksum is treated as a successfully decoded address. This establishes
synthetic interoperability, not universal camera/device acceptance.
