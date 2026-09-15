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

## Receiving-request scanner review — 2026-09-12, before implementation commit

Scope: `scan_qr.c`, `jni_scan.c`, shared JNI payment record serialization,
`zcl_qr.h`, and the locally hardened quirc provider. No camera UI, networking,
wallet import, signing or consensus code is introduced. The source and local
provider differences are pinned and documented separately.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow | C validates dimensions, strides and last-addressed pixel before allocation/read. Dimensions <=1024, pixel stride <=4, row stride <=8192 and input <=8 MiB prove every copied pixel fits. Decoder output length is checked against 1024 before the existing payment parser. JNI output reuses the existing bounded record serializer. |
| Buffer underflow | Width/height >=21 precede subtraction. Row stride must include the last pixel. Provider flood fill checks image borders and its bounded explicit stack before pushing. Extraction indices are checked before forming a grid pointer. |
| Out-of-bounds reads/writes | Provider grid versions are checked before table-address formation and bitmap reads. QR grids are 21..177; raw bits need fewer bytes than the fixed 8896-byte payload buffer. Version/ECC table indices are bounded. Berlekamp-Massey loop invariants keep syndrome indices nonnegative. The adapter only extracts index zero after count==1. |
| Integer overflow/underflow | Frame arithmetic has bounded operands before multiplication. Provider allocations check size_t products before allocating. Line intersections use int64 on image-bounded corners; alignment areas use int64 and must fit the image area. Subsequent area*100 <=104857600 and step squares <=1050625 fit int. Region counts*100 also fit int. |
| Signed/unsigned conversions | JNI rejects negative dimensions/strides before size_t conversion. C proves dimensions <=1024 before int casts. Nonfinite or projected coordinates beyond +/-4096 are rejected before conversion; all callers handle failure. Existing provider signed/unsigned comparisons involve proven nonnegative bounded indices. Finder run lengths cover preceding image pixels, so their subtractions do not wrap. |
| Use-after-free | One invocation owns each decoder, its image/fill buffers and decoded workspace. No context or image pointer escapes. Work is complete before cleanup. JNI uses a snapshot, not a pinned or retained managed array. |
| Double-free | quirc's pixel/image alias case is handled explicitly. Wrapper ownership has one destroy/free path per allocation. Fault injection observes each of the four allocation sites and exactly-one release, including resize failures. |
| Memory leaks | Checked allocation failure returns through the appropriate owner. The provider leaves an existing context unchanged on failed resize and frees partial new allocations. JNI frees its bounded snapshot even when the VM copy, decoder or result allocation fails. |
| NULL dereferences | Public image/layout/output pointers and managed arrays are checked before use. Every allocated pointer is checked. The original first-resize NULL-source memcpy was reproduced under UBSan and is now skipped when its length is zero. NULL optional clearing spans are no-ops. |
| Uninitialized memory | Provider contexts and decode workspace are zero-initialized. New image allocation is zeroed and the full active image is copied before identification. Fields are read only after successful decode/range checks. JNI request state initializes before use; rejected output remains untouched. |
| Dangling pointers | All borrowed spans must remain stable during the call; no native handles or stored callbacks exist. Provider buffers are private to one context. The alias pointer is replaced before image identification and is never freed separately. |
| Invalid pointer arithmetic | The validated last-pixel formula bounds row/pixel offsets. Extraction validates index before taking an array address. Version validation precedes version-table pointer formation. No serialized-struct casts or ownership-tagged pointers are used. |
| Format-string vulnerabilities | No decoder, wrapper or JNI logging. Public fixture tools use fixed format strings and check snprintf/fwrite/fclose results. Decoded payloads are always data, never format strings. |
| Stack exhaustion | No recursion or VLA. Flood fill uses a bounded heap stack. Large code/data results use a fixed heap workspace. The provider decoder has a fixed approximately 9 KiB datastream stack buffer; mirror scratch is fixed approximately 4 KiB. These calls are sequential, and authored wrapper/JNI frames retain the 4096-byte compiler gate. |
| Excessive allocation | Provider image <=1 MiB, fixed context/workspace and fill storage <=682 entries; JNI snapshot <=8 MiB. Constant sizeof allocations cannot overflow, and every variable-size product is checked or proven bounded before allocation. There is no native frame queue/cache. Camera queue bounds remain a required separate adapter gate. |
| Malformed serialization | QR error correction must succeed. Unsupported modes cannot silently truncate a payload. Only supported byte/text encoding reaches the existing exact payment parser; wrong networks, duplicate/unknown fields, invalid amounts/UTF-8 and arbitrary non-request content are refused. Scanned data does not enter wallet record or recovery APIs. |
| Malformed network input | No network operation is added. A selected network is a separate caller parameter and is never inferred from the image. Future network sync has no new trust or validation shortcut. |
| Race conditions | No mutable global provider or wallet scan state. Each call owns its context and scratch; shared tables are const. Concurrent mutation of caller-owned input is outside the API contract. The camera adapter must enforce sole frame ownership and bounded worker dispatch. |
| Resource exhaustion | Per-frame candidate, alignment and fitness budgets are explicit. Exhaustion is reported and causes whole-frame refusal; no partial decode is accepted. Bounds cap remaining table/bitmap loops. Fuzzing has input/RSS/allocation/time limits. No universal real-device latency guarantee is inferred. |
| Secret leakage | There is no secret import/export or spending callback. Images and rejected payloads are never logged, persisted or transmitted. Native snapshots, provider pixels/context/fill allocations, decoded workspace and provider datastream/mirror stack buffers are cleared before release. Managed/frame/runtime copies require their own owners and cannot all be claimed erased. The purpose remains public requests, with no guarantee that camera users never point at private material. |

The original provider's NULL-source copy and nonfinite-to-int conversion fail
local UBSan probes. Named provider regressions cover the fixes, work budgets,
index bounds and unsupported modes. Independent encoder/JNI tests, adversarial
span tests, every-allocation fault injection and full-provider ASan/UBSan/LSan
complement this manual review; they do not constitute complete memory-safety
proof or real-camera acceptance. Final check results are recorded in PROGRESS.

## Camera packet and IPC review — 2026-09-12, before implementation commit

Scope: `camera_frame.c`, `jni_camera.c`, `zcl_camera.h`, the exact-text result
extension in `scan_qr.c`/`zcl_qr.h`, and Android frame/IPC ownership. The pinned
provider is unchanged. No network, key handling, transaction or consensus logic
is modified. Every category below was explicitly checked against the source.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow | Existing image bounds precede sampling. Output dimensions <=384 prove `5+width*height <=147461`; capacity is checked before any write. Payload length <=1024 precedes copying exact decoded text. JNI output uses the checked byte-array helper. |
| Buffer underflow | Input dimensions >=21 precede all subtraction. Step is ceil(max dimension/384), hence 1..3. Packet length >=5 precedes header reads and subtraction; direct capacity subtraction follows offset<=capacity. |
| Out-of-bounds reads/writes | Each sampled coordinate `(dimension-1)/step*step` is below its source dimension. Existing last-pixel bounds prove source indexes. Canonical packet length must match its checked dimensions exactly. Canary, shortened-span/capacity and malformed-header tests run under sanitizers. |
| Integer overflow/underflow | All source dimensions <=1024 and strides <=8192 are validated first. Output sums/products and sampling arithmetic are bounded before use. Packet fields are LE16 converted into size_t. New allocations use a constant maximum or an already checked 5..147461 length. |
| Signed/unsigned conversions | Negative jint dimensions/strides/offset/length are rejected before size_t casts. Direct capacity is nonnegative before range arithmetic. Packet header shifts are size_t and bounded; byte truncation is deliberate encoding of validated <=384 dimensions. |
| Use-after-free | JNI borrows a direct plane only synchronously. CameraCapture keeps Image open until C returns; ImageReader/device closure runs on the same handler. No pointer, Image, pin or native handle escapes. Result text is copied before provider cleanup. |
| Double-free | Each of the two new JNI malloc sites has one invocation owner and one free path. Provider ownership is unchanged. Android queued packets use atomic transfer; closed clients clear only unclaimed pending replies. Images close in one finally block. |
| Memory leaks | Allocation returns are checked; encode/decode/VM failures converge on zero/free. Service close discards queued owned inputs and finalizes active work. Camera release uses nested finally blocks for session/device/reader and thread cleanup. An OS open that never replies can retain at most one camera owner, documented rather than claimed immediately reclaimable. |
| NULL dereferences | Public input/output pointers are checked. Layout validity is delegated to the existing NULL-aware image checker. JNI validates buffers, capacity, addresses, arrays, exceptions and allocations before access. A missing camera/plane/service causes refusal. |
| Uninitialized memory | JNI decoded structs initialize to zero. Image samples fill every packet pixel and all header bytes before return. Only successful parsing populates exact request text/length; failure leaves caller output unchanged. No uninitialized tail is returned to Java. |
| Dangling pointers | All C spans are borrowed for one call, with documented nonoverlap/lifetime. Native output is copied into VM-owned arrays. Camera callbacks check closure and dispose late devices/sessions; managed preview references clear on background/detach. |
| Invalid pointer arithmetic | The direct offset/length must fit the direct-buffer capacity before pointer addition. Source indexes follow validated stride bounds. Packet+5 is formed only after a valid canonical header/length. No serialized-struct casts or pointer tagging. |
| Format-string vulnerabilities | No camera/decoder payload logging. Public request labels/messages are data passed to fixed Android string resources, with no HTML, autolinking, executable routing or format-string interpretation. C diagnostics use fixed test strings only. |
| Stack exhaustion | No recursion/VLA/new caller-sized stack buffer. The exact-text struct adds a bounded 1024-byte member. Authored C/JNI frames pass the <=4096-byte compiler gate. Provider stack bounds remain those of the preceding review. |
| Excessive allocation | JNI owns at most one <=147461-byte packet per call; decoder image is now at most 384*384 in this route. Camera has two bounded platform images, one pending packet and a bounded preview bitmap/scratch. One decoder worker and one outstanding Binder request prevent frame queues. All allocation sizes are proven before allocation, all returns checked. |
| Malformed serialization | App-local packet version, dimensions and exact length are checked by C. It is not a Zclassic wire format. QR payload parsing occurs in isolated C and repeats in app C before display; no server-supplied display record is trusted. Unknown/malformed/wrong-network requests refuse. |
| Malformed network input | No network path is enabled. Non-exported explicit service binding and owning-app UID checks constrain IPC callers. Returned bytes remain untrusted even from that isolated service. No scan-to-key/import/transaction route exists. |
| Race conditions | C has no new mutable globals. Camera resource access is serialized on one handler; close marks cancellation synchronously and posts resource cleanup. Atomic ownership prevents queued-array double cleanup or callbacks after disposal. Client checks request ID, UID, closure and pending ownership; service allows one decode. Caller mutation of borrowed C spans remains outside the documented contract. |
| Resource exhaustion | Camera output <=640*480 pixels; C packet <=147461 bytes; at most four submitted frames/second and one outstanding frame. Startup/reply deadlines bound waiting and require user retry after failure. Provider work budgets still apply. OS driver/VM scheduling and immediate termination of already active work are not falsely claimed bounded by wall time. |
| Secret leakage | Scan has no access to keys or recovery workflows. Arbitrary camera content may still be private: owned queued frames, JNI copies, decoder scratch and preview data are cleared on cleanup. No logs/files/telemetry/network transmission or saved Activity frames. Local Binder transfers bounded camera data to an isolated process without app permissions. Driver/VM/Binder/GPU copies cannot be claimed universally erased. |

Evidence: all 20 native tests, full authored/provider analysis and complexity
gate, 41 JVM/JNI tests, 12070 camera fuzz executions with matching final hashes,
and real emulator Binder/permission/camera lifecycle tests described in
SCANNING_QR.md. No sanitizer result is treated as a complete memory-safety proof.

## Read-only Electrum codec review — 2026-09-12, before implementation commit

Scope: `rpc_json.c`, `rpc_values.c`, `electrum_*.c`, their public/private
headers, pinned JSON/UTF-8 provider use, and public fixture/fuzz tools. No TLS,
socket, JNI, custody or consensus implementation changes are included.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow | Frame <=16384 and request <=256. Decoded output counts are checked against destination capacity before public copies. Header hex <=2974 characters decodes into <=1487 bytes. The line accumulator checks capacity before every non-LF byte. |
| Buffer underflow | Token offsets are checked before subtracting from frame length. Signed-number leading-minus subtraction follows nonempty-token checks. Header byte accesses follow exact length checks. Reverse hash indexing uses i<32. |
| Out-of-bounds reads/writes | Provider events must lie within the caller span before narrowing. Index count <=128; nesting stack <=8; container next indices are validated. Private member/child helpers only receive tokens belonging to a successfully parsed document. UTF-8 decoder checks sequence length before continuation-byte access. |
| Integer overflow/underflow | Frame/token/string counts are bounded before arithmetic. Integer magnitude checks `(limit-digit)/10` before multiply/add; limit is INT64_MAX or INT64_MAX+1, both >=9. Money operands are each bounded to +/-2.1e15 before summing. Request formatting checks negative/truncated snprintf returns. |
| Signed/unsigned conversions | IDs are uint32 and compare exactly in int64. Magnitudes cast to int64 only when <=INT64_MAX; INT64_MIN has a separate branch. Heights are checked 0..INT32_MAX before uint32 conversion. Hex nibbles are nonnegative before byte encoding. Offsets <=16384 precede uint16 casts. |
| Use-after-free | No allocator or freed object is involved in the codec. Invocation-owned token indexes borrow a stable frame only for the synchronous call. No token/frame pointer is retained. |
| Double-free | No malloc/calloc/realloc/free calls in the new protocol path or reused lexer/UTF-8 path. Fixture FILE ownership has exactly one fclose after a successful fopen. |
| Memory leaks | Fixed caller/stack storage requires no cleanup allocation path. Every fixture stream closes after write success or failure. Future TLS/socket resources require a separate ownership review. |
| NULL dereferences | Public frame/output/length arguments are checked before use. Missing typed members refuse through NULL-aware accessors. Internal document/token pointers have a documented successfully-parsed-document contract. Framing reset accepts NULL as a no-op. |
| Uninitialized memory | Document/index state initializes before parsing; parsed public result structs initialize locally. Decoded scratch is inspected only through a successful returned length. Requests copy only snprintf's checked written length. Failed public result parsing leaves all caller bytes unchanged, exercised by canaries/fuzz assertions. |
| Dangling pointers | All spans remain caller-owned, stable and nonoverlapping for the call. Tokens contain bounded offsets, not retained frame pointers exposed to callers. Line bytes remain owned by their enclosing object until reset. |
| Invalid pointer arithmetic | Token pointer subtraction is private and only between members of the same document array. Every byte offset is proven within the original frame before addition. No wire-to-struct pointer casts, alignment assumptions or pointer tagging. |
| Format-string vulnerabilities | Request/fixture snprintf formats are compile-time constants, checked for failure/truncation. The only interpolated request string is exactly 64 validated lowercase hex bytes with explicit precision. Server strings never become format strings or logs. |
| Stack exhaustion | No recursion or VLA. JSON parsing uses an eight-entry stack; authored frames pass the <=4096-byte compiler gate. The largest nested call path combines bounded document, header and hex scratch (under 8 KiB plus bounded provider/call overhead). The 16 KiB line object must be caller-owned heap/enclosing storage on Android, not an automatic thread-stack object. |
| Excessive allocation | No authored or provider protocol allocation. Maximum scratch and traversal sizes are constants. Fixture tools use fixed frames <=16385, with their largest buffer static rather than on the stack. Fuzz inputs <=16385, RSS <=512 MiB, individual allocations <=32 MiB and execution time are capped. |
| Malformed serialization | Strict JSON grammar, UTF-8/escape/surrogate checks, duplicate-key rejection and exact integer representations precede semantic access. Canonical header lengths/CompactSize and pinned genesis hashes reject malformed/wrong-network identity replies. These checks do not claim PoW or consensus validation. |
| Malformed network input | The codec treats frames as untrusted: nonzero expected ID must match; non-null error, mixed method/reply, unsupported version, malformed result, invalid money or excessive resource use refuse. Batches/notifications are unsupported by reply APIs. No network is enabled until transport/session handling is qualified. |
| Race conditions | No mutable global production state. Each caller exclusively owns its line/index/output. Immutable provider tables may be shared. Concurrent mutation of a borrowed span is outside the documented API contract; future transport must enforce one owner. |
| Resource exhaustion | At most 128 tokens, eight nesting levels and 257 event iterations. Duplicate comparison is bounded by that token budget and 256-byte keys, sufficient for DNS names in feature maps. Decoder scans are bounded by the frame cap. Socket deadlines, retry budgets, connection counts and notification limits remain unimplemented and must be enforced by the next layer. |
| Secret leakage | No key, entropy, signing, wallet record, log or network callback in this API. Requests contain public script hashes whose disclosure still has address-privacy implications. Line reset clears bytes; parsed extension/server text is never exposed to UI. Test/fuzz seeds contain only public genesis/amount/protocol fixtures. |

The reused provider's raw-Unicode string decoding defect is explicitly bounded
to ignored syntax-checking or printable-ASCII output; no decoded Unicode text
is consumed. See READ_ONLY_SYNC.md. All 22 native executables pass
ASan/UBSan/LSan, authored GCC/Clang and provider Clang analysis, and authored
complexity <=10. Those results complement this review and do not prove complete
memory safety, secure networking or accepted balances.

## Offline sync and TLS quarantine review — 2026-09-13

The active implementation reviewed here is `sync.c`/`zcl_sync.h`, its public
fixtures and fuzzer, and build-profile separation. Existing uncommitted TLS
source is preserved as a disabled candidate, with its known unresolved hazards
in TLS_REVIEW.md. Preservation does not approve that code or resolve its review.

| Required hazard | Explicit active-code review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Address parsing requires exactly 35 bytes before the copy. Six phase values bound the request-method table. Existing bounded frame parsers handle all replies. Caller outputs stay unchanged until a full successful report; canaries exercise failed requests/reports. |
| Integer overflow/underflow; signed/unsigned conversions | Six nonzero IDs are reserved with first_id <= UINT32_MAX-5. Increment stops after the sixth reply. Validated enum range precedes subtraction/index conversion. Existing signed money bounds remain unchanged. |
| Use-after-free; double-free; leaks; dangling pointers | No allocation, ownership transfer or retained pointer exists. The session copies public address/result data. All borrowed spans remain valid only during a synchronous call. Abort clears candidate amounts and tip. |
| NULL dereferences; uninitialized memory | Public pointers are checked. Start initializes the entire session even on failure; parsed tip/address temporaries initialize before use. Failed-state corruption with fault=OK cannot publish success. Caller must initialize state and must not mutate its private fields. |
| Pointer arithmetic; format strings | No new production pointer arithmetic or formatting. Tests bound searches by remaining span and check snprintf results; test diagnostics contain only fixed strings/line numbers. |
| Stack usage; allocation limits; resource exhaustion | Fixed session/report and <=256-byte request buffers; no recursive calls, VLAs, allocation or retry loops. The existing bounded parser limits remain. Fuzz fixture scratch is static only in the single-threaded host harness. Authored function complexity remains <=10. |
| Malformed serialization/network input | Strict expected-ID parsers gate every state transition. Wrong network/genesis, errors, notifications and unexpected replies invalidate the attempt. Changed final tip publishes nothing. Tip equality establishes only consistency of server statements, not consensus or atomic mempool state. |
| Races; lifetimes | One worker owns a session and its connection/request-ID range. No concurrent mutation or cross-connection response is allowed. Timeout/disconnect/cancellation uses abort; no callback, scheduler, network or persistence is introduced. |
| Secret leakage | No keys, seed, secret logs or transmission are introduced. Address script hashes still reveal address interest to an eventual endpoint; identity screening cannot provide privacy or authenticate that endpoint. TLS stays excluded from normal archives and every JNI/Android build. |

Active-source Clang/GCC analysis and configured hash/JSON/QR provider analysis
pass. All 23 native functional tests pass ASan/UBSan/LSan; a separate archive
and forbidden TLS/JNI configuration test passes. The offline sync fuzzer passes
128180 executions in 121 seconds with source/binary hash rechecks. None of this
is a passing result for the quarantined TLS provider or real-network acceptance.

## Balance watch review — 2026-09-13

Scope: `sync_watch.c`, `zcl_sync_watch.h`, fixtures and event-sequence fuzzer.
The watch composes the reviewed attempt and never accesses transport or custody.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Validated 35-byte address and exact 32-byte source ID precede copies. Every request/reply delegates to the existing bounded C API. Snapshot output is a fixed caller struct, populated only after pointer/lifetime checks. |
| Integer overflow/underflow; checked conversions | Deadline addition checks now <= UINT64_MAX-timeout; timeout is 1..30000. Tokens stop at UINT64_MAX. Age subtraction follows monotonic-clock acceptance; clock rollback removes the report before subtraction. No narrowing conversions occur in the watch. Six request IDs retain the existing UINT32_MAX-5 bound. |
| Use-after-free; double-free; leaks; dangling pointers | No allocations, frees, retained pointers or native handles. Struct members own copied public data. Close clears state and makes further calls refuse. The adapter must retain the original owner lifetime for asynchronous work; numeric token equality alone is explicitly insufficient across recreated owners. |
| NULL dereferences; uninitialized memory | Init clears the entire object, then sets initialized only after validation. All public dereferences follow argument checks. Snapshots and intermediate reports initialize locally. Invalid/closed snapshots and failed token outputs leave caller bytes unchanged. Native structs are never serialized. |
| Pointer arithmetic; format strings | No pointer arithmetic or formatting in authored watch code. Test/fuzz offsets follow size bounds and fixtures check formatted output lengths. No untrusted text enters logs or format strings. |
| Stack usage; allocation limits; resource exhaustion | Fixed attempt, cached report and snapshot; no heap, recursion, variable stack arrays, queue, automatic retry or background timer. Authored frame/complexity gates pass. Fuzzer caps event count at 128 and frame length at 16384, with bounded execution/RSS/allocation settings. |
| Malformed serialization/network input | The watch accepts complete-only reports from its own C attempt. Strict reply/identity/money checks remain in force. Source metadata cannot authorize an endpoint. Deadlines include all phases; rejection clears partial amounts and marks any previous report stale. |
| Races and ownership | One worker owns all watch calls; no shared mutable globals. Late tokens return before time/state changes, tested against a byte-for-byte current-state snapshot. Cancellation and timeout stop publication; the eventual adapter must also interrupt/close real I/O. |
| Secret leakage | No key, seed, signing, storage, network or logging path. Cached addresses/amounts remain private application data: close clears the owner; restart/source changes discard it. No fresh balance can be restored from serialized UI state, and no state represents verified or spendable funds. |

Tests cover both networks, complete-only publication, freshness at 59999/60000
milliseconds, timeout at the exact deadline, failures at all six phases, clock
rollback, near-UINT64_MAX deadlines, token exhaustion, old/completed tokens,
restart with empty state and unchanged error outputs. Static analysis covers
the enabled code; the TLS review remains blocked and separate.

## Authentication continuation timing review — 2026-09-13

Scope: the new pure predicate in `custody_policy.c`, its JNI wrappers and the
elapsed-clock calls in `WalletAuthentication`. The key-protection predicate,
cryptographic operations and record persistence are unchanged.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access; pointer arithmetic | The C/JNI addition uses only two integer values, no buffers or dereferenced pointers. The fuzzer reads exactly three eight-byte spans after an exact 24-byte length check. |
| Integer overflow/underflow; signed/unsigned conversions | Ordering is checked before subtraction; no deadline addition exists. Negative jlong inputs refuse before uint64 conversion. A compile-time assertion proves the C duration fits jlong. Fuzzer shifts are 0..56; timestamp translation is guarded in both directions. Boundary tests include UINT64_MAX and Long.MIN/MAX. |
| Use-after-free; double-free; leaks; dangling pointers; NULL dereferences | No allocation, free, retained pointer, array pin or JNI object is added. Existing Pending identity/lifetime and cancellation own the platform signal. C/JNI use no env/type dereference. |
| Uninitialized memory; format strings; secret leakage | All new state is an initialized monotonic timestamp or checked primitive. No formatting, logging, secret input or serialized state. Test data are public timestamps. Pending continues to hold only a provider handle and public/ciphertext data. |
| Stack usage; allocation limits; resource exhaustion | Constant tiny C frames; no recursive calls, loops in the predicate, allocations or retries. Existing handler bounds remain; the C predicate is checked independently when Android delivers callbacks. All authored functions remain <=10 complexity. |
| Malformed input; races | Signed JNI and backward-clock refusals fail closed. Android reads one monotonic clock including sleep. Main-thread ownership and exact Pending/Cipher identity checks remain. Rechecking at successful callback and delivery prevents expiration between those events; resume expires even an unfinished prompt. This is an additional time gate, never authentication by itself. |

The original Handler-only path is documented in KEYSTORE_PLATFORM.md. Native
and JNI tests cover exact 89999/90000ms boundaries and invalid clocks; the
timestamp fuzzer checks translation invariance and that expired forward time
cannot reopen the window. These tests do not qualify a physical authentication
provider or prove an end-to-end hardware suspend/resume workflow.

## Sync owner lifetime review — 2026-09-13

Scope: `sync_owners.c`, its header and lifecycle fixtures/fuzzer. The pool is a
caller-owned C value, with no production globals or retained external pointers.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Every slot search is bounded by the four-element array. Address/source inputs pass through the existing exact-length watch initializer before publication. No caller input controls an array index or memory-clear length. |
| Integer overflow/underflow; checked signed/unsigned conversions | Issued IDs are checked below 9223372036854775807 before increment; close and clear-all preserve the counter. Zero/oversized IDs refuse lookup/close. No pointer/integer conversion or narrowing occurs. The cap prepares for exact positive Java-long representation. |
| Use-after-free; double-free; leaks; dangling pointers | No heap or free. A successful lookup lends an in-pool watch only for the current serialized C call. Borrowed pointers cannot cross JNI or survive close/reuse. Late callbacks must look up the original ID, which is never reassigned. Caller lifecycle/locking remains an explicit adapter obligation. |
| NULL dereferences; uninitialized memory | All public pointer arguments are checked. The pool initializes once to zero; a local watch initializes fully before a slot is occupied. Failed opens and failed lookups preserve outputs. Close clears an entire known slot; clear-all clears only slots and keeps issuance history. |
| Pointer arithmetic; format strings | Only addresses of bounds-checked array elements/members are formed. No byte-offset arithmetic, format strings or input logging in production. Test diagnostics use fixed messages/line numbers. |
| Stack usage; allocation limits; resource exhaustion | Four fixed slots and one local watch during open; no recursion, VLA, dynamic allocation, automatic retry or I/O. Full pools and exhausted IDs refuse. Static warning/frame gates and complexity <=10 apply. Fuzzer caps events/history at 128 and its campaign has time/RSS limits. |
| Malformed serialization/network input | No serialization or new network parser is added. The existing watch validates address/network/source; existing reply parsing remains authoritative. IDs are lifetime selectors only, never address/endpoint authentication or spending authority. |
| Races and ownership | The enclosing adapter must serialize all pool operations and every borrowed-watch use. No asynchronous pointer is returned. Tests deliberately reuse a slot and match old/new attempt-token values while proving the old owner cannot reach or mutate the new watch. Cross-thread locking will require separate JNI acceptance. |
| Secret leakage | Only public address/source and unverified balance/report state is held. Closing zeroes the released slot. No key, seed, custody, log or transport path is added, and TLS quarantine remains in force. |

## Read-only sync JNI review — 2026-09-13

Scope: `jni_sync.c`, its thin managed owner, shared public test fixtures and
native failure-injection/fuzzer target. No new networking or custody capability.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Input address/source copies independently cap 35/32 bytes. Request storage is 257 bytes including a checked status byte. Replies allocate exactly 16384 bytes; JNI array length/region checks precede use. Nine fixed Java-long snapshot fields use constant indexes. The fake VM checks every region against its exact array length. |
| Integer overflow/underflow; checked conversions | Negative time/token/ID inputs refuse before unsigned conversion. Deadlines fit positive Java-long time; first request ID is 1..UINT32_MAX-5. Owner and attempt IDs stop at INT64_MAX. Snapshot unsigned values are checked before conversion; signed pending deltas are already bounded in C. Size-to-jsize conversions use the existing checked JNI helper. |
| Use-after-free; double-free; leaks; dangling pointers | No native pointer crosses JNI. The single explicit registry's mutex spans every borrowed-watch use and close. A reply has one allocation and one clear/free path, including JNI exceptions. No Java global reference or pinned array is retained. Explicit managed close releases a slot; forgotten owners cause bounded exhaustion, never unbounded allocation. |
| NULL dereferences; uninitialized memory | JNI helpers check arrays and allocation results. Local snapshot/request arrays initialize to zero. Frame bytes are consumed only after a successful complete copy, then the full allocation is zeroed even on failure. NewByteArray/NewLongArray/region exceptions return no result. Request publication follows successful Java packet construction; failure aborts that attempt. |
| Pointer arithmetic; format strings | Request payload offset is one byte in a known 257-byte array. Other byte/array copying delegates to length-checked helpers. No arithmetic reconstructs native pointers. No input-derived logging or format string is added. |
| Stack usage; allocation limits; resource exhaustion | Stack holds fixed small watch/snapshot/address/request values; network frames stay off stack. One bounded frame allocation per synchronous reply; at most four persistent public watches. C frame and <=10 complexity gates pass. Fuzz histories/inputs/heap/RSS/time are bounded. |
| Malformed serialization/network input | Existing strict C parsing handles all frames, and oversized JNI inputs poison the current attempt. No private native struct is serialized or reconstructed from attacker bytes. NULL unavailable reports are distinct from zero amounts. JNI errors never promote a verified/spendable balance. |
| Races and lifetimes | The registry mutex covers open, lookup, mutation and close; snapshot Java allocation happens after releasing it. All callbacks retain an original owner ID and attempt token. Managed methods add per-owner serialization. Direct concurrent JNI tests exercise the native mutex without relying on the managed lock. No Java callback is invoked while holding the native mutex. |
| Secret leakage | This registry contains public addresses, source metadata and unverified reports only. Frame and released-slot storage are cleared. No wallet record, RNG, seed, key, signer, endpoint or network call is exposed. All fixture addresses/frames are public and unfunded; only the test APK bundles them. |

Native fault injection redirects allocation only for the test target's JNI
translation unit, leaving shipped code and libFuzzer's allocator unchanged.
The bounded fake VM is single-threaded and tracks its local references for
deterministic cleanup. Actual JVM -Xcheck:jni and device fixtures are separate
evidence for the real VM boundary. This does not qualify physical custody or
the quarantined TLS candidate.

## Fuzz configuration enforcement — 2026-09-13

`ZCL_FUZZ=ON` now requires `ZCL_SANITIZE=ON`. Previously the fuzz harness could
carry sanitizer flags while linked core/provider compilations had only coverage
instrumentation. The negative configuration regression reproduces that prior
acceptance and now requires an explicit refusal. Its positive configure checks
the actual emitted commands for enabled native sources and Android/Commons
providers, requiring ASan/UBSan and fail-on-finding behavior. On this host it
observes 56 such compilations; missing host Clang is explicitly unqualified for
the positive check. These are build-configuration checks, not additional fuzz
executions or a substitute for sanitizer runs.

The command inspection also found the standalone JSON-provider test already had
ASan/UBSan but lacked `-fno-sanitize-recover=all`. That option is now mandatory
there as in the core/provider builds. Its eight cases pass with the stricter
failure behavior. The previously fuzzed JNI binary remains byte-identical after
the build-profile change; no unchanged fuzz campaign was repeated.

## Signed amount display review — 2026-09-13

Scope: `zcl_amount_delta_format`, its JNI byte-copy helper and public fixtures.
This is display formatting; the existing nonnegative payment parser is unchanged.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | C formats into an initialized 18-byte temporary. Prefix is exactly zero or one; the existing amount formatter writes at most 17 bytes. Capacity is checked before copying to the caller. Tests check exact output, adjacent canaries and unchanged buffers/lengths for every insufficient capacity. |
| Integer overflow/underflow; signed/unsigned conversions | Delta must lie within +/- MAX_MONEY before negation, excluding INT64_MIN and every out-of-range value. The magnitude then fits uint64_t exactly. Prefix plus returned count is <=18. JNI jlong has the same signed 64-bit range; result length is checked before jsize conversion. No floating-point or locale conversion participates. |
| Use-after-free; double-free; leaks; dangling pointers | No C allocation/free, retained pointer, pin or Java global reference. JNI copies one bounded stack array into a checked Java allocation; local references are VM-owned for the native call. |
| NULL dereferences; uninitialized memory | Public C output pointers and JNI env are checked. Temporary byte arrays and lengths initialize before use. The shared private JNI helper receives only checked env and initialized local arrays. NewByteArray/SetByteArrayRegion failures return no result and preserve the pending exception. |
| Pointer arithmetic; format strings | The only new C offset is a proven zero/one byte prefix inside the 18-byte temporary. No untrusted index, implicit NUL, printf formatting or input-derived diagnostic is used. Fuzzer canaries bound the returned length before indexing. |
| Stack usage; allocation limits; resource exhaustion | Fixed <=18-byte production arrays, no recursion/VLA, bounded existing decimal loops. At most one <=18-byte Java result allocation. Host campaign has time/input/RSS limits; no retry or background work is introduced. |
| Malformed input; races; ownership | Arbitrary signed 64-bit values fail closed outside money range. Output changes only after all checks succeed. No global state or mutable shared input exists; caller owns output exclusively for the call. Positive and negative display changes remain invalid inputs to the payment-amount parser. |
| Secret leakage | Only public monetary display values pass this API. It accepts no key, seed, record, address or network frame and emits no logs. Secret-bearing state and TLS quarantine are unaffected. |

## Read-only display expiry hint review — 2026-09-13

Scope: one relative `next_change_ms` field in the C snapshot and its JNI/managed
projection. C returns a delay to the current attempt deadline or fresh-report
expiry, and zero if neither can change by time alone. It grants no freshness
authority to a platform timer; delivery must read the C snapshot again.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Snapshot is a fully initialized C value. JNI packet grows from nine to ten longs, with both array creation and region count changed together. Fake-VM storage and independent bounds now require ten. Constant field index nine is in bounds. No network or wallet-file layout changes. |
| Integer overflow/underflow; conversions | The clock update cancels every expired attempt before deadline minus now. Freshness delay is computed only for age < FRESH_MS. No absolute-time addition occurs in production. Delays are positive <=60000ms or zero, checked before jlong conversion; managed decoding rejects negative delay. Tests include unsigned and signed maximum clock boundaries. |
| Use-after-free; double-free; leaks; dangling pointers; races | No new C allocation or pointer retention. The existing registry mutex still covers snapshot derivation; the ten-long Java result is allocated after unlocking and has no global reference. The owner retains its established serialized clock and explicit close semantics. |
| NULL dereferences; uninitialized reads | Existing C argument checks apply. New field starts at zero on all no-event/error states. JNI now refuses NULL env before touching the watch; the fault fixture proves this does not expire its attempt. Java allocation/region exceptions still return no result. |
| Pointer arithmetic; format strings; stack; resource exhaustion | No new pointer arithmetic, format string, loop, recursion or VLA. Stack growth is one uint64_t/long field. The bounded relative hint permits one timer instead of periodic polling; timers and worker work remain outside this C slice. |
| Malformed input and state consistency | Existing strict protocol and clock checks are unchanged. Cancelled/offline/rollback/restarted state has no scheduled transition unless a new explicit attempt is active. Tests follow a returned delay on a copied watch and require unchanged flags just before it and a real change exactly at it. |
| Secret leakage | Only public status/timing metadata is added. No seed, key, wallet file, endpoint, diagnostic content or transport authority is exposed. TLS remains quarantined. |

## Bounded history codec review — 2026-09-13

Scope: `electrum_history.c`, appended history request type, host fixtures and
the existing Electrum fuzzer. The codec reports server assertions only.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The parsed array has exactly 16 slots. Loop index is checked before forming an entry pointer; duplicate comparison reads only the already counted prefix. A seventeenth child refuses without copying output. Existing hex decoding checks decoded count against the 32-byte destination before writing; short/odd/invalid hashes refuse. Request formatting retains its checked 256-byte temporary and exact output capacity checks. |
| Integer overflow/underflow; signed/unsigned conversions | Strict int64 parsing precedes the -1..INT32_MAX bound and int32 cast. No height arithmetic or confirmation calculation occurs. The size_t count increments at most 16 times. Request IDs remain nonzero uint32 with fixed PRIu32 formatting, and checked nonnegative snprintf lengths precede size_t conversion. |
| Use-after-free; double-free; leaks; dangling pointers | No allocation/free, retained pointer, socket, thread or callback. Token pointers refer only to the current successfully parsed stack document. Output is a caller-owned value copied after all entries succeed. Private partial hex writes cannot escape a failed call. |
| NULL dereferences; uninitialized memory | Public output and frame arguments use explicit checks. Result is used only after the existing envelope helper succeeds; entry pointers only after child lookup succeeds. Candidate history initializes to zero, including unused entries, before parsing. Private helpers receive validated document tokens and bounded local output. |
| Pointer arithmetic; format strings | New production code forms only checked entry/member addresses; existing token traversal supplies bounded children. No new raw byte offsets or input-derived format string. The request method name is selected from two fixed literals; script hash formatting reads exactly 64 initialized bytes. |
| Stack usage; allocation limits; resource exhaustion | Fixed 128-token document plus 16-entry candidate; existing hex helper uses a fixed 2974-byte temporary in a separate frame. The nested call chain remains bounded, with no recursion/VLA/heap. Strict 4096-byte individual-frame warnings pass. Traversal and duplicate searches are quadratic only within the fixed 16-entry cap. Frame/depth/token bounds remain unchanged. |
| Malformed serialization/network input | Shared parser rejects bad UTF-8/escapes, duplicate keys, incorrect IDs, notifications, errors and trailing data even in ignored fields. History rejects nonobjects, malformed IDs/heights, local-only -2, and duplicate decoded hashes including case aliases. Oversized history never succeeds partially; no completeness, amount, inclusion or spending claim is derived. |
| Races and ownership | Synchronous caller-owned stable nonoverlapping spans; no globals or persistent state. Callers must serialize writes to their own output. This codec alone does not bind a reply to an address, source, tip or owner; that remains an independent sync integration gate before presentation. |
| Secret leakage | Inputs/outputs are public protocol metadata only. No seed, key, custody file, fee estimate or network operation is exposed or logged. Historical reference files were inspected as text, not executed. TLS remains blocked and excluded. |

## Optional history sync lifetime review — 2026-09-13

Scope: the opt-in seven-response C sync/watch profile. Default six-response
balance behavior and existing phase values remain unchanged. No JNI history
entry point, source transport or presentation is enabled in this slice.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | HISTORY is an appended enum handled before the existing six-element request table is indexed. Active-phase validation bounds every other index. History traversal uses only the <=16 count produced by the strict codec. Candidate history clearing uses its exact sizeof. Existing request/frame limits and transactional output checks remain authoritative. |
| Integer overflow/underflow; conversions | Opt-in initialization reserves seven nonzero IDs and rejects first_id > UINT32_MAX-6. The last reply does not increment its ID. Phase transitions explicitly route BALANCE to HISTORY to TIP_AFTER without relying on the appended enum's ordering. Positive int32 heights are checked before uint32 conversion and comparison with the tip; no confirmation or amount arithmetic is introduced. |
| Use-after-free; double-free; leaks; dangling pointers | All new state is fixed caller-owned storage. No allocation, free, retained external pointer or asynchronous callback. Existing JNI owners still refer to registry IDs and borrow watches only under the mutex; their report packet remains ten longs and their profile stays balance-only. |
| NULL dereferences; uninitialized memory | Opt-in wrappers delegate to the existing argument-checking initializers before accessing state. Invalid starts clear state. Reports initialize fully, history availability defaults false, and every abort clears candidate history and availability together. Private partial reports cannot publish before DONE. |
| Pointer arithmetic; format strings | New production pointers name existing members/checked array elements only. No byte offsets, pointer reconstruction, input logging or format strings. Existing checked request encoder handles the history method. |
| Stack usage; allocation limits; resource exhaustion | Candidate, cached report and snapshots each grow by one fixed 16-entry history and availability flag. Owner count remains four and storage remains bounded; strict individual-frame warnings apply to the JNI build too. No timer, thread, heap, recursion, VLA, retry or extra query in the default profile. History overflow fails the opt-in attempt, never truncates. |
| Malformed serialization/network input | Every reply uses the current expected ID and strict existing envelope parser. The codec rejects duplicate IDs, malformed heights and oversize arrays. Claimed heights beyond the initial tip or changed final height/hash return IO_UNCERTAIN and clear all candidate values. Equal tips establish only internal server consistency, not proof or an atomic mempool snapshot. |
| Races, ownership and restart | Profile is immutable within the watch lifetime and shares its source/address, token, clock and deadline. Late tokens cannot advance the current clock or cancel a replacement. A failed refresh retains only the prior completed report as stale; rollback clears it. Close/reinitialization discards reports and the prior source; callers must still retain owner identity as well as token across callbacks. |
| Secret leakage | Only public unverified history IDs/heights are added. No keys, wallet records, amounts derived from transactions, endpoint or authentication authority. Existing TLS quarantine and hardware custody controls remain intact. |

Measured host sizeof values: history 584, report 688, attempt 704, watch 1504,
snapshot 752 and four-owner pool 6056 bytes. These are host ABI measurements,
not a claim about every Android ABI or total thread stack use. Static frame
warnings and both Android ABI builds complement this storage measurement.

## History JNI projection review — 2026-09-13

Scope: history-enabled owner creation, one atomic public snapshot projection,
the thin managed decoder and native/JVM/device fixtures. No history screen or
network connection is enabled here.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Snapshot storage has 156 initialized longs: 12 metadata fields and 16 rows of nine longs. The C count is checked <=16 before offset arithmetic; maximum row ends at index155. Hash indexes are word*4+byte <=31. The fake VM independently checks actual array lengths/regions, including a maximum history packet. Managed decoding checks exact length, count and word bounds before indexing. |
| Integer overflow/underflow; signed/unsigned conversions | Each uint32 hash word is assembled from four uint8 bytes and converts exactly to positive jlong. Signed int32 reported heights also fit jlong exactly. Count <=16 proves 12+9*count<=156; a static assertion proves jsize representation. Existing signed timestamp/token/owner guards apply; C enforces the extra request ID for history mode before changing the attempt. |
| Use-after-free; double-free; leaks; dangling pointers | New owner creation reuses the same four-slot registry and never-reused IDs. Failed opens publish no slot; close clears it. Snapshot retains no Java reference or pointer, and its C stack value is copied into a local Java array after unlocking. JNI allocation/region exceptions return NULL; the VM owns local-reference cleanup. Reply frame allocation/zeroization/free remain unchanged and fault-tested. |
| NULL dereferences; uninitialized memory | Both snapshot JNI functions refuse NULL env before touching native time/state. All output longs initialize to zero. Only successfully sampled state is encoded; failures return an error packet or NULL on VM exception, never a successful partial report. The opt-in open delegates to existing network/array and watch argument checks. |
| Pointer arithmetic; format strings | New pointer offsets are bounded rows in the fixed array; no pointer/integer casts or retained borrows cross JNI. Hash encoding uses explicit fixed loops. Production adds no format string or input diagnostic. Managed hex formatting acts only on checked public words. |
| Stack usage; allocation limits; resource exhaustion | Fixed maximum JNI result payload is1248 bytes plus the existing bounded C snapshot on the nested helper frame. No VLA, recursion, new native heap, timer or worker. Java creates at most one156-long result and16 small public row objects/hex strings per snapshot. Required frame warnings/analyzers apply; combined balance/history uses one sample rather than racing two reads. |
| Malformed input and publication | C remains authoritative for parser, tip, money, height, freshness and complete-only rules. Managed decoding rejects malformed packet shape/flags/heights/words and an unavailable snapshot with history. Absent history remains distinct from an empty assertion. New JNI entry points cannot enable transport, spend, select a server, restore native state or derive a fee. |
| Races, ownership and restart | The mutex spans owner lookup and the entire C snapshot. Java result allocation occurs after unlock. Managed operations retain their original owner and sample the elapsed clock while serialized. Mixed profile owners consume the same bounded pool; callbacks for closed history owners cannot reach replacement balance owners, even when attempt tokens match. |
| Secret leakage | These packets contain unverified public transaction IDs/heights and existing public amounts/status only. No key, seed, wallet file, endpoint or input-derived log is added. Fixtures use synthetic public claims and test-only assets; custody controls and TLS quarantine remain unchanged. |

## Bounded transparent transaction codec review — 2026-09-13

Scope: transaction check/read/write/ID C units, public owned representation,
deterministic tests, wire/object fuzzer and offline original-prefix projection.
This is a bounded v4 transparent wire codec, not a consensus or signing engine.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Public length is capped at1925 before parsing. Reader/writer offsets start at0, advance only after `length <= capacity-used`, and sticky failures never advance. Byte pointers form only after that check and nonzero length. Parsed counts are bounded before array indexing; serializer validates all counts/scripts before traversal. All transaction hash reversal loops are exactly32 bytes. Canaries, every fixture truncation/capacity and max-size cases exercise bounds. |
| Integer overflow/underflow; signed/unsigned conversions | Little-endian integers assemble into uint64 with widths<=8, shifts<=56, unsigned operands. uint32 casts follow exactly4-byte reads. CompactSize is checked for canonical width and local capacity before size_t conversion. Static assertions bind all counts/scripts to single-byte encoding and the1925-byte wire formula; length increments are bounded by that formula. Each output must fit `MAX_MONEY-total` before addition; negative wire encodings exceed the unsigned money limit. Expiry is checked before use. No fee, height-difference or signed-money arithmetic occurs. |
| Use-after-free; double-free; leaks; dangling pointers | No heap allocation/free or retained input pointers. Parsed output owns all field bytes; scratch candidates remain on the synchronous stack. Serialization writes a private fixed scratch buffer and publishes only after complete validation/encoding. ID output is copied only after both hash calls succeed. Input/output spans must be stable and nonoverlapping for the call. |
| NULL dereferences; uninitialized memory | Every public pointer is checked before access; check helper also rejects NULL transaction/size output. Reader's status/offset and candidate fields initialize before reads. Short input makes failure sticky; subsequent private reads return0 or do nothing without accessing missing bytes. Hash and writer buffers initialize before use. Unused parsed fields stay zero and never participate in serialization. |
| Pointer arithmetic; format strings | Cursor addition and byte-copy offsets follow remaining-capacity checks; hash reversal cannot subtract below0. No pointer subtraction, unaligned loads or input-derived format strings in production. Array addresses follow checked count loops. Test-only hex pointer subtraction stays within one fixed literal. |
| Stack usage; allocation limits; resource exhaustion | Owned transaction is about2.2KiB on the host, parser candidate and writer scratch are in separate bounded frames. ID nests one1925-byte serialization buffer with the serializer's similarly bounded frame and hash-provider calls. Strict4096-byte frame warnings pass. No recursion, VLA, heap, persistent owner, mutex or worker is added. Duplicate comparison is at most28 pairs. Unsupported scripts/vectors refuse before copying instead of truncating. |
| Malformed serialization/network input | Exact v4 header/group, canonical lengths, complete span, zero shielded tail, nonempty vectors, unique/non-null outpoints, expiry and output-money bounds are enforced. Scripts remain opaque and may be invalid to execute; this limitation is explicit. Legacy/v3/shielded/oversized transactions fail closed. Positive parsing does not promote unverified sync/history into funding, signing or broadcast authority. |
| Races and ownership | No mutable global state or borrowed report/network data. The caller owns synchronization of its stable objects; the API does not preserve a review across later mutation. Exact-byte review and funding ownership are separate next steps. No JNI/UI path exposes this codec yet. |
| Secret leakage | Inputs, outputs, scripts and hashes are public transaction data. No seed/key, wallet file, endpoint, network, diagnostic secret or signing operation enters this slice. Original test rows are public synthetic data; the projection script reads pinned Git objects and runs only text/hash tools. TLS and hardware custody controls remain unchanged. |

## Script decoding and previous-output review — 2026-09-13

Scope: exact script-to-address decoding and hash-matched previous-output
extraction. No funding, key ownership, signing, broadcast or chain authority.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Template comparisons short-circuit on exact25/23-byte lengths before reading prefixes/suffixes. The20-byte hash copy starts at checked offset3/2. Previous bytes use the existing1925-byte bounded parser; output indexing occurs only after `index < output_count` with count<=16. Fixed32-byte ID comparison precedes publication. Tests substitute every opcode byte, truncate/mutate previous frames and exercise UINT32_MAX indexes. |
| Integer overflow/underflow; conversions | No arithmetic on amounts or hashes. Template offsets and lengths are fixed constants with matching exact-length checks. uint32 index is compared before array conversion/indexing. Network enum must be MAINNET/TESTNET; it is caller-selected metadata because script bytes have no network tag. No signed conversion or pointer-derived count. |
| Use-after-free; double-free; leaks; dangling pointers; races | Neither helper allocates, frees, stores a pointer or creates mutable global state. Previous parsing creates a fully owned2200-byte local object; hashing serializes that same object, so no borrowed byte view escapes. Stable nonoverlapping caller objects remain the synchronous contract. Output copies only after all checks pass. |
| NULL dereferences; uninitialized memory | Public script/address and input/output pointers are checked; previous-wire NULL rejection is delegated to the checked parser. Failed parsing returns before the previous object is read. The address candidate and ID buffer initialize before use; a template assigns kind before copying its hash. Failed operations preserve all output bytes. |
| Pointer arithmetic; format strings | Script suffix/hash pointers form only after exact-length equality. Previous-output pointers form only after bounded index validation. No new production format strings, pointer subtraction, casts to unaligned typed storage or mutable cursor logic. |
| Stack usage; allocation limits; resource exhaustion | Template scratch is one small address. Previous extraction adds one2200-byte owned transaction above the existing bounded ID/serializer/hash call chain, roughly7KiB of bounded nested application scratch. No recursion/VLA/heap; strict4096-byte individual-frame warnings apply. Size/profile refusal occurs before expensive hashing; no repeated retry, network query or owner is added. |
| Malformed input and authority | Exact transaction hash and output index bind extracted amount/script to supplied canonical bytes. Unknown scripts stay opaque until the separate exact-template decoder succeeds. Alternative pushdata or trailing opcodes never get silently classified. Neither success claims inclusion, unspentness, maturity, chain identity, private-key ownership or a permissible fee. Funding evidence and authenticated review remain unimplemented gates. |
| Secret leakage | Only public transaction/address fields are processed. Tests use pinned synthetic prefixes and local zero/FF public hashes. No seed, private key, wallet file, endpoint, secret diagnostic or JNI capability is added. TLS remains quarantined; hardware custody policy is unchanged. |

## Bounded transaction assessment review — 2026-09-13

Scope: owned public accounting/destination data derived from hash-matched
previous outputs, with an explicit caller-provided absolute fee ceiling.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Entire current transaction passes the existing bounded object checker before any traversal. Previous descriptor count must exactly equal the validated input count<=8 before indexing. Output loops are <=16. Each previous span independently enters the strict1925-byte parser/hash/index extractor; every destination passes exact template decoding. Public assessment copies only after all rows, sums, fee and current ID succeed. Max-count tests and failure canaries cover the owned arrays. |
| Integer overflow/underflow; signed/unsigned conversions | Input and output sums reuse checked `zcl_amount_add`; every addition remains <=MAX_MONEY. Fee reuses `zcl_amount_subtract` and cannot underflow; explicit ceiling is checked <=MAX_MONEY before work and compared without conversion. No floating point, multiplication, fee-rate division, narrowing or signed arithmetic. Current serialized size is the already bounded size_t from object validation. |
| Use-after-free; double-free; leaks; dangling pointers; races | No allocation/free, callback, timer, native owner or mutable production global. Descriptors borrow stable previous spans only for the call; repeated inputs may share bytes. The result owns all rows/IDs and retains no descriptor or transaction pointer. Caller output must not overlap any input and caller serializes mutation. A completed assessment cannot authorize a later mutated transaction. |
| NULL dereferences; uninitialized memory | Public previous/output pointers are checked and transaction NULL is rejected by the common checker. Previous wire NULL is rejected by its parser. Candidate fields and unused rows initialize to zero. A local previous-output/address is read only after its producing helper succeeds. Any late failure returns before copying public assessment storage. |
| Pointer arithmetic; format strings | New code only indexes arrays under validated fixed counts; byte cursor arithmetic stays in the previously reviewed codec. No new pointer subtraction, string formatting, unaligned typed load, secret diagnostic or input-derived format. |
| Stack usage; allocation limits; resource exhaustion | One1056-byte owned candidate adds to the bounded previous-output/ID/serialization call chain, roughly8KiB of nested application scratch. Per-frame4096-byte warnings remain enabled. At most8 bounded previous transactions are processed; repeated sources are deliberately rechecked without introducing a persistent cache. No recursion, VLA, heap, network retry or background worker. |
| Malformed input and value consistency | Source count/order/hash/index, duplicate/null current outpoints, unknown input/output templates, insufficient funds, money overflow and excessive fee all refuse the whole result. No partial row or hidden output reaches review data. Current ID/size cover current scripts as serialized, not a future signature. Independent numeric fixtures and late-source mutations exercise the composition beyond individual helper tests. |
| Authority and secret leakage | The result contains public numeric/address metadata only. It cannot establish chain inclusion, unspentness, maturity, branch validity, key/change ownership or consent. ID does not bind network/fee policy; those remain explicit fields and future authenticated-review requirements. No seed, private key, wallet record, JNI/UI signing or broadcast capability is added. Synthetic sources contain no spendable funds. TLS quarantine and hardware custody remain unchanged. |

## Public change-address derivation review — 2026-09-13

Scope: a fixed internal-chain wrapper around the existing entropy-to-address
derivation, preserving the receiving wrapper's external-chain behavior.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Existing entropy16/20/24/28/32, output35 and blinding32 checks remain before their respective reads/writes. Both public wrappers share those checks. Five path elements use the same fixed loop and existing checked BIP32/HMAC/public-key/address primitives. New chain selection is private and fixed by wrappers, never an unchecked caller index. Bounds tests cover every invalid entropy length through33, SIZE_MAX, short output capacities and invalid blinding spans. |
| Integer overflow/underflow; signed/unsigned conversions | Chain is fixed uint32 0 or1. Hardened purpose/coin/account and normal index encoding are unchanged; index>=2^31 refuses before derivation. No increment, silent retry, fee arithmetic or persisted counter is introduced. Independent oracle includes index2^31-1 on both branches and networks. |
| Use-after-free; double-free; leaks; dangling pointers; races | The shared derivation retains one bounded transient EC context and one cleanup path. No key pointer escapes or mutable global is added. Context creation/randomization failures return through cleanup, including allocated-but-uncreated cases. Both branches now exercise allocation/context/blinding failures; the wrapper verifies every owned allocation is cleared and freed exactly once. Caller entropy/blinding stay stable and owned for the call. |
| NULL dereferences; uninitialized memory | Shared validation rejects NULL entropy/address/length and the EC boundary rejects NULL blinding. Seed/public-key/key/context storage initializes before use. Failed derivation never reads an unproduced child/public key or publishes a partial address. Provider fault injection checks unchanged output bytes and lengths at early/middle/final hash steps, including RIPEMD160. |
| Pointer arithmetic; format strings | No new pointer arithmetic or format string in production. The only path change replaces fixed chain0 with a private fixed wrapper choice0/1. All public text still uses checked exact35-byte address encoding. No key, mnemonic, entropy or path-specific error content is logged. |
| Stack usage; allocation limits; resource exhaustion | Existing fixed seed64/public-key33/extended-node64/context frames and <=1024-byte transient context remain. No new large buffer, VLA, recursion, worker or persistent allocation. PBKDF2 remains exactly2048 iterations and derivation exactly5 levels. Every injected invalid child returns at that level; tests assert no retry or unexpected extra child call. |
| Malformed input and compatibility | Both API branches use the same original wallet entropy/passphrase/account/network profile. Receive remains external0; change is internal1. Unknown networks, hardened address indexes, malformed entropy/blinding and small outputs refuse. OpenSSL independently derives both paths without calling app seed/HD/hash/address code. No wallet record or existing receiving address metadata is migrated or overwritten. |
| Secret leakage and ownership authority | Existing optimization-resistant cleanup clears temporary mnemonic/seed, parent/child nodes, final private node, public scratch and context on success/failure. Fault wrappers inspect cleared spans and pre-free storage. Caller inputs remain caller-owned and must be cleared afterward. Only a public address is returned; no signing key, index reservation, JNI change route or transaction approval is added. Authenticated change ownership and durable index/recovery are separate gates; hardware custody and TLS quarantine remain unchanged. |

## Immutable unsigned review lifetime — 2026-09-13

Scope: one caller-owned draft lifetime and copied public snapshots/bytes.
No key, authentication, JNI, signing or broadcasting authority is introduced.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Preparation reuses the strict1925-byte parser and bounded assessment. The unsigned-script loop traverses only validated <=8 inputs. Retained bytes come from the serializer's checked fixed-capacity output, never a second read of a borrowed span. Copy checks capacity against the successfully stored length before memcpy. Snapshot copies fixed owned storage. Canaries, every truncation/capacity and subsequent caller mutation exercise publication boundaries. |
| Integer overflow/underflow; signed/unsigned conversions | Deadline addition follows `now <= UINT64_MAX-90000`. ID increment follows `issued < INT64_MAX`, so positive IDs fit a future signed adapter without conversion here. Read validates matching positive ID before clock checks; `last <= now < deadline` precedes subtraction for remaining time. No timestamp narrowing, signed math, fee recalculation or wall/server clock enters this code. The fuzz model independently uses duration subtraction. |
| Use-after-free; double-free; leaks; dangling pointers | No heap, free, callback, timer or retained external pointer. The prepared representation owns canonical bytes and the entire assessment. Inputs remain stable only during the synchronous call and may be destroyed afterward. Returned snapshots/bytes may be changed by callers without affecting the retained draft. Clear uses optimization-resistant zeroization on the entire data object and retains only the issuance counter. |
| NULL dereferences; uninitialized memory | Public owner/output pointers are checked before access. Draft/previous pointer checks are delegated to the strict parser/assessment before any reads. Candidate data initializes before preparation and is published only on complete success. Failed preparation leaves owner/ID untouched; failed reads preserve caller outputs. Inactive state cannot be read under an old ID. |
| Pointer arithmetic; format strings | Production adds no byte offsets, pointer subtraction, reinterpret casts, format strings or logging. It accesses validated input members and fixed owned objects. Fuzz byte offsets follow the remaining10-byte step check and an overall640-byte cap. |
| Stack usage; allocation limits; resource exhaustion | Owner and candidate data are fixed approximately3KiB objects; preparation's owned transaction occupies a separate bounded frame. The first optimized Android build inlined preparation and correctly refused a5344-byte frame. Preparation is separated into its own C unit, preserving the4096-byte frame gate without a warning waiver or noinline annotation. The complete nested path remains bounded (roughly14KiB of application scratch plus provider frames); no recursion/VLA/heap/global pool. An active draft refuses replacement, and at most8 bounded previous sources are assessed once per opening. |
| Malformed input and authority | Every opening rechecks serialization, exact previous hashes/indexes, all destination templates, money totals and explicit fee ceiling. Nonempty input scripts refuse, even if otherwise parseable; unsigned status does not establish spendability. Neither transaction ID nor lifetime ID binds user consent, chain context, inclusion/unspentness, key/change ownership or future signatures. Those gates remain unavailable. |
| Races, lifetime and restart | Caller serializes all operations under one enclosing adapter lock and initializes the owner only once per callback lifetime. IDs never repeat across clear/cancel. Invalid or stale IDs return before sampling time, so extreme stale clocks cannot invalidate a replacement. Valid rollback or inclusive expiry clears data; successful reads never move the fixed deadline. Teardown must clear; process restart must not restore state or IDs. There is no JNI adapter in this slice. |
| Secret leakage | Only supplied public transaction metadata is retained; no seed/private key, custody record, endpoint or secret diagnostic. Draft data clears on cancel/expiry/rollback/teardown. Callers separately own and dispose of input/output copies. Synthetic fixtures contain no real funding. TLS and the BLAKE2 candidate remain blocked and excluded; hardware policy is unchanged. |

## Review context projection — 2026-09-13

Scope: exact raw lock/expiry and outpoint/sequence fields in the same immutable
snapshot as accounting. This does not interpret contextual chain validity.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Context traversal uses the previously validated <=8 input count; each ID copy is exactly32 bytes between fixed arrays. Unused candidate rows initialize to zero. Snapshot assignment copies the whole fixed context; eight-to-two replacement tests check that no old extra row remains. |
| Integer overflow/underflow; signed/unsigned conversions | uint32 lock, expiry, index and sequence assign directly without conversion or arithmetic. No count, money, confirmation, finality or replacement calculation is introduced. Tests retain high-bit lock/sequence values and the maximum accepted expiry499999999 exactly. |
| Use-after-free; double-free; leaks; dangling pointers; races | Context owns its bytes and shares the existing owner/ID/lock/lifetime. No allocation, free, pointer retention, global state, callback or separate snapshot clock is added. Caller mutation and snapshot-copy mutation cannot alter retained context. Whole-data clear covers context too. |
| NULL dereferences; uninitialized memory | The private projection is called only with the checked owned transaction and initialized candidate after successful serialization. All public argument guards and failure atomicity remain unchanged. Partially assessed/prepared data cannot publish an ID or snapshot. |
| Pointer arithmetic; format strings | Only fixed checked array members and sizeof32-byte IDs are used. No cursor, pointer subtraction, format string, reinterpret cast or log. |
| Stack usage; allocation limits; resource exhaustion | Context adds328 host bytes to the owner and snapshot, without adding the full2200-byte parsed transaction to persistent state. Preparation remains a separate C unit;4096-byte frame warnings and both ABI builds remain mandatory. Loops are bounded to8 and add no hash, parse, allocation, recursion, VLA, retry, worker or timer. |
| Malformed serialization/network input | Context is derived only from the exact strict parsed transaction already matched to previous sources and explicit fee policy. The existing parser bounds expiry and preserves uint32 fields; no field is normalized, truncated or silently omitted. IDs stay in display byte order and indexes correspond to the same assessed input row. |
| Secret leakage and authority | Additional fields are supplied public transaction metadata. They share cancellation/expiry cleanup and contain no seed/key, endpoint or wallet record. A raw lock/expiry/sequence value proves no finality, current-chain acceptance, replaceability, inclusion, unspentness, ownership or consent. JNI, signing and broadcast remain absent. |

## Unsigned review JNI ownership and projection — 2026-09-13

Scope: one process-wide C review owner, bounded input copies, public packet
projection, thin managed lifetime, fake/real VM and emulator fixtures. No UI,
key, authentication, signing, broadcast or network route is added.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Each Java byte array length is checked against1925 before region copy; previous array count must be1..8 before element indexing. All input storage is one fixed allocation. Packet counts are checked1..8 and1..16 before offsets; header20 + inputs136 + outputs112 yields268 longs, last index267. Hash loops emit exactly8/5 words from32/20 bytes. JNI fake regions independently enforce actual array capacities and maximum packets. Managed decoding requires exact packet length before traversing rows. |
| Integer overflow/underflow; signed/unsigned conversions | Opening rejects negative fee/time and time above INT64_MAX-90000 before unsigned casts. ID range has a static assertion for jlong; stale IDs still fail in C before time effects. Validated uint32 hashes/lock/expiry/index/sequence convert exactly to positive jlong. Totals/fee remain <=MAX_MONEY and size/counts remain bounded before casts; packet size is statically within jsize. Negative/oversize JNI counts reject before size_t conversion. Managed narrowing follows explicit width/count checks; money construction delegates to C. |
| Use-after-free; double-free; leaks; dangling pointers | One checked sizeof allocation owns all input spans. Its descriptors point only into itself and remain borrowed during synchronous preparation. One cleanup tail zeroes its entire storage then frees once. Fake allocation wrappers inspect every byte before free on all region/element/length failures. JNI deletes each local element reference even when a read raises an exception; no pin, global reference or pointer-valued handle escapes. Managed constructor/read failures cancel their original ID, and close clears ID/clock. |
| NULL dereferences; uninitialized memory | Public JNI entry checks env and required arrays before VM use; pending exceptions stop further reads. NULL inner elements refuse. Allocated input storage and result arrays initialize before use. Header/row projection writes only private scratch; any projection error publishes one status, never partial rows. Failed Java allocation/region publication returns NULL and attempts ID-bound cancellation. Fake VM covers partial native scratch writes before exceptions. |
| Pointer arithmetic; format strings | Previous indexing is bounded by the checked <=8 count. Packet pointer offsets use fixed20/17/7 constants under validated counts; hash byte indexes max31/19. The wire status prefix uses a1926-byte array with a1925-byte payload. No unaligned typed loads, pointer reconstruction, pointer subtraction, format strings or input logging enter production. |
| Stack usage; allocation limits; resource exhaustion | Persistent owner remains3352 host bytes. Snapshot1392 and maximum long packet2144 are bounded;4096-byte individual-frame warnings pass, including optimized Android builds. Input copying uses one fixed roughly17KiB heap block rather than a large automatic array. At most8 element refs are acquired/deleted in sequence and one Java result array is allocated. No recursion/VLA, socket, retry loop, worker, finalizer or growing native registry. Busy/exhausted state refuses instead of replacing a live draft. |
| Malformed input and publication | All copied inputs reenter the existing strict parser/previous-source/accounting/lifetime validation. Packet projection independently checks counts, money, kind/network and positive bounded remaining time before signed publication. Managed decoder checks status, shape, widths and immutable row construction, closing on failure. Public raw fields do not establish contextual chain validity or consent. |
| Races, ownership and restart | Mutex spans C owner state, copied input preparation and snapshots; Java result allocation occurs after unlock. A deterministic publication hook closes/reopens at allocation, then fails allocation: stale cleanup leaves the replacement live. Managed elapsed clock sampling is serialized, with a blocked-clock two-thread regression. Each owner must close on foreground teardown; no ID, clock or snapshot is restored from Bundle/disk/intent. IDs are process-local lifetime references, not authentication. GC cannot guarantee cleanup of an abandoned owner; exceptional mutex failures may require restart. |
| Secret leakage | Only synthetic/supplied public transaction data crosses this adapter. No seed/private key, custody file, endpoint or source transport. Retained draft and native input copies clear on their respective cleanup paths. Managed snapshots/returned bytes remain caller-owned public copies. Fixtures have no real funding; package isolation keeps transaction assets out of app APKs. TLS/BLAKE2 quarantine and hardware custody policy remain unchanged. |

The expanded test-only seed helper writes fixed filenames with exclusive
`wbx` creation in a caller-created new directory, checks write and close results,
and preserves existing files on refusal. Partial failed fixture generation never
grants application or wallet authority. Both required asset-listing mutations
operate on tool output without altering an APK.

## Canonical transparent address encoding — 2026-09-13

Scope: public C encoding for both transparent destination kinds, and delegation
of the existing P2PKH-only public-hash helper. No new JNI entry or key path.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Encoder copies exactly20 hash bytes from the owned address into a22-byte version/hash payload at offset2. Existing bounded Base58Check encoding checks capacity before publishing any byte or length. No terminator is appended. All35 undersized capacities, exact-size output, canaries and unchanged-failure checks are covered for every original vector. |
| Integer overflow/underflow; signed/unsigned conversions | Network and kind validate before selecting an existing uint16 prefix. High-byte shift and low-byte mask each yield <=255 before uint8 casts. Payload length is fixed22; no input-controlled arithmetic, truncation or money operation. Existing P2PKH wrapper preserves argument/network/hash-length validation order before copying20 bytes into an owned address. |
| Use-after-free; double-free; leaks; dangling pointers; races | No allocation/free, callback, timer, global state or retained pointer. Both functions use small synchronous public stack objects; callers provide stable nonoverlapping input/output spans. Existing hash helper delegates without changing caller ownership. |
| NULL dereferences; uninitialized memory | Address/text/length pointers are checked before use; unsupported network/kind returns before prefix/hash work. Payload and wrapper address initialize before assignment. Failed Base58Check/provider/capacity operations retain the existing failure-atomic output contract. |
| Pointer arithmetic; format strings | Only a fixed offset2 into a22-byte payload is added; the20-byte copy fits exactly. No pointer subtraction, unaligned typed access, string format or input logging. Tests use bounded trusted public hex literals and explicit nibble checks. |
| Stack usage; allocation limits; resource exhaustion | One22-byte payload and one small wrapper address add only fixed storage above the existing bounded Base58Check/hash frames. No recursion/VLA/heap/retry or network operation. Existing4096-byte frame and strict compiler gates remain enabled. |
| Malformed input and compatibility | Only explicit MAINNET/TESTNET plus P2PKH/P2SH combinations encode. Unknown enums refuse without changing output. Four original public address/script vectors independently bind both networks/kinds; all hash-byte values and wrong-network refusal supplement them. The old receiving helper remains P2PKH and emits identical bytes; no wallet record, derivation path or network prefix is changed. |
| Secret leakage and authority | Inputs contain only a public20-byte destination hash and explicit metadata. Encoding proves neither private-key/redeem-script ownership nor inclusion, change status or spending approval. No key export, endpoint, signing/broadcast or custody change. TLS and BLAKE2 findings remain isolated. |

## Canonical address JNI and review display factory — 2026-09-13

Scope: one fixed public-record JNI encoder, a managed P2SH factory and canonical
address derivation from public review destination data. No view is wired yet.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | JNI requires exactly21 record bytes before reading type/hash. The shared region helper rejects count>21 or negative count before copy. Hash copy uses offset1/length20 within that checked record. Output is bounded35-byte text. The managed P2SH factory requires20 bytes before copying to offset1 of a21-byte record; review hash text requires40 characters before hex decoding. Fake VM checks exact regions and output canaries. |
| Integer overflow/underflow; signed/unsigned conversions | Network is checked by the existing JNI network helper. Record type must equal1 or2 before conversion to the enum, avoiding out-of-range enum conversion. All lengths are fixed or checked; no input-sized arithmetic, monetary change, pointer-derived count or unchecked narrowing. |
| Use-after-free; double-free; leaks; dangling pointers; races | No native heap, pins, global references or retained pointer. Input/output records are separate fixed local objects and Java result ownership stays with the VM. The managed factory creates its own21-byte record, so later caller hash mutation cannot change address/script state. Encoding is stateless and supplies no review-lifetime authority. |
| NULL dereferences; uninitialized memory | New entry rejects NULL env and pending exception before VM operations. Shared read helper rejects NULL input and checks length/region exceptions. Record/text/address initialize before use; only complete successful C encoding publishes a result. Fake VM covers partial input writes, length/get/new/set failures, NULL allocation both with/without pending exception, and initially pending exceptions. |
| Pointer arithmetic; format strings | Only fixed offset1/20-byte hash copies and existing bounded encoder operations. No unaligned load, pointer subtraction, format string, input diagnostic or secret logging. Managed decoding uses checked fixed-width public hex text. |
| Stack usage; allocation limits; resource exhaustion | Native scratch is21-byte record,35-byte text and one small address above existing bounded Base58Check frames. No recursion/VLA/native heap/worker/retry. Java creates bounded address text/record objects; malformed oversized inputs refuse before any additional proportional allocation. Strict4096-byte frame and both ABI gates remain enabled. |
| Malformed input and compatibility | Every unknown type byte, unsupported network, bad record size or JNI exception refuses. Original mainnet/testnet address/script vectors independently check both factories. Existing receiving factory remains unchanged. Review destinations map to exact expected P2PKH/P2SH scripts and selected network; there is no network inference from hash bytes. |
| Secret leakage and authority | All data is public destination metadata. A P2SH address does not prove redeem-script possession; no change, funding, finality, consent or signing claim is added. Rendering later must recheck its original foreground review owner and clear on failure/background. No wallet/custody record, endpoint, source transport or key enters this factory; TLS/BLAKE2 quarantine remains unchanged. |

## Bounded unsigned draft construction — 2026-09-13

Scope: fixed public funding/output request, canonical outpoint construction and
existing full assessment before publishing an owned unsigned transaction.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The public request has fixed8 funding and16 output rows. Nonzero counts and both upper bounds validate before indexing. Every previous span carries an explicit length into the existing bounded canonical parser. Selected uint32 output index must be below parsed output_count before use. Address scripts write through the existing capacity-bearing encoder into fixed25-byte arrays. No caller result is written until all checks pass. |
| Integer overflow/underflow; signed/unsigned conversions | Counts use size_t and refuse SIZE_MAX before loops. Index/sequence/lock/expiry remain uint32 without narrowing; the index comparison performs no narrowing and the parsed output count is bounded by16. Fee ceiling and destination amounts remain uint64. Existing assessment checks each sum, subtraction and MAX_MONEY/ceiling bound; constructor adds no separate money arithmetic. Expiry and fee policy refuse out-of-range values before source work. |
| Use-after-free; double-free; leaks; dangling pointers | No allocation/free, pin, handle or retained pointer. Previous bytes are borrowed for this synchronous call only. Private request-derived descriptors stay in one bounded assessment frame. Published transaction owns all fields/scripts/outpoints; tests destroy request/backing source data and retain exact serialized output. |
| NULL dereferences; uninitialized memory | Public entry checks request/output pointers before fields. Internal cross-file helpers check their pointer arguments; parser checks previous wire pointers and lengths. The private transaction initializes to zero before any preparation; inputs never inherit previous scripts. Failed parse/hash/index/script/assessment returns before publication. All used source rows are caller-initialized; unused rows are never read. |
| Pointer arithmetic; format strings | Only indexes within previously checked fixed arrays and existing bounded parser/script helpers. No new pointer subtraction, reconstructed pointer, unaligned load, variadic formatting or user-data log. Test-only byte canary scans use the exact enclosing object size. |
| Stack usage; allocation limits; resource exhaustion | Host request872, funding24, destination40 and owned transaction2200 bytes. Optimized Clang20 host frames are2264 for public construction,2216 for previous binding and1192 for assessment. Separate translation units keep the large preparation frames separate without noinline or relaxed limits; both Android ABIs pass the4096-byte gate. No heap, VLA, recursion, worker, network, unbounded collection or retry. Source parsing is deliberately reused during final assessment and remains bounded to8 sources of<=1925 bytes. |
| Malformed serialization and failure atomicity | Canonical parser rejects unsupported/truncated/oversized sources; selected index must exist. Exact P2PKH/P2SH output destinations must match the explicit requested network. Existing assessment rejects duplicate outpoints, unsupported funding scripts, excessive totals, underfunding and excessive fee. Distinct indexes may share a previous transaction. Whole output/canaries remain unchanged on every refusal, including later-row failures. |
| Races and ownership | Constructor is stateless. Caller must keep request/source spans stable and nonoverlapping with output for the synchronous call; source spans may share bytes. Nothing crosses a callback or process boundary in this C slice. Current review owner/JNI/lifecycle synchronization remains unchanged. |
| Secret leakage and authority | All inputs/results are public unsigned transaction metadata. No entropy, private key, custody record, authenticated ownership, change reservation, chain finality, signing, broadcast or endpoint is added. Raw sequence/lock/expiry and supplied previous bytes do not prove spendability. Legacy/v3/shielded/larger funding remains explicitly unsupported. TLS and BLAKE2 findings remain isolated. |

Exact synthetic draft bytes and independently qualified OpenSSL transaction ID,
both networks, all source truncations, shared/duplicate funding, money/count/
index/network/kind boundaries, maximum rows and smaller replacement cleanup have
deterministic coverage. The structured fuzzer also feeds arbitrary previous wire
bytes and checks request stability, failure atomicity, ordered fields, unsigned
scripts, full assessment and canonical serialization round trips.

## Draft JNI construction and review handoff — 2026-09-13

Scope: stateless bounded public draft construction from Java arrays, followed
by managed opening with the same privately copied sources. No key or consent.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Outer previous/output counts validate1..8/1..16 before allocation or element access. Every previous byte region checks<=1925 and every address checks<=35 before copying. Exact parameter length is3+2*input_count+output_count<=35 before a long region read; the last used index is34. Private field helpers independently check request/count shape before indexing. Status/payload fit a1926-byte array. Fake VM callbacks check actual array capacity independently of claimed length. |
| Integer overflow/underflow; signed/unsigned conversions | Negative and oversized jsize values refuse before size_t casts. Counts are bounded before fixed offset arithmetic or jsize conversion. jlong uint32 fields require0..UINT32_MAX; money requires0..MAX_MONEY; expiry requires<500000000 before construction. Fixed sizeof allocation has no input-sized arithmetic. Existing C constructor/assessment retains all checked sums and fee subtraction. Tests include every parameter's INT64_MIN/MAX, high-bit uint32 values and exact maximum counts. |
| Use-after-free; double-free; leaks; dangling pointers | One checked16272-byte host allocation owns request and all native previous spans. Descriptors point only inside it and remain borrowed until synchronous construction returns. One cleanup tail clears the entire allocation and frees once before Java result allocation. Each element reference deletes after copying, including non-NULL references returned with a pending exception. No pin, global reference, pointer handle or retained request. Fake allocator checks identity, full zeroization and no live allocation/reference at publication. |
| NULL dereferences; uninitialized memory | Entry rejects NULL env and initial pending exception. Required arrays and each inner reference check before reads; every VM length/region call checks exceptions. Native input storage, parameter scratch and result packet initialize to zero. Partially copied fields stay private and are discarded on failure. The transaction is used only after successful failure-atomic construction. NULL allocation and result-publication failures refuse; no partial result packet is returned. |
| Pointer arithmetic; format strings | Only bounded fixed-array indexing and the one-byte status prefix are introduced. Source descriptors use an array row's start and its checked explicit length. No pointer subtraction, reconstructed pointer, unaligned typed access, variadic format or input logging in production. Test fuzz signed-word assembly checks range before casting; its negative conversion avoids implementation-defined unsigned-to-signed overflow. |
| Stack usage; allocation limits; resource exhaustion | Native input storage is one fixed16272-byte allocation; parameter scratch280bytes and packet1926bytes. Optimized Clang20 host JNI entry frame2328bytes; constructor serialization remains a separate translation unit with its2200-byte transaction. Strict4096-byte frame checks, including fake VM tests, and both Android ABIs pass. At most24 local element references are acquired/released sequentially. No recursion/VLA, retry, network, worker creation or growing native collection. Managed counts/sizes bound temporary arrays before copying. |
| Malformed serialization and publication | Addresses reenter canonical C parsing on the explicitly selected network. Public hashes cannot silently coerce another network. Copied funding/index/context/amounts reenter the existing strict constructor and full assessment. Failure packets contain exactly one status byte; successful packets contain only the exact canonical unsigned wire. Managed shape/status decoding rejects malformed packets and clears its temporary JNI packet in finally. No malformed source can reach clock sampling/review opening. |
| Races and ownership | The JNI constructor is stateless and cannot cancel/replace the active review. Java inputs must remain stable for the synchronous copy. Managed preparation serializes factory calls and owns private previous copies through both construction and review opening; mutation of original sources from the clock callback cannot replace them. BUSY and preparation failures preserve the older review. Existing review ID/mutex/lifetime and foreground cancellation remain authoritative; no new handle is persisted or replayed. |
| Secret leakage and authority | Inputs are public previous transactions, canonical destinations and numeric policy/context. Whole native source storage and private managed source/draft byte copies clear on all exits. Managed output addresses, numeric metadata, snapshots and explicitly returned unsigned bytes are public GC/caller-owned values, not promised physical zeroization. No seed, private key, wallet record, endpoint, authenticated funding, change classification, chain finality, signing or broadcast enters this adapter. TLS/BLAKE2 quarantine and hardware policy remain unchanged. |

Deterministic fake VM faults, real JVM checked-JNI cases and actual emulator
construction-to-review lifecycle observations complement this review. The
bounded fuzzer combines complete small/maximum requests on both networks with
signed metadata, arbitrary previous bytes, malformed addresses/counts and VM
read/allocation/publication faults, checking every success and cleanup.

## Pending-exception refusal in secret JNI adapters — 2026-09-13

Scope: shared byte-copy/publication helpers and the existing five key JNI
entries. The fake VM reproduced GetArrayLength with an exception already
pending; early guards now refuse without clearing or replacing that exception.
This is a defensive JNI contract finding, not an observed production crash or
sanitizer memory violation. The exact failing fixture/source hashes are kept.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Existing explicit byte/char capacities remain unchanged. New guards execute before array length, region or allocation calls. Key scratch remains32-byte entropy/blinding,215-byte mnemonic and430-byte UTF16 conversion storage. Fault VM independently checks claimed regions against fixed actual arrays and injects partial writes. No output length publishes on a read failure. |
| Integer overflow/underflow; signed/unsigned conversions | No production arithmetic, cast, count limit or conversion changes. Negative/oversized jsize values still refuse before unsigned use. Test loops use checked<=216-element arrays and bounded region subtraction; fuzz metadata cannot grow native storage. |
| Use-after-free; double-free; leaks; dangling pointers | No new production heap, reference, pin or retained pointer. Existing key cleanup tails still clear every owned secret scratch array on failure. The test tracks pointers seen during RNG/VM read/publication and observes their zeroization while each span is still live; it never inspects a dead stack frame. VM result ownership remains local to the invocation/managed caller. |
| NULL dereferences; uninitialized memory | NULL env/arguments refuse before ExceptionCheck. Entropy generation now refuses NULL env or pending exception before drawing randomness. Phrase readers and publishers check pending state before their first VM call. Existing initialized scratch and deterministic cleanup remain, including partially written byte/char regions, NULL allocation with/without exception and failed output publication. |
| Pointer arithmetic; format strings | No new production pointer operation, offset, format string or log. Test canary/zero scans are bounded by the actual registered span and matching live buffer; no cross-object pointer ordering/subtraction. Diagnostics emit only fixed check locations, never mnemonic/entropy/key bytes. |
| Stack usage; allocation limits; resource exhaustion | Only ExceptionCheck calls and early returns are added. Existing4096-byte frame limits and bounded buffers remain. Test-only RNG substitution emits public bytes and is confined to the fake VM target; shipped CSPRNG and cryptographic providers are unchanged. No retry, exception polling loop, recursion, VLA, native worker or allocation is added. |
| Malformed input and exception handling | Shared read/new-byte helpers refuse a preexisting exception before prohibited JNI operations. Phrase-size/new-phrase helpers do the same. Original exceptions stay pending so the VM handles them; no ExceptionClear or replacement exception is introduced. Region failure may leave partial caller scratch, explicitly documented; secret callers clear it before returning. |
| Races and ownership | Each JNIEnv remains invocation/thread local. No global production state, lock, identity, callback or lifetime changes. Fake VM globals are single-threaded test state only. Existing sync/review fault fixtures and real JVM tests exercise users of the shared helpers. |
| Secret leakage and custody | The fix prevents unnecessary RNG work and further VM calls on a pending exception. Native scratch clears even after publication failure. An unreachable VM array partially filled before an exception remains under VM/GC control; this is not a physical Java-heap erasure guarantee. Device tests keep fresh test entropy in mutable arrays with finally cleanup and no string/log conversion. Only the published zero-entropy phrase is compared as text. Hardware per-use custody, GCM, key profiles and TLS/BLAKE2 quarantine remain unchanged. |

Android's [JNI exception contract](https://developer.android.com/ndk/guides/jni-tips#exceptions)
permits exception inspection and selected cleanup calls while an exception is
pending; array access/allocation/publication calls are outside that set.

## Recovered-wallet internal address binding — 2026-09-13

Scope: public internal address derivation after the existing recovered-header/
entropy check. GCM/hardware authorization remains a platform precondition.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | All header/entropy/blinding spans carry lengths. NULLs, exact64-byte blinding, index and35-byte output capacity validate first. Existing header parser requires exactly80 bytes before field reads; recovery checks entropy length before derivation. The only new span split is offset32/length32 inside the checked64-byte blinding span. Both intermediate addresses are fixed35-byte arrays. Final output copies exactly35 bytes after every check succeeds. |
| Integer overflow/underflow; signed/unsigned conversions | No new money/count arithmetic, allocation math or signed conversion. uint32 index must be below0x80000000; it passes unchanged into existing derivation. Unknown network/profile/entropy sizes fail in existing header validation. SIZE_MAX lengths refuse before indexing. No automatic next-index calculation or invalid-child retry. |
| Use-after-free; double-free; leaks; dangling pointers | The wrapper allocates no heap and retains no pointer. Existing derivation owns and clears/frees one bounded EC context per call; the wallet check completes cleanup before internal derivation starts. Fault tests inspect cleared context storage and balanced allocation/release on failures at all ten child steps across both derivations. |
| NULL dereferences; uninitialized memory | Required pointers check before use. Parsed info and both public address candidates initialize to zero. Header validation and recovered-wallet verification must succeed before internal derivation; the result length must equal35 before publication. Failure never exposes partial candidate or prior caller output. |
| Pointer arithmetic; format strings | One fixed checked blinding offset32 is introduced. Output is a fixed-size memcpy; no cursor, pointer subtraction, unaligned load, variadic format, user data diagnostic or secret logging. Input/output spans must stay nonoverlapping and stable for the synchronous call. |
| Stack usage; allocation limits; resource exhaustion | Adds one small public info structure and two35-byte public address arrays above existing bounded derivation frames. All414 production functions in72files pass complexity<=10 and4096-byte frame checks, including both Android ABIs. Two existing full derivations are deliberate: no retained seed/key cache is introduced. No recursion/VLA, network, retry loop, worker, new native allocation or persistent state. |
| Malformed serialization and failure atomicity | Every header truncation/single-bit mutation, wrong wallet/entropy, invalid length/index/blinding/capacity and NULL input is covered. The network/account/profile comes only from the checked header. Both networks, all five entropy widths and index0/1/2^31-1 produce the same canonical P2PKH result as the existing internal derivation. Output canaries and the whole prefilled result remain unchanged on refusal. |
| Races and ownership | Stateless, with no new callback/ID/mutex/registry. Caller owns stable header/entropy/blinding spans; this API cannot authenticate concurrent mutation or infer that platform GCM succeeded. No receive/change index is advanced, reserved, restored or persisted by deriving an address. |
| Secret leakage and custody | Only a public address is returned. The wrapper adds no secret copy; existing mnemonic/seed/private child/context cleanup is reused without modification. Independent32-byte blinding halves belong to the caller and require cleanup with entropy. GCM and per-use hardware enforcement are unchanged. Success does not establish source inclusion/unspentness, output classification, user consent or signing authority. TLS/BLAKE2 findings remain isolated. |

The fuzzer includes a complete valid recovered-wallet derivation in each input,
then altered and raw headers plus arbitrary bounded metadata, comparing public
results to the existing internal derivation and checking input/output stability.
These fixtures qualify C binding and cleanup; they do not qualify physical
hardware custody, mutable-index recovery or a sending flow.

The JNI key fault fixture also clears each tracked pointer immediately after
observing its live span zeroed. Later cleanup events inspect only the saved
cleared flag, not a pointer whose stack object's lifetime has ended. No native
production path or secret policy changes; the focused ASan/UBSan/LSan JNI key
fixture passes with the same partial-read/publication cleanup assertions.

## Authenticated change-counter codec — 2026-09-13

Scope: fixed content record and private HKDF-SHA512/HMAC key, conditionally bound
to a recovered wallet. No storage, freshness or transaction authorization.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Record length must equal80 before reading its16-byte prefix or64-byte tag. Counter uses four fixed bytes at8..11; reserved ranges6..7/12..15 must be zero. Encode checks80-byte capacity before preparation. HKDF info has compile-time size for its fixed label, exact80-byte checked header and one counter byte. All HMAC key/message/output lengths fit the existing256/512/64 bounds. Output canaries and every truncated/mutated record are exercised. |
| Integer overflow/underflow; signed/unsigned conversions | Counter remains uint32 and must be<=0x80000000. Shifts are unsigned at0/8/16/24; encoding masks to0xff before narrowing. There is no increment, addition of a file offset, generation counter, signed conversion or input-sized allocation. SIZE_MAX and oversize spans fail before copying. The exhausted sentinel is representable but does not authorize derivation. |
| Use-after-free; double-free; leaks; dangling pointers | No new heap allocation, retained pointer or registry. The existing wallet check owns/cleans its transient EC context. Extracted/derived keys are caller/private-stack spans, cleared on every exit; the public API never returns them. Fault instrumentation observes clearing while each span is live and immediately retires the pointer before its lifetime ends. OpenSSL oracle objects have deterministic cleanup. |
| NULL dereferences; uninitialized memory | Public record/index pointers check before use. Existing recovered-wallet validation checks header/entropy/blinding. Private info/prefix/keys/MAC candidates initialize before use. Prefix parsing only writes an internal candidate; external index publishes after full MAC success. Provider failure at extract, expand or tag generation, including injected partial scratch writes, leaves caller results unchanged and clears keys. |
| Pointer arithmetic; format strings | Only fixed checked offsets into80-byte records and a fixed HKDF-info array. Prefix0..15 and tag16..79 are disjoint regions, including when HMAC writes a tag into the same enclosing candidate array. No pointer reconstruction/subtraction, unaligned typed access, input formatting or secret logs. Volatile tag comparison scans exactly64 bytes; its accumulator clears afterward. |
| Stack usage; allocation limits; resource exhaustion | No recursion/VLA, worker, retry, file operation or new allocation. Each operation deliberately rechecks the recovered wallet instead of retaining entropy or a MAC key. Optimized host encode/decode frames152/168bytes; NDK ARM64 frames176/192bytes. Strict4096-byte frame checks and all420 production functions in74files at complexity<=10 pass for the current source. All additional loops and cryptographic message sizes are fixed. |
| Malformed serialization and authentication | Unknown version/profile/reserved fields, wrong lengths and invalid counters refuse. The MAC key binds the exact checked wallet header through HKDF info. Valid state from another wallet/network fails. All640 record-bit edits, header changes, NULL/length/capacity/range boundaries and caller-output atomicity are tested. Prefix validation avoids secret work for malformed structure. |
| Cryptographic use | Existing bounded HMAC-SHA512 implements RFC5869 extract and one-block expand with an independent fixed salt and validated uniform wallet entropy. Full64-byte HMAC authenticates the fixed16-byte prefix. No password KDF, random-nonce assumption, truncated tag or signature scheme is invented. Independent OpenSSL HKDF/HMAC matches40 complete records. Exact optimized x86_64/ARM64 comparison loops inspect all64 bytes before equality; no broader timing proof is claimed. |
| Races, freshness and ownership | Stateless synchronous codec; caller supplies stable nonoverlapping spans. Old authenticated records still verify, explicitly demonstrated. No file freshness, monotonicity, index reservation, rollback protection, seed-discovery completeness or current-chain evidence is inferred. Those belong to the forthcoming durable storage owner and sending acceptance. |
| Secret leakage and custody | Extracted and MAC keys clear on success/error; decoded expected MAC also clears. Only the public counter/tag record or uint32 index publishes. Counter metadata is authenticated, not encrypted. Physical GCM/per-use hardware authentication remains a platform prerequisite; no new JNI secret path, key export, signing/broadcast or wallet mutation is introduced. TLS/BLAKE2 quarantines remain unchanged. |

## Bounded append-only change storage — 2026-09-13

Scope: public-data Android/Linux IO, fresh paired creation, bounded tail
observation and compare-and-append. Caller authentication is mandatory; this
slice does not yet supply an authenticating reservation or repair wrapper.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Wallet parse bounds ciphertext to124..140 before IO. State structural inspection requires exactly80 bytes before fields; it is shared privately with the authenticated codec. Snapshot tail_len must equal80 before append comparison. File observations cap size at5,242,880 before selecting min(size,80); pread always stays inside the80-byte candidate tail. Positive IO counts cannot exceed the remaining span. Outputs publish only a complete owned snapshot after cleanup. |
| Integer overflow/underflow; signed/unsigned conversions | Nonnegative bounded off_t is checked before uint32 conversion. A static assertion keeps the file cap within INT32_MAX. Tail length is<=file size and<=80 before subtraction/narrowing. Start+offset remains<=5MiB before off_t conversion. Append requires size>=80, divisible by80, and below cap before position-1; next position is<=65535. There is no unbounded counter increment, multiplication, signed narrowing or input-sized allocation. SIZE_MAX and UINT32 extremes refuse before use. |
| Use-after-free; double-free; leaks; dangling pointers | No heap, retained descriptor, pointer handle or copied owner. Each invocation initializes directory/lock/state descriptors to-1, transfers ownership immediately on open and closes once on every path. Read and append use the same state descriptor, without close/reopen. Linux/Android close is consumed even when it reports EINTR. Fault tests count actual /proc/self/fd entries before/after refusals and observe no descriptor growth. |
| NULL dereferences; uninitialized memory | Public output/snapshot pointers and wallet/state spans validate before reads. Existing path validation remains. All candidate snapshots, parsed wallet structures, stat structures and byte scratch initialize before use. Partial pread remains private and never publishes on failure. Private helpers have validated caller/descriptor/span preconditions; no nullable pointer is retained. |
| Pointer arithmetic; format strings | Only fixed bounded byte-array offsets, checked pread position and remaining-write spans. Exact wallet comparison length is<=140 and state comparison length80. No pointer reconstruction/order/subtraction, unaligned typed read, variadic format, user-derived filename, secret log or pathname-as-authority fallback. Stable nonoverlapping caller spans and trusted private parent paths are explicit preconditions. |
| Stack usage; allocation limits; resource exhaustion | No recursion/VLA, growing allocation, scan of historical records, worker or network activity. Observations read at most80 bytes plus bounded wallet metadata; file cap5MiB/65536 records is deliberately below derivation exhaustion. IO retries cap256 and fsync16; lock acquisition is nonblocking. FIFO opens are nonblocking and then refused by fstat. Strict4096-byte frame checks and433 production functions in76files at complexity<=10 pass. |
| Malformed serialization and file policy | Empty/partial observations are public recovery metadata and cannot pass normal append. Structural prefix/version/profile/reserved/counter checks do not claim MAC authentication. The caller must reject bad MACs before append. Regular/private/effective-UID/single-link/bounded state files are mandatory; symlink, FIFO, hardlink and loose-permission fixtures refuse. Exact supplied wallet bytes must match committed storage, never an unpromoted pending record. |
| Races and durability | Existing private directory and nonblocking flock cover wallet matching, observation and compare/write. Append compares size and tail under the lock on the same descriptor opened with O_APPEND; stale snapshots return BUSY. File flush/close, directory flush and lock/directory close all precede success. Twelve competing processes with the same observation yield one success. Same-UID malicious mutation/rollback outside cooperative locking remains an explicit limitation. |
| Initialization, interruption and recovery | Both creation routes refuse orphan change state. Fresh paired creation flushes/closes state0 and flushes its directory name before any pending wallet write. Existing wallet missing state returns NOT_FOUND and never resets. Every partial/uncertain artifact remains. A fully appended successor consumes its prior index even if flush/close/publication fails; partial data blocks normal append. Tests reach26 child-process interruption boundaries and inject every flush/close stage; they do not simulate power loss. Explicit authenticated repair, migration and gap-aware seed discovery remain unfinished. |
| Secret leakage, cryptography and custody | These IO APIs receive public authenticated-counter metadata and encrypted wallet records only. They neither derive nor retain entropy, keys or blinding; no new secret copy requires zeroization. Structural inspection is private and explicitly unauthenticated. The secret-facing wrapper must verify GCM/recovered wallet/state MACs and publish a privately derived address only after append succeeds. No JNI/send route, hardware relaxation, TLS/BLAKE2 enablement, rollback-resistance or transaction-approval claim is added. |

## Authenticated change reservation — 2026-09-13

Scope: C composition of the recovered-wallet/state codec, internal address
derivation and durable append. GCM/per-use hardware remains a platform
precondition; no transaction approval or sending route is introduced.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Exact ciphertext length124..140 checks before its copy into140 private bytes; the private copy then passes the existing wallet parser. Entropy length must equal the checked header's16/20/24/28/32 length before any derivation. Snapshot requires tail80, nonempty aligned file size below cap. Helpers use fixed32/64-byte blinding,80-byte records and35-byte public addresses. NULL output and required spans refuse; every caller result byte remains unchanged on failure. |
| Integer overflow/underflow; signed/unsigned conversions | Verified next index must equal file_bytes/80-1 after size>=80 and divisibility checks. Size below5MiB proves index<=65534 before derivation and index+1. No caller-selected index or automatic invalid-child retry. Entropy and ciphertext lengths remain size_t and are bounded before copying. No new signed conversion, unbounded product, monetary arithmetic or input-sized allocation. Final-index65534 succeeds and authenticated65535 at the full cap refuses another reservation. |
| Use-after-free; double-free; leaks; dangling pointers | No new heap, retained key, descriptor or handle. The encrypted wallet copy owns its bytes across all crypto/IO calls. Entropy is a stable borrowed span only for this synchronous invocation. Existing cryptographic context and storage descriptor owners retain their deterministic cleanup. Test blinding observers retire tracked pointers while spans remain live and retain only a cleared flag afterward. |
| NULL dereferences; uninitialized memory | Wallet context, parsed record, public candidate, snapshots and all random scratch initialize before use. Every helper status gates the next operation. A failed decode that writes UINT32_MAX, failed derivation that writes a partial address, or failed encode that writes partial state remains private and cannot reach append/publication. RNG failures, including stronger injected partial writes, clear their whole initialized scratch span. |
| Pointer arithmetic; format strings | Only the checked wallet memcpy and existing fixed-length codec/derivation spans are introduced. The private header is exactly the first80 bytes of the copied validated record. Entropy is never copied, split, formatted or logged by this wrapper. No pointer subtraction/reconstruction, unaligned access or variadic format. Stable nonoverlapping input/output spans remain required. |
| Stack usage; allocation limits; resource exhaustion | A fixed ciphertext context, one80-byte state, owned snapshot and public result remain on bounded stack; each helper owns32 or64 blinding bytes. No recursion/VLA, worker, secret cache, extra allocation or retry. Four existing root/internal derivations are intentionally composed instead of retaining keys. Native strict4096-byte frames and439 production functions in77files at complexity<=10 pass. Crypto and IO remain bounded and run synchronously on a future platform worker. |
| Malformed wallet/state and authentication | The same private wallet bytes bind parse, recovered-root checks, state MAC and final storage comparison. Both networks/all entropy widths, changed ciphertext, wrong entropy, each state-byte mutation, all short initial files and valid MAC at wrong position are exercised. Missing, partial, bad-MAC, misplaced and capped heads refuse, without repair/reset. Structural IO is never used as a substitute for MAC verification. |
| Races, failures and durable publication | Wallet/state observation finishes before secret work; append compares the same snapshot and private wallet under the storage lock. A competing writer during preparation returns BUSY with unchanged result. All cryptographic preparation finishes before append. Result assignment occurs only after append's file/dir/lock cleanup succeeds. A complete uncertain append consumes the index, and a later reservation advances; partial append blocks. Faults cover all ten close and four fsync points across observe/append, plus partial write. |
| Secret leakage and cryptographic use | Existing OS CSPRNG generates independent32-byte decode,64-byte root/internal and32-byte encode blinding. Every blinding span clears on success/error, observed live by source-only test hooks. Existing codec/derivation clears transient seeds, private children, MAC keys and contexts. Only public index/network/address publishes. Tests use public zero entropy and inert ciphertext; no hardware/GCM proof, key export, secret cache, transaction consent, signing/broadcast or quarantine waiver is claimed. |
| Recovery and authority | Fresh create encodes authenticated index0 before paired storage, with no caller-selected counter. Existing/orphan state is never replaced. Cancellation or lost publication after success burns the index. Public reservation metadata is not an authorization receipt or proof of funding/change classification. Explicit append-only repair, migration, unused-gap seed discovery, malicious rollback threat handling and exact sending-review binding remain separate acceptance work. |

## Append-only repair IO — 2026-09-13

Scope: explicit public-data repair planning and append IO. Normal authenticated
reservation never invokes repair. The authenticating recovery caller, including
healthy/misplaced-head and unsupported-format refusal, remains separate work.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Snapshot length must equal min(file_bytes,80) before use. Shared snapshot comparison independently bounds both tails to80 before memcmp. Replacement inspection requires exactly80 bytes. The fixed zero-padding array is80 bytes, and its checked plan permits0..80 bytes. Existing complete-write helper bounds every returned count against the remaining span. No output record is partially published; the planner updates its owned public result only after every check. |
| Integer overflow/underflow; signed/unsigned conversions | File size is uint32 bounded to5MiB before division or ceiling increment. Slots after rounding must be1..65535 before multiplying by80. That product is>=original size and<=cap-80, so padding subtraction cannot underflow and replacement fits the cap. Empty size explicitly maps to slot1. No signed conversion or input-sized allocation is added in production. Boundary fixtures cover all subrecord offsets plus cap-161/-160/-81/-80/-79/-1/cap/+1 and UINT32_MAX. |
| Use-after-free; double-free; leaks; dangling pointers | No allocation, retained descriptor, pointer handle or replacement pathname. Shared descriptor ownership transfers on open and closes exactly once after read/compare/pad/write/flush failures. Linux/Android close is never retried. Repeated repair uses a new complete owned snapshot, not a pointer into an old file mapping. Fault fixtures observe unchanged /proc/self/fd counts. |
| NULL dereferences; uninitialized memory | Required planner pointers check before access. Wallet and replacement spans enter existing bounded parsers. Plan, parsed wallet, observed snapshot and all padding initialize before use. A failed plan/parse prevents descriptor acquisition and writes. Partial IO remains in the append file as explicit evidence; no uninitialized bytes are written as padding or replacement. |
| Pointer arithmetic; format strings | Only checked bounded spans into the80-byte padding/replacement and existing IO cursor are used. No pointer reconstruction/order/subtraction, unaligned typed loads, formatted paths or secret logs. All previous file bytes are left in place. Caller-owned spans remain stable and nonoverlapping for the synchronous call. |
| Stack usage; allocation limits; resource exhaustion | No recursion/VLA, growing buffer, file scan, worker or heap allocation. New production repair adds fixed80-byte padding and small metadata over existing bounded wallet/tail readers. At most160 bytes can be appended per call, within5MiB; IO retries and nonblocking lock behavior are unchanged. Strict4096-byte frames and445 production functions in78files at complexity<=10 pass. |
| Malformed files and CAS | File policy reuses regular/private/effective-UID/single-link/size checks on the same descriptor used for append. Exact committed wallet and observed size/tail must match under one lock. Missing state refuses and cannot initialize. A replacement with wrong structural counter/version/length refuses before mutation. Low-level IO deliberately does not authenticate MACs or determine whether a head should be repaired; those are mandatory recovery-caller duties. |
| Partial writes, repeated recovery and durability | Padding precedes the replacement and neither overwrites bytes. Failure in either write preserves the original prefix and any completed padding/partial replacement. A subsequent explicit attempt rounds the new length upward and advances conservatively. File fsync/close, directory fsync and owner closes precede success. All160 short initial lengths, padding/replacement fault stages and12 process interruptions preserve prior bytes. No power-loss simulation or malicious-rollback proof is claimed. |
| Races and test fault scope | Snapshot comparison is shared with normal append, preserving its existing CAS refusal. Stale repair cannot overwrite a competing record. Test-only partial-write injection can now begin at a selected ordinal and remains an error afterward; the older first-call mode is unchanged. All prior storage/reservation fault targets remain in the full safety gate. No production syscall replacement or security assertion is weakened. |
| Secret leakage and authority | Repair receives public metadata and authenticated replacement bytes only, and introduces no secret material to clear. Empty existing state burns at least0; missing state never authorizes reset. Healthy or authenticated inconsistent heads require refusal by the forthcoming authenticating recovery API. This primitive returns no address, reservation, signing authority or consent. Normal reservation, GCM/hardware policy and TLS/BLAKE2 quarantine remain unchanged. |

The IO plan is bounded arithmetic, not proof of the historical consumed-index
bound. An empty/short initial file with a committed wallet may mean lost history
and cannot be repaired safely merely by padding it. The authenticating recovery
caller must require verified predecessor/profile/position evidence for a
recognized interrupted suffix; ambiguous loss and unsupported formats remain
preserved/refused pending independent discovery. No such caller or JNI route is
introduced by this IO checkpoint.

## Bounded recovery predecessor probe — 2026-09-13

Scope: read-only public metadata needed for the future authenticated recovery
policy. The probe performs no MAC verification, repair or index publication.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Shared pread helper explicitly requires length<=80 and start<=cap-length before any read. Tail length remains min(size,80); predecessor is a separate owned80-byte array. Offset and returned counts remain within the validated span. Probe publishes its whole owned snapshot only after both reads, metadata recheck and all closes. Tests check321 exact file lengths and every output byte/absence field. |
| Integer overflow/underflow; signed/unsigned conversions | File size is already nonnegative uint32 bounded to5MiB. Only size>80 enters predecessor arithmetic: (size-1)/80>=1, then (slot-1)*80 fits uint32 with end<=size. Range subtraction occurs only after length<=80; start+read offset stays within the cap before off_t conversion. No unchecked signed count conversion, user-sized allocation or growing counter. Full-cap fixtures verify final offsets. |
| Use-after-free; double-free; leaks; dangling pointers | One invocation owns directory/lock/state descriptors. Tail and predecessor use the same open descriptor, with no close/reopen or retained buffer/pointer. Every failure closes each owner once; Linux/Android EINTR close is consumed. Source-only fault tests count descriptors and verify unchanged outputs after cleanup failures. |
| NULL dereferences; uninitialized memory | Public output and wallet spans validate before use. Parsed wallet, current/probe snapshots and stat structures initialize. Files<=80 keep has_predecessor false and predecessor bytes zero. Partial reads remain private and are never published. A failed metadata call cannot expose fields that fstat did not fill. |
| Pointer arithmetic; format strings | Only checked byte-array offsets and one bounded predecessor position are introduced. No pointer reconstruction/order/subtraction, unaligned loads, formatted pathname or input logging. Fuzzer zero-length input uses a valid one-byte backing object, avoiding NULL pointer arithmetic. All returned data is copied into caller-owned structs. |
| Stack usage; allocation limits; resource exhaustion | No heap, recursion/VLA, file scan, worker or secret cache. At most160 state bytes are read per probe, plus the existing bounded wallet record. Existing normal observe still reads<=80. Read retries remain<=256 per span and locks stay nonblocking. Strict4096-byte frames and448 production functions in78files at complexity<=10 pass. |
| Malformed files, races and failure atomicity | Existing nofollow/nonblock/private/regular/effective-UID/single-link/size checks remain shared. After predecessor read, fstat and size comparison run again. An injected size change returns BUSY with unchanged output; both reads and all six stat/five close points have fault coverage. Cooperative writers use the same lock. This does not claim protection against malicious same-UID mutation or rollback. |
| Secret leakage, authentication and authority | Inputs/output are ciphertext and public state metadata only; no new secret is copied or retained. MAC, supported profile, predecessor/current positions and consumed-index bound must be verified by the recovery caller. Probe success never repairs a file, initializes missing state, returns an address, establishes freshness or grants transaction consent. GCM/hardware and TLS/BLAKE2 quarantine remain unchanged. |

## Authenticated suffix recovery — 2026-09-13

Scope: selective C recovery after a verified predecessor and recognized current
prefix. Shared invocation-private custody helpers preserve existing reservation
behavior. No JNI, signing, discovery or hardware-policy change is introduced.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Shared preparation checks record length 124..140 before copying into its fixed private ciphertext array, then parses that copy and requires the exact header entropy length. Current decoding requires an 80-byte tail. Repair shape requires a predecessor, size greater than 80 and a fragment of 16..80 bytes. Only then is the 16-byte public prefix compared at tail offset 80-fragment. Predecessor and successor each use fixed 80-byte codec spans. No caller output is added. |
| Integer overflow/underflow; signed/unsigned conversions | The existing checked plan bounds next_index to 2..65535 for this caller, before subtracting 1 or 2 for current/predecessor positions. Shape checks precede fragment subtraction and pointer offset. Healthy-head position subtraction follows successful exact-tail authentication and the probe's size invariant. File arithmetic stays uint32/size_t below 5 MiB; no new signed conversion, unchecked product, growing counter or input-sized allocation. |
| Use-after-free; double-free; leaks; dangling pointers | The local custody context owns copied ciphertext and borrows stable entropy only until synchronous return. It is not retained, exported or copied into an unlocked-wallet registry. Existing codec context and IO descriptor cleanup remain authoritative. The refactor adds no heap or descriptor owner. Fault observers clear tracked pointers while blinding spans are still live and retain only cleared flags afterward; descriptor counts remain unchanged on failures. |
| NULL dereferences; uninitialized memory | Public wallet/entropy spans are checked by preparation, and the trusted directory enters existing storage validation. Every private context, snapshot, plan and record initializes before use. Private helpers receive non-NULL initialized local owners and are called only after successful preparation. Each status gates the next step. Failed RNG/codec calls with partial scratch writes cannot reach repair, and all random scratch clears. |
| Pointer arithmetic; format strings | The only new offset selects a supported fragment within a checked 80-byte owned tail. No pointer reconstruction, subtraction, ordering, unaligned typed access, formatted input or secret logs. MAC bytes are never compared through this new memcmp: it covers only the 16 public format/counter bytes. Full MAC verification remains in the existing constant-time codec comparison. |
| Stack usage; allocation limits; resource exhaustion | Fixed private ciphertext, owned recovery snapshot, checked plan and 80-byte replacement; helper scratch is 32-byte blinding or an 80-byte expected public record. No recursion/VLA, heap allocation, history scan, worker, retained secret or retry. Probe reads at most 160 state bytes; repair reads its bounded CAS tail and appends at most 160 bytes within the existing cap. Crypto calls and IO retries remain bounded. Strict 4096-byte frame checks and the production complexity gate apply. |
| Malformed wallet/state and authentication | A current valid MAC must be aligned at its exact position; a misplaced authenticated tail refuses immediately. Damaged structure/MAC permits only further qualification. The immediate predecessor must authenticate against the recovered wallet at next_index-2, and the current supported public prefix must match next_index-1. Wrong entropy cannot grant repair because predecessor verification and successor encoding independently check the recovered wallet. Unsupported formats, incomplete prefixes, missing/short initial state and exhausted capacity refuse. |
| Cryptographic use and secret leakage | The shared helpers retain the same fresh OS blinding and codec calls as reservation. Recovery uses independent 32-byte blinding for each of two decodes and two encodes, cleared on every exit. Four source-only RNG/codec failure points exercise partial scratch output and live clearing. Existing codec clears seed-derived private keys, MAC keys and contexts. New expected/replacement records and copied ciphertext are public/encrypted data; no secret copy or key export is added. |
| Races, failure publication and durability | Probe and final repair match the same private ciphertext. Repair compares original size/tail under its lock on the descriptor used to append. A competing repair returns BUSY; source-only mutation of the caller ciphertext after copying does not change the bound identity. Success waits for all existing file/directory flushes and descriptor cleanup. No address, index or authorization result is returned. Cooperative locking does not protect against malicious same-UID interior mutation or old valid snapshots. |
| Interruption, recovery and authority | Every original byte remains, including padding and partial replacements after failure. A complete successor remains consumed. An incomplete replacement after an invalid predecessor is deliberately refused on another authenticated attempt; raw repair capability does not authorize that retry. Eighteen process boundaries and all ten close/four flush stages exercise these distinctions. Missing or ambiguous history needs independent discovery/review; no rollback, format migration, power-loss proof or seed-discovery completeness is claimed. Exact-record GCM/per-use hardware remains mandatory, and TLS/BLAKE2 remain quarantined. |

## Android fresh paired creation — 2026-09-13

Scope: one JNI creation entry point, a thin managed adapter and explicit platform
CREATE/RESTORE dispatch into already qualified C persistence. No existing wallet
or restoration counter is reset, and hardware policy is unchanged.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Shared JNI byte reads check signed VM lengths against fixed path1024, record140 and entropy32 capacities before copying. Each array read has an explicit returned length. The existing C creation owner then validates exact record/header/entropy bounds. No Java array pin or native output buffer is added; the result is a scalar status. Fake negative/oversized lengths, NULL arguments and six array-access failures are covered. |
| Integer overflow/underflow; signed/unsigned conversions | Negative jsize values refuse before size_t conversion; bounded positive lengths cannot exceed stack arrays. Status values already fit jint. No new counter arithmetic, unchecked product, narrowing of an input index, or input-sized allocation. Both networks/all five entropy widths reach the existing checked C state0 encoding. |
| Use-after-free; double-free; leaks; dangling pointers | Path/ciphertext/entropy copies belong to one JNI invocation. All core calls are synchronous and retain no pointer. No heap, global reference or new descriptor owner is introduced. The existing C storage owner closes its descriptors; the platform worker retains sole managed entropy ownership. Native fault observers retire the secret pointer immediately after checking its live span cleared. |
| NULL dereferences; uninitialized memory | JNI helpers handle NULL env/input before VM use. All arrays and lengths initialize to zero. Each failed read prevents later reads and the core call. Whole entropy32 clears even if a partial region read throws; no uninitialized tail can enter crypto because the returned length publishes only after a successful read. Pending exceptions remain pending and prevent forbidden VM operations. |
| Pointer arithmetic; format strings | This adapter introduces no byte offset, pointer reconstruction, pointer ordering or formatted input. Captured arrays are passed with their checked lengths. A fake callback mutates the original ciphertext after its copy; the core still receives and stores the original captured record. No secret, path or ciphertext enters status errors or logs. |
| Stack usage; allocation limits; resource exhaustion | Fixed arrays total 1196 bytes plus lengths/status; no VLA, recursion, heap allocation, loop, worker or retry is added in JNI. Existing C derivation/storage remain bounded. This call runs on the existing platform worker. A scalar result avoids a post-commit VM allocation failure path. Strict 4096-byte frame and complexity gates apply. |
| Malformed wallet/state and VM boundary | Shared JNI helpers stop on oversize, negative, NULL or pending-exception input. C still parses the record, verifies recovered wallet identity, encodes authenticated state0 and performs paired creation. Wrong entropy creates neither wallet nor change files. Existing wallet/pending/orphan state refuses without replacement. Bounded fake-VM fuzzing mutates lengths, records, entropy, NULLs and partial-read exception ordinals under sanitizers. |
| Races, lifecycle and failure recovery | The existing worker serializes setup and owns managed entropy until its finally block; JNI takes private copies. Closing discards queued work and suppresses UI delivery, but an active persistence call can finish. Its durable/uncertain artifacts remain for restart inspection; no failed status retries creation or resets state. Existing paired C write/flush/lock/CAS and process-interruption acceptance remain authoritative. |
| Secret leakage and cryptographic use | JNI clears all 32 native entropy bytes on success, VM failure and core refusal. It never clears or modifies the caller's managed array; that owner still clears it after the operation. No new cryptographic algorithm or retained key is introduced. Fresh per-use platform GCM precedes creation, C verifies the recovered root and creates its own state MAC with fresh blinding. Public fixture GCM tests do not claim TEE/StrongBox custody. |
| Restoration and authority | Exhaustive platform routing permits paired state0 only for CREATE, uses wallet-only persistence for RESTORE and refuses UNLOCK as creation. Restored seeds may have historical change use and require discovery before any initial counter is authorized. JVM tests inspect actual files for all three actions; opt-in hardware-flow assertions now include state presence/absence but remain unqualified on the software emulator. No reservation/recovery JNI, signing, broadcast, consensus or quarantine change is included. |

The initial JNI fuzzer exposed a fixture-isolation defect: truncating the owned
absolute directory path selected a different valid directory. The assertion,
exact source/binary, input and public fixture artifacts remain preserved. The
corrected harness admits only its full owned locator or lengths rejected before
filesystem access (0, -1, 1025), with a pre-call invariant. A registered sanitizer
regression replays the original input, all 256 path selectors, a successful
paired write and all six VM exception ordinals. Refused calls also require the
pending wallet file to remain absent. Production path semantics are unchanged.

## Test and fuzzer complexity cleanup — 2026-09-13

Scope: finish the inherited fixture refactor and enforce the existing test
complexity cap in `check-c-safety.sh`. No production C or assertion is changed.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Extracted helpers receive the same bounded arrays, counts and capacities after the same guards. The amount helper uses the explicit 20-byte extent instead of sizeof on its pointer parameter. Counter decoding receives four bytes only after the six-byte input minimum. Canary, capacity, failure-atomicity and exact-output assertions remain present. |
| Integer overflow/underflow; signed/unsigned conversions | All arithmetic, casts, signed-delta bounds and count checks are preserved. The counter helper shifts uint32 bytes by 0, 8, 16 and 24. No new narrowing, input-controlled product or unchecked subtraction is introduced. |
| Use-after-free; double-free; leaks; dangling pointers | Helpers borrow caller-owned fixture spans only during synchronous calls. None retains pointers or acquires heap/descriptor ownership. JNI reference release, live secret-scratch checks and final cleanup remain in their original lifetime. |
| NULL dereferences; uninitialized memory | Existing preconditions and initialization stay before helper calls. Public success reports are checked before traversal. JNI destination pointers and signed lengths are still checked before decoding. Failed snapshots retain full byte-for-byte sentinel assertions. |
| Pointer arithmetic; format strings | Existing checked offsets are preserved. No new pointer reconstruction, ordering, input formatting or logging. The moved JNI wire mutation retains its size cap before copying from offset two. |
| Stack usage; allocation limits; resource exhaustion | Small synchronous helpers replace oversized control-flow bodies; no recursion, VLA, new allocation or unbounded loop. Existing fuzz input/event caps and fixture static scratch ownership remain unchanged. All 761 fixture functions in 101 files are observed at complexity <=15; all 457 production functions in 80 files remain <=10. Neither cap nor baseline was raised. |
| Malformed input; races; secret leakage | Every malformed-input, stale-owner, exact-accounting, exception and zeroization assertion is retained. Fixture state remains host-only and single-threaded where documented. No runtime lock, secret lifecycle, storage policy, cryptographic operation, consensus predicate or custody authority changes. |

All 59 native tests pass with ASan/UBSan/LSan, and authored Clang/GCC analysis
passes. Separate bounded fuzz campaigns exercise every changed fuzz translation
unit against copies of the existing public corpora; evidence is recorded in
the accompanying progress entry. This is fixture acceptance, not hardware
custody, chain validation or general security certification.

## Scanner JNI exception boundary — 2026-09-14

Scope: pending-exception/NULL-environment guards on `packCameraPlane`,
`scanCameraPacket` and `scanQr`, plus a registered fake-VM fixture.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | No production copy, capacity or image-layout arithmetic changes. Entry refusal precedes direct-buffer/array access. The fixture supplies backed public spans, checks region lengths against capacity, checks output canaries and exact packet/request bytes, and compares every input byte after each call. |
| Integer overflow/underflow; signed/unsigned conversions | Guards add no arithmetic or casts. Existing negative jsize/jlong and length/capacity checks remain. Fixtures cover negative, empty, short and INT32_MAX array lengths and invalid direct capacities; valid fixture dimensions are proven <=147 before jint conversion. |
| Use-after-free; double-free; leaks; dangling pointers | Existing single-owner allocations and cleanup paths are unchanged. Source-only allocator hooks track one live allocation, inspect every byte for zero while it is live, free once and immediately retire its pointer/length. No production pointer/reference is retained. |
| NULL dereferences; uninitialized memory | The environment NULL check short-circuits before dereferencing the VM table. Every pending exception remains pending and prevents all ordinary JNI calls and allocation. The fixture fills allocations with nonzero bytes before use, injects partial array reads and checks whole-allocation clearing. |
| Pointer arithmetic; format strings | No new production offsets, pointer reconstruction or formatting. Fixture painting stays within fixed checked image dimensions; JNI output bounds precede indexing. Failures print only a fixture line number, never camera data. |
| Stack usage; allocation limits; resource exhaustion | Guards allocate nothing and add no loop or retained state. Existing frame/packet/raw-image caps remain. Public test backing arrays use documented single-threaded static storage; the module matrix fits the strict 4096-byte frame limit. The test observes at most one invocation allocation and injects allocation refusal. |
| Malformed input; races; VM failure atomicity | Registered tests exercise all four packing and five decoding VM operations, including every exception ordinal, pre-existing exceptions and NULL entries. No JNI call follows a pending exception. Output publication failures return NULL, input arrays remain unchanged, and owned native memory clears. No production synchronization or lifetime contract changes. |
| Secret leakage and authority | Scanner data is public request/image input. No wallet key, custody operation, network source, signing, consent or consensus behavior is introduced. Tests generate an unfunded public receiving QR and inspect native cleanup; they do not claim erasure of VM/Binder/provider copies or positive hardware custody. |

The registered test fails against the prior scanner entries at the first VM
operation with a pending exception, and passes after the guards. All 60 native
ASan/UBSan/LSan tests pass in 44.23 seconds; Clang/GCC analysis passes with 457
production functions <=10 and 780 fixture functions <=15. Actual VM/device
and bounded QR fuzz evidence are recorded separately in the progress log.

## Amount and sync JNI pending exceptions — 2026-09-14

Scope: reuse the checked byte-array helpers for amount parsing/formatting and
refuse pending exceptions before either sync snapshot reads its native owner.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Amount parsing keeps a fixed 17-byte array and formatting retains 17/18-byte arrays. Shared helpers check VM counts before copying and jsize limits before publication. New fixtures retain input comparison, output canaries and untouched bytes past the result. Sync's existing 10/156-long packet bounds are unchanged. |
| Integer overflow/underflow; signed/unsigned conversions | Only shared-helper length refusal maps back to INVALID_ENCODING, preserving the existing JNI length status. Core amount overflow remains OUT_OF_RANGE. Valid money fits positive jlong; signed delta checks and INT64_MIN/MAX refusals stay in the core. No new monetary arithmetic, multiplication or narrowing. Independent exact strings and signed status fixtures cover zero, one zatoshi, maximum and overflow. |
| Use-after-free; double-free; leaks; dangling pointers | Amount paths retain only invocation stack bytes and VM-local output references. Removing the duplicate formatter adds no heap/owner. Fake arrays are bounded public host-only state. Sync guards run before registry lock/borrow, so refused calls retain neither a lock nor an owner pointer. |
| NULL dereferences; uninitialized memory | Shared byte helpers reject NULL environment/input and pending exceptions before VM access. Arrays and returned lengths initialize. Sync checks NULL before ExceptionCheck. Length/region/publication faults remain pending and stop further VM calls; no failed read can reach parsing. |
| Pointer arithmetic; format strings | No new production offsets or format operations. Test strings are fixed public literals with length checks before copying. Diagnostics contain only fixture line numbers. |
| Stack usage; allocation limits; resource exhaustion | Production removes one duplicate helper and introduces no loop, worker, native allocation or retained data. Existing fixed frames and packet bounds remain. Fake-VM tests observe no allocation/access for pending calls, failed Java allocation and partial publication. Complexity caps are unchanged. |
| Malformed input; races; native lifetime | Parse fixtures cover negative/empty/oversized lengths, malformed strings and every VM read fault; format fixtures cover each VM publication fault and allocation refusal without an exception. Pending sync reads use a time at/after expiry, then a valid earlier read proves that the refused call did not advance or expire the owner. Both history modes exercise this; the fuzzer also injects pending snapshot reads at INT64_MAX. |
| Secret leakage and authority | All affected data is public amounts and unverified sync metadata. No secret, key, custody policy, network endpoint, signing, consensus or interpretation of verification status changes. Byte helpers keep their existing pending-exception semantics. |

Both new regressions fail on the preceding JNI implementations and pass after
the changes. Final native, VM/device and fuzz observations are recorded in the
progress log; no broader wallet or hardware qualification is inferred.

## Public camera scene generator — 2026-09-14

Scope: host-only fixed PNG scene for actual emulator camera-to-review testing.
No Android production C source or provider changes.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Rendering requires exactly 921600 bytes. The QR side is checked in 1..49 before scale-six loops. Center x=180 bounds columns to 33..326; vertical centering bounds rows to 93..386, within 640x480 RGB. Each black module writes three bytes per proven pixel. The writer takes an explicit length and requires the same extent. |
| Integer overflow/underflow; signed/unsigned conversions | All dimensions and scale are fixed. The checked side keeps signed products and centered offsets nonnegative and below 640/480; only these values convert to size_t. Fixed allocation and row products fit both 32-bit and 64-bit size_t. The 924486-byte IDAT length fits uint32; a chunk's length is checked before subtraction/narrowing. Adler residues stay below 65521, so each addition is below 131041 and the final shift fits uint32. CRC operations use unsigned shifts/XOR. No input-supplied dimension or arithmetic. |
| Use-after-free; double-free; leaks; dangling pointers | Main owns the one pixel allocation and frees it once after the synchronous renderer/writer. QR arrays are borrowed only during that invocation. A successful exclusive fopen has exactly one fclose even after a short write. No retained pointer or asynchronous generator operation. |
| NULL dereferences; uninitialized memory | Allocation and arguments are checked before access. QR work arrays initialize to zero. The complete RGB extent initializes white before module painting; only a successful render reaches the writer. The complete row scratch initializes; each row replaces exactly its 1920 RGB bytes, preserving its fixed block/filter bytes. CRC/Adler words initialize and remain invocation-owned; helper pointers borrow these proven live objects. |
| Pointer arithmetic; format strings | Pixel offsets use the established fixed dimension bounds. Header and diagnostics are fixed strings; neither argv nor pixels are format strings. argv[1] is read only after argc==2. |
| Stack usage; allocation limits; resource exhaustion | Two version-eight QR work arrays and the separate writer's 1926-byte row scratch fit the 4096-byte compiler frame limit. Rendering, 480 row writes and eight CRC bit steps per byte have fixed finite bounds and no recursion/VLA. The only heap allocation is 921600 bytes. No intermediate codec allocation. File writes and close are checked; a file-size-limit fixture forces a short write and observes failure. |
| Malformed input; races; secret leakage | The only external input is an explicit new output path. Exclusive creation refuses existing paths, including symlinks, without deleting or overwriting anything. Public address/amount/label are fixed unfunded fixtures. The single-threaded generator never opens a wallet, node, network connection or Android resource. No parser, key material, mutable shared state or new authority. |

Strict compilation, separate Clang/GCC analysis, ASan/UBSan/LSan execution,
identical repeated output, existing-file refusal and forced short-write refusal
pass. An independent image reader decodes the PNG, and its signature/IHDR match
the original encoder's fixed reference. All 61 native tests pass in 44.72 seconds
with the unchanged safety caps.
An initial PNG experiment was removed after standalone analysis reported two
possible zero-allocation writes in the general repository PNG provider. That
provider is not part of this generator or APK; its generic API has not been
qualified by the fixed-size experiment. The analyzer traces are retained in
`.cache/android-wallet/camera-scene-20260914/png-analysis-paths.txt`. No finding
was suppressed and no platform provider source changed. The PPM alternative was
rejected by the emulator, which substituted a default scene; the final writer
emits only this fixed PNG fixture, not a general image-encoding API.

## Wallet-header JNI secret cleanup fixtures — 2026-09-14

Scope: extend the existing host key-entry fault harness and fuzzer to
`createWalletHeader` and `recoveredWalletAddress`. Production C, serialized
records, RNG and custody policy are unchanged.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Fake arrays retain 216-byte/216-jchar backing storage. Region access checks signed start/length, declared extent and physical extent before copying. Fuzz input is 2..217 bytes, so header copies are at most 215 bytes; the existing JNI 80-byte capacity refuses oversized headers. Entropy mutation remains at most 33 bytes with source bytes available. Input snapshots compare fixed complete objects, including on deliberately invalid declared lengths. |
| Integer overflow/underflow; signed/unsigned conversions | Fuzzer size is checked before subtracting two or narrowing to jsize. Operation modulo is bounded by seven. Header offsets are fixed below 80. Signed malformed lengths never determine a host copy without region validation. No new production arithmetic, format or length rule. |
| Use-after-free; double-free; leaks; dangling pointers | No new heap allocation or release. Three fake input snapshots live only in the synchronous runner. Secret span observations are retired while the zero hook still has a live pointer; post-return verification reads only cleared flags. Four separately compiled mutants omit one entropy/blinding clear in each entry and fail that assertion without dereferencing expired stack pointers. |
| NULL dereferences; uninitialized memory | NULL environment, NULL entropy/header, pending exception, every VM fault ordinal and failed allocation without an exception are exercised. Fake inputs/results and the cached public header initialize completely. RNG failure now dirties one output byte before refusing, so cleanup also covers partial provider output. Ordinary fake VM calls reject a pending exception. |
| Pointer arithmetic; format strings | Only validated fake-array offsets and bounded input slices are added. Diagnostics remain fixed public strings and line numbers. No pointer is constructed from an integer or serialized field. |
| Stack usage; allocation limits; resource exhaustion | The runner adds three fixed fake-array snapshots (1320 bytes on this host), below the unchanged 4096-byte frame warning limit. The public zero-entropy header is derived once per single-threaded fixture process, then copied from an 80-byte cache. No recursion, worker, VLA or production allocation. Fuzz input, per-case deadline and RSS limits remain explicit. Production/test complexity caps remain 10/15; result checks were split when the extended helper first exceeded the fixture cap. |
| Malformed serialization; races; native lifetime | Header length/field damage, mismatched entropy, invalid network, partial JNI reads/publication and RNG refusal all preserve caller inputs and clear touched secret spans. Cached fixture state is host-only and used synchronously; production receives no new mutable state or retained owner. The object-array pack/unpack entries are linked but are not claimed as covered by this key-entry harness. |
| Secret leakage and authority | Entropy and blinding are observed separately from public header/address bytes; the five existing key entries retain their prior scratch-clearing checks. Only fixed public vectors and generated fuzz bytes enter the harness or its snapshots. No real wallet, key, datadir, device authentication, signing or consensus authority is exercised. Header consistency tests do not establish GCM authentication; actual Android record tests remain a separate claim. |

All 61 native ASan/UBSan/LSan tests pass in 44.49 seconds. Enabled-provider and
authored-source analysis pass; the changed fixture separately passes Clang and
GCC analysis in both unit and fuzz modes. All four omitted-cleanup mutants are
rejected. Evidence is under `.cache/android-wallet/jni-header-20260914`.

## Wallet-record JNI arrays and references — 2026-09-14

Scope: a host fault fixture/fuzzer for record pack/unpack, plus an in-memory
Android record/GCM fixture. Production C and record/custody rules are unchanged.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Fake objects contain 141 bytes and exactly four child slots. Byte-region callbacks validate signed offsets/counts against both declared and physical bounds. Object writes require an index in 0..3. Fuzz size is 2..143 before any payload copy, bounding it to 141 bytes. JNI capacities remain 80/12/48/140. All ten network/entropy profiles have exact byte and component checks. |
| Integer overflow/underflow; signed/unsigned conversions | Checked positive input size precedes subtraction/narrowing. Profiles are bounded by ten and derive only 16..32-byte entropy / 32..48-byte ciphertext sizes. Negative, zero, adjacent and INT32_MAX fake lengths are deliberate unit inputs; no unchecked count reaches a callback copy. Result verification checks positive lengths/capacity before byte comparisons. No new production arithmetic. |
| Use-after-free; double-free; leaks; dangling pointers | Six fixed fake objects model one class, one container and four byte arrays without a heap. Live local handles are distinguished from child objects retained by the returned container. Deleting a temporary does not destroy its retained child bytes. Every JNI use requires a live handle; duplicate/foreign deletion fails. At most two new locals may coexist and success leaves only the returned root. Remaining exception-path locals belong to the JNI return frame, modeled by resetting the fixed pool before the next invocation. No native pointer escapes. |
| NULL dereferences; uninitialized memory | Fake state, inputs, references and cached public headers initialize. NULL environment and each NULL input refuse; pending entries make no ordinary VM call. Every VM fault ordinal is injected, including NULL without exception and non-NULL with exception at class/array allocation. Partial read/publication faults are modeled. An output is inspected only after a non-NULL result. |
| Pointer arithmetic; format strings | All offsets use proven backing-array bounds. Reference identity is compared against each occupied slot, without relational arithmetic on unrelated pointers. Diagnostics are fixed strings/line numbers. No input is treated as a format string or address. |
| Stack usage; allocation limits; resource exhaustion | Four fixed input snapshots fit the unchanged 4096-byte frame limit. The six-reference pool and ten 80-byte header fixtures are fixed process-local host state. Header derivation runs once per profile in a single-threaded process. No new heap, recursive parser, VLA, native worker or unbounded loop. VM-call expectations are eight for pack and sixteen for unpack; no calls continue after a pending fault. Complexity caps stay 10/15. |
| Malformed serialization; races; native lifetime | Unit cases cover every supported profile, malformed lengths/header fields, all JNI callback failures and exact component/network projection. Input snapshots remain byte-identical on success/refusal. The fuzzer bounds every packet/operation and checks accepted projections against the C codec. No fixture uses shared concurrent writers or changes production serialization. |
| Secret leakage and authority | Header fixtures use public zero entropy and fixed blinding. Host ciphertext bytes are structural test data, not authenticated records. The separate Android fixture uses a public AES test key, actual provider IVs/GCM, clears recovered entropy in finally and opens no wallet directory/Keystore alias. It proves tag refusal separately from structural parsing. No custody policy, signing, real funds, endpoint or consensus predicate changes. |

All 62 native ASan/UBSan/LSan tests pass in 44.69 seconds. Clang/GCC analysis
passes for enabled production code and separately for both modes of the new
fixture; all 832 fixture functions remain within the existing cap. Three
isolated mutants omit the class-local release, part-local release, or pending
exception check; each fails the intended fixture. Bounded record JNI fuzzing
completes 376,843 runs in 121 seconds without a finding. Architecture passes.
Android build/lint and real-VM observations are recorded in the progress log.
Evidence is under `.cache/android-wallet/jni-record-20260914`.

## Public QR/payment JNI fault fixtures — 2026-09-14

Scope: add a single-threaded host fixture/fuzzer for the existing receiving QR
and payment adapters, plus real Android public-packet tests. No production C,
parser, provider, protocol, custody or consensus change.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Each fake array has 1682 backing bytes. Read callbacks require offset zero, count 0..1024 and exact agreement with the declared source length. Publication requires a positive count within the backing extent and exact agreement with allocation. The production QR/payment input caps remain 35/1024 and output caps 1682/449. Two uint64 canaries surround the fixed result; bytes beyond successful output retain their marker. Both networks/types, full 200-byte label/message fields and malformed declared lengths are exercised. |
| Integer overflow/underflow; signed/unsigned conversions | Fuzz size is checked in 4..1029 before subtracting four, copying at most 1025 bytes or narrowing to jsize. Negative and INT32_MAX declared lengths never enter a copy. The fixed maximum-payment prefix is shorter than 64 bytes; adding two 200-byte fields and the nine-byte separator remains below 473 bytes and the 1024-byte input cap. Payment length is at most 449, QR side is checked in 1..41 before multiplication, and all successful sizes fit jsize. Independent amount decoding shifts uint64 by 0..56 bits only. |
| Use-after-free; double-free; leaks; dangling pointers | No fixture heap allocation, free, retained external reference or asynchronous work. The fixed result represents one JNI return-frame local; each invocation resets the pool. Input snapshots and callback buffers are live for the synchronous call. Returned bytes are inspected only while that fixed object is current. Production returns copied public bytes, with no pinned array or native handle. |
| NULL dereferences; uninitialized memory | NULL environment/input, preexisting exceptions, all four VM-call faults, NULL without exception and non-NULL with exception are tested. Every ordinary fake callback refuses pending exceptions. Source/result objects initialize completely, including padding. A C projection reference is inspected only after its successful status; stale reference state is never used for a rejected input. Partial reads/writes initialize one byte before raising the modeled exception. |
| Pointer arithmetic; format strings | Copies and comparisons use checked fixed extents. Source mutation indexes are 0..34; fuzz control reads require four bytes. Payment offsets end at 49+200+200 and QR comparison ends at 1+41*41. No pointer is serialized or constructed from input. Diagnostics are fixed strings with line numbers. strlen reads only fixed NUL-terminated public literals, never a fuzz span. |
| Stack usage; allocation limits; resource exhaustion | One fixed fake-array snapshot remains below the 4096-byte frame limit. Other fixture buffers/reference objects are fixed process-local storage. Every case has at most four ordinary VM calls; fault cases assert their exact stop ordinal. Fuzz input, time, per-case timeout and RSS are capped; no recursion, VLA, worker, file or network operation enters the harness. Production/test complexity caps remain 10/15, with 456/848 functions checked. |
| Malformed serialization; races; native lifetime | Invalid lengths/networks, every address-byte NUL mutation, maximum payment projection and generated malformed UTF-8/URI input preserve source bytes and refuse invalid publication. QR module bytes and parsed payment fields are compared to the existing C result: this qualifies JNI projection, not independent parser correctness. All fixture mutable state is host-only and single-threaded. Four isolated source mutants alter QR width, amount byte order, message presence or allocation exception handling; each fails its intended assertion. |
| Secret leakage and authority | All addresses are public original vectors or zero-hash fixtures; payment labels/messages/amounts are public markers. Device tests use in-memory input copies and Canvas pixels, with independent ZXing decoding, no screenshots/files/wallet/Keystore/camera/network access. They check exact maximum UTF-8 fields and successful calls after refusals. No recovery input, spending key, signature, source verification or payment approval is introduced. |

All 63 native ASan/UBSan/LSan tests pass in 44.78 seconds. Enabled production
and provider analysis, and separate Clang/GCC analysis of both fixture modes,
pass without changed thresholds. All four mutation checks fail as intended.
Bounded fuzz and Android observations are recorded in the progress log. Evidence
is under `.cache/android-wallet/jni-public-20260914`.

## Bounded public BLAKE2b-256 provider and wrapper — 2026-09-14

Scope: the official reference provider with two complete-parameter initialization
lines; one internal public-data-only hash helper; independent vector generator,
unit/fuzz fixture and separately compiled provider-failure fixture. No JNI,
secret-key input, transaction signature hash or signing entry is introduced.
The original analyzer finding and its source are preserved separately.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Wrapper rejects input above 4096, personalization lengths other than 16 and output capacity below 32 before any provider access. Parameters are exactly 64 bytes with personalization at 48, enforced at compile time. Provider init reads eight complete uint64 words; update's buffer length remains 0..128, fills/subtracts only proven extents and compresses complete128-byte blocks. Final pads the bounded remainder and writes 32 digest bytes through a private64-byte provider buffer. Fixed eight-word state, sixteen-word compression arrays and the twelve 16-entry permutations keep every round index within bounds. Output is copied only on successful finalization. Tests check all 65 capacities per vector, red zones, unchanged suffixes, SIZE_MAX and oversize claims backed by one-byte objects. |
| Integer overflow/underflow; signed/unsigned conversions | Wrapper lengths are size_t and no untrusted multiplication, narrowing or allocation arithmetic is added. A single bounded update consumes at most 4096 bytes, so byte counters do not overflow in this profile. Compression's uint64 addition intentionally wraps modulo2^64; rotations use fixed nonzero shifts 32/24/16/63. Byte load/store operations use unsigned values and explicit little-endian assembly or memcpy. Provider update subtracts only checked available bytes. Test fuzzer checks 17..4114 before subtraction and copies at most 4097; capacity is0..64. Fixed generator arithmetic fits even32-bit size_t. |
| Use-after-free; double-free; leaks; dangling pointers | Production allocates and frees nothing, retains no caller pointer and has no handle or asynchronous callback. Parameter/state/digest objects live through one cleanup sequence. Existing zcl_secure_zero owns no allocation. Fault hooks inspect/clear only live scratch and retain only counters after return, never expired pointers. Generator and fixtures have no heap/file ownership; stdout operations are checked. |
| NULL dereferences; uninitialized memory | All three caller pointers are rejected if NULL, including empty input. Complete state/parameter object representations and digest initialize before use; original generic init/init_key now also clear the whole parameter block before field assignments and byte loads. No NULL/invalid state is passed to raw provider APIs. Each provider return is checked and stops subsequent stages on failure. The final output is never inspected or published after a failed stage. Faults dirty scratch and check initial parameter bytes, state and digest, eliminating dependence on zero-filled incidental stack memory. |
| Pointer arithmetic; format strings | Wrapper uses fixed bounded memcpy spans, never alignment-sensitive casts or addresses derived from input. Provider arithmetic stays inside the validated message and its fixed arrays; no pointer moves before the object. Personalization is raw bytes, not an implicit C string. Diagnostics contain only fixed messages/line numbers. Generator formatting uses fixed formats and public bytes/version text; no input becomes a format string. |
| Stack usage; allocation limits; resource exhaustion | The wrapper owns a 64-byte parameter block, 32-byte digest and 248-byte state on the measured64-bit builds. Host optimized frames are 376 bytes for the wrapper and 24/40/104/216 for its directly used provider functions; each remains below the unchanged4096-byte warning gate. No recursion, VLA, heap, lock, network, file or worker exists. At most 32 message-block compressions occur for a 4096-byte message. Public test input snapshots are fixed4097-byte serial fixture globals, never app state. Fuzz time, input, RSS and per-case timeout are bounded. |
| Malformed serialization; races; explicit lifetimes | The helper hashes public bytes without interpreting transaction serialization or accepting a branch/height. Exact16-byte personalization is required, with no truncation/implicit padding. Callers must own stable nonoverlapping spans for the synchronous call; all mutable production state is invocation-local, with immutable provider tables. Host fixtures are single-threaded; their mutable global counters/buffers are not linked into Android. Raw keyed/tree/general provider interfaces are not exposed as wallet APIs. |
| Secret leakage, cleanup and authority | This profile excludes keys, entropy, passwords and keyed hashing. The wrapper still clears its state/parameters/digest through the existing non-elidable zeroizer on every provider exit. Provider compression temporaries remain public and carry no secret-erasure claim. The upstream keyed self-test is supporting provider evidence, not permission to add secret hashing. No APK/JNI symbol, custody policy, wallet persistence, sealed core, endpoint, signing or consensus predicate changes. Original branch-specific signature-hash and authenticated review/key ownership remain separate acceptance gates. |

Clang/GCC analysis, strict host/NDK builds, independent vectors and all 65 native
ASan/UBSan/LSan tests pass. Six isolated mutants fail their intended assertions:
each of three omitted wipes, omitted personalization, ignored init failure and
publication after failed finalization. Separate provider/wrapper differential
fuzz campaigns and actual API 30/35/36 standalone execution are recorded in
[BLAKE2_REVIEW.md](BLAKE2_REVIEW.md). Production/test complexity caps stay 10/15.
The exact debug, test and unsigned release APK bytes are unchanged because the
new internal helper is not yet reached by JNI. This is bounded public-hash
qualification; no transaction signing or hardware custody acceptance is claimed.

## Pinned original signature-hash fixture oracle — 2026-09-14

Scope: host-only libsodium generator and pinned Git-object extraction script;
144 derived public expected digests. No production C, JNI, wallet parser,
signer, consensus predicate or Android runtime dependency is introduced.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Every cursor read checks position <= length and count <= remaining before constructing a span. Wire decode is capped at 11000 bytes, text lines at 32767 bytes plus NUL, scripts at 128, inputs at 8, outputs at 16, spends/outputs at 4 and JoinSplits at 3. Single-byte counts above those caps refuse before access; extended lengths are outside this fixture shape. Fixed spend/output/JoinSplit extents are 384/948/1698 bytes; selected hash spans exclude only the specified signatures. Complete parse requires exact end-of-input. Projection copies require prefix <= 1925-11 and a script fitting its fixed array. Digest arrays are 32 bytes; byte formatting accepts at most 32. |
| Integer overflow/underflow; signed/unsigned conversions | Cursor subtraction follows the position bound. Hex length is checked even and divided before copying, so indexes stay within the validated NUL-terminated line and destination. Decimal conversion checks errno, a complete token and the exact signed/unsigned range before casting. Signed original hash types convert explicitly modulo 2^32 as in the original uint32 wrapper. Row/index/branch are bounded before conversion. Wire lengths fit the provider's unsigned long long update length. Little-endian stores accept only widths 4/8, giving shifts 0..56. Projection size addition follows a subtraction check; all loop products and bit masks have fixed small limits. |
| Use-after-free; double-free; leaks; dangling pointers | The oracle owns no C heap allocation. Views borrow the current fixed wire array only during synchronous verification; selected prefixes/scripts copy to three separately owned fixture objects before the next row replaces wire. Reconstructed projection views borrow those live objects during generation. Hash state lives through checked finalization and sodium_memzero. fopen is checked, stream errors and normal fclose are checked, and no file remains open while emitting output. Deliberate malformed-fixture assertions abort the isolated test process; OS cleanup releases its descriptor. No retained pointer is inspected after its source lifetime. |
| NULL dereferences; uninitialized memory | argc is checked before argv[1]. Decoders receive live fixed buffers, split fields only after finding separators, and refuse missing/incomplete lines. Every view and component digest array starts at zero. Each provider init/update/final result is checked before its next use. Missing optional component hashes remain zero; optional key spans are read only after a nonzero JoinSplit count. No expected output is emitted until all 130 rows match and all three projections are captured. |
| Pointer arithmetic; format strings | Offsets stay inside checked source objects. Hashing the six-component array uses an unsigned-byte pointer to the complete array representation, not pointer arithmetic beyond its first 32-byte subarray. Mutable TSV splitting occurs only inside the current terminated line. Source text never becomes a format string. Diagnostics contain fixed descriptions and public row/line numbers; numeric/header formatting uses explicit widths and PRI macros. The provider version is public text printed with a fixed format. |
| Stack usage; allocation limits; resource exhaustion | Fixed line/wire/projection storage is serial host fixture state, never app globals. Per-frame compiler limits remain 4096 bytes; no recursion/VLA/unchecked allocation exists. Input accepts exactly 130 strictly increasing rows and each row has the fixed count/byte caps. Generation has exactly three projections, two script profiles, four branches, three amounts and six total selected inputs: 144 cases. The extraction script checks the pinned original dataset hash, creates a private new directory, invokes the oracle with a 30-second CPU limit and checks its exit. Standard I/O/libsodium own their runtime internals; this does not assert that those libraries allocate nothing. |
| Malformed serialization; races; explicit lifetimes | Truncated/trailing wire, large counts, invalid hex, changed expected digest, negative index, overflowing branch, duplicate/missing rows and an incomplete line fail before any expected header bytes publish. File parsing is a deliberately bounded fixture reader, not an accepted wallet/network parser. Its globals and streams have one serial owner. The extraction script refuses existing reports, preserving their manifest and exact bytes, and reads only pinned Git objects from the original checkout. |
| Secret leakage, cryptography and authority | All input is published original randomized fixture data or public projections. Proof/signature bytes are opaque hash inputs or excluded serialization fields, with no verification/ownership claim. No real wallet, key, entropy, endpoint, node, original C++ executable or sealed core is accessed. The original amount is zero; nonzero projected expectations are derived from the reviewed serializer and require further independent comparisons before qualifying wallet code. Historical and UINT32_MAX branch cases are explicit test values, not current-chain selection. TLS and hardware custody boundaries remain unchanged. |

The host target matches all 130 untouched original v4 expected hashes, then
reproduces all 144 committed projected digests. Ten malformed-input fixtures
and eight oracle mutants refuse before writing any expected bytes. Both static
analyzers and ASan/UBSan/LSan pass. All 65 normal native tests remain green in
45.26 seconds; 884 test functions remain within the unchanged complexity cap
of 15. Reproduction and exact evidence scope are in [TRANSACTIONS.md](TRANSACTIONS.md).

## Independent nonzero-amount oracle gate — 2026-09-14

Scope: require two exact published ZIP 243 transparent-input records in addition
to the original 130 zero-amount records before fixture generation. Only host
fixture C, copied public data/license and bounded extraction glue change.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The shared field splitter accepts only seven/eight fields and writes to matching fixed caller arrays. It finds each separator before advancing and rejects trailing fields. The ZIP reader reuses the existing 11000-byte wire, 128-byte script, exact 32-byte result and bounded structural parser; no serialization limit grows. All reads occur within one checked complete NUL-terminated line. Source records are checksum-pinned before the wrapper script invokes the oracle. |
| Integer overflow/underflow; signed/unsigned conversions | Input index is checked in 0..7, type in 1..0x83, branch in 0..UINT32_MAX and amount in 1..2100000000000000 through checked strtoimax before conversion. Negative/overflow amounts and NOT_AN_INPUT are refused in this deliberately transparent fixture. Little-endian amount serialization remains eight bytes, with shifts at most 56. Both published amounts exceed uint32; reversal/truncation/zeroing mutations fail independently of the original zero-amount records. |
| Use-after-free; double-free; leaks; dangling pointers | New file-loop reuse owns one checked fopen/fclose at a time. Its row callback is a fixed local function pointer, checked non-NULL and never retained. Every ZIP view borrows the current fixed wire only for its synchronous comparison. Original projections already own their copied bytes, so reusing wire for ZIP cannot mutate them. No new heap ownership or delayed reference exists. Assertion failures terminate only the isolated fixture process, which releases its descriptors. |
| NULL dereferences; uninitialized memory | argc must be three before either path is used. Field arrays, parsed view, script/expected/digest buffers initialize; all fields are assigned by the checked splitter before conversion. The generic loop checks path/callback and accepts only expected row counts 130/2. Provider and I/O statuses remain checked. Row-order state resets before each dataset; exactly two strictly ordered ZIP rows must finish before any expected header bytes publish. |
| Pointer arithmetic; format strings | Only validated field separators and bounded slices are added; no integer-derived pointer or unaligned load. Expected ZIP bytes compare directly, without the separate original uint256 display reversal. Diagnostics retain fixed text/public row and line numbers. New fields never become printf formats. |
| Stack usage; allocation limits; resource exhaustion | ZIP verification uses the existing fixed globals, one fixed view and three small local byte buffers. No cap, recursion, VLA, thread or heap allocation is added. Per-frame warning limit remains 4096 bytes. Two additional bounded records are read under the existing 30-second extraction CPU limit; generated case count remains 144. The checksum file identifies copied data/license; it grants no source execution or wallet authority. |
| Malformed serialization; races; lifetimes | Ten original refusals still fail. Ten ZIP cases cover changed/zero/negative/overflowing amount, unsupported index, invalid hex, changed digest, duplicate/missing rows and an extra field. Every ZIP refusal occurs after all original rows match and before any expected output. Mutable fixture buffers/row state have one serial owner and are not Android runtime state. |
| Secret leakage, cryptography and authority | Only published public transaction/script/proof bytes and amounts are processed. No Python source is fetched or executed; data is selected from exact JSON bytes. The two reference hashes qualify amount serialization for this independent oracle; no Zcash-to-Zclassic chain validity, current branch, proof validity, funding, key, signing or custody claim follows. All 144 prior projected digest values remain byte-identical; only their provenance comment changes. Original wallet-constructor and authenticated authorization gates remain open. |

Both static analyzers pass the final oracle. All 130 original comparisons and
both ZIP comparisons pass under ASan/UBSan/LSan, followed by byte-identical
regeneration of the existing 144 digest values. Ten original and ten ZIP
refusals withhold expected output; all three amount mutations are caught only
by the added nonzero gate. All 65 native tests pass in 45.09 seconds and all
887 test functions remain within the unchanged cap of 15. Architecture and
existing-report preservation pass. No Android production source changes in
this slice; the previous APK and device evidence remain the applicable scope.

## Bounded transparent v4 SIGHASH_ALL constructor — 2026-09-14

Scope: internal public hash construction; fixed vector/capacity/max-size tests,
a separately compiled four-step fault provider, bounded object fuzzer, and
shared host-only mode of the already qualified original/libsodium oracle.
No JNI, key, signature, wallet storage, branch selection or node predicate changes.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Complete existing transaction validation precedes iteration: 1..8 inputs, 1..16 outputs, bounded scripts, unique/non-null outpoints, money sum and expiry. Index is checked only after a valid count. scriptCode is 0..128 and output capacity at least32. Fixed 544-byte writer checks remaining capacity before every copy/increment; its used count starts at 0 and never exceeds its extent. Final preimage is at most 397 bytes. Compile-time assertions bind writer size to all current profile limits and the BLAKE2 cap. Raw component bytes use a pointer to the complete six-element array representation. Three absent shielded hashes remain 32-byte zeros. Capacity tests cover0..64, max component/profile tests cover544/397 bytes and canaries/unchanged suffixes. |
| Integer overflow/underflow; signed/unsigned conversions | Input money is checked against MAX_MONEY and output sums use the unchanged complete validator. All counts/scripts are bounded before additions or conversions. Integer writer accepts only widths1/4/8 and shifts uint64 by0..56; branch uint32 shifts are0..24. Displayed outpoint bytes reverse using the fixed31-i index with i<32. SIGHASH_ALL is exactly uint32 value1; no arbitrary mode or signed hash-type conversion is exposed. Fuzz integer reads require32..4096 input bytes, offsets<=4096 and width<=8 before bounded cyclic reads. Remaining-money generation only subtracts values<=remaining and adds1 to a value<=MAX_MONEY. Malformed SIZE_MAX counts/lengths never govern a generation loop or unchecked constructor copy. |
| Use-after-free; double-free; leaks; dangling pointers | Production allocates/frees nothing and retains no caller pointer. Its work object lives through all provider calls, publication and exactly one cleanup. Helper byte arrays are synchronous public temporaries, never returned. Fault fixtures record only borrowed work spans; the zero callback inspects and retires them while the object is live, and post-return assertions inspect cleared flags/NULL markers only. Missing cleanup fails before any stale-span read. Shared oracle views borrow one validated wire for one call; CLI projections remain copied before the next dataset replaces wire. No owned handle or asynchronous callback is introduced. |
| NULL dereferences; uninitialized memory | Transaction, scriptCode and output pointers are checked even for empty scriptCode. Full work representation is memset to zero before use, including writer flags, all six hashes, final domain and digest. Public helper arrays initialize. Each hash status is checked before the next phase or publication; a fault may dirty private output but never caller output. Final domain receives all 16 bytes explicitly. New unit/fuzz fixture storage initializes before mutation and snapshots; fake provider outputs are checked zero before dirty writes. The oracle library uses no CLI buffers or mutable row state, and validates caller spans before parsing. |
| Pointer arithmetic; format strings | Only checked writer/source offsets, fixed outpoint reversal and bounded byte spans are used. No alignment-sensitive integer load, serialized pointer, implicit C-string length or unchecked script traversal exists. Domains are exact 16-byte arrays, deliberately without a terminator. Diagnostic formats are fixed strings/line numbers and contain no transaction, address or secret bytes. No new production formatting or logging is added. |
| Stack usage; allocation limits; resource exhaustion | Work is 800 bytes on the measured64-bit builds, including a 544-byte writer, six 32-byte hashes,16-byte domain and32-byte digest. Host optimized compiler frame is 904 bytes; the unchanged 4096-byte per-frame gate remains. Small public endian/outpoint scratch and provider frames are bounded, with no recursion/VLA. Exactly four bounded BLAKE2 calls occur on success, with fewer after a fault. There is no production heap, I/O, timer, lock, worker or global mutable state. Test/fuzz objects use fixed serial public storage; fuzz length/time/RSS/per-case runtime are capped. Maximum profiles are deterministic unit cases and explicit fuzz seeds. |
| Malformed input; races; explicit lifetimes | Caller-owned transaction/script/output objects must remain stable and nonoverlapping for the call. Shape/index/amount/capacity failures preserve output; fake provider and fuzz snapshots preserve transaction/script input. Fuzzer invalidations cover oversized counts/lengths, output overflow, duplicate/null outpoints and expiry. Its status expectation reuses the existing validator/serializer; its successful digest oracle uses an independent reader/libsodium, so independent transaction-validity is not claimed. Input scriptSig bytes are intentionally excluded by v4; both active scriptSig changes and inactive tail bytes preserve the digest in max-profile tests. Future review must bind exact unsigned bytes separately. |
| Secret leakage, cryptography and authority | The operation accepts public transaction/script metadata and explicit branch data only; it neither selects current branch/height nor obtains a key, signature, authorization or broadcast capability. Owned work and the existing BLAKE2 wrapper's scratch clear on provider exits. Small public endian/outpoint temporaries and provider compression temporaries carry no secret-erasure promise; keys/entropy/passwords are outside this interface. Amount/scriptCode are explicit, without funding or redeem-script inference. Arbitrary historical/test branch values in vectors do not establish current-chain validity. The exact original domains, wire byte order and zero empty-component rules are checked against 132 published reference comparisons and144 derived expected digests. No sealed core or custody boundary changes. |

All 67 native ASan/UBSan/LSan groups pass in 45.23 seconds. Separate Clang/GCC
analysis passes production, unit/fuzz plain and oracle modes, generator/library
modes and the fault provider. All 467 production and912 test functions remain
within unchanged 10/15 caps. Eleven isolated mutants fail their intended
assertions, including both omitted and incomplete work cleanup. Differential
fuzzing completes 1,116,954 executions in 121 seconds without a finding. The
original/ZIP generator still reproduces the exact committed expected header.

Android/JVM/lint/artifact and architecture gates pass. Actual release-archive
standalone tests pass on x86-64 API 30/35/36, including every independent vector,
max profile, capacity and refusal. ARM64 is compiled only. APK bytes remain
unchanged because JNI does not reference this operation. Hash evidence does not
qualify signing, authenticated review/key ownership, current branch selection,
proof validation or hardware custody. Evidence and reproduction are in
[TRANSACTIONS.md](TRANSACTIONS.md) and `.cache/android-wallet/sighash-core-20260914/`.

## Live-review P2PKH digest binding — 2026-09-14

Scope: internal review-bound public hash operation, shared existing lifetime
transition, unit/fault fixtures and extension of the review state-machine fuzzer.
No secret, JNI method, signing capability or chain predicate is added.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The entry checks non-NULL owner/output, live ID, capacity>=32 and index below both assessed count and8 before reading a row. The owner representation is private, created only by validated review opening under the adapter lock. Its wire is bounded1925 and parsed again before hashing. Only exact P2PKH templates pass; reconstruction fits25 bytes. The hash constructor rechecks the complete parsed object and all bounded spans. Only32 output bytes copy after success. Tests cover0..64 capacities, canaries/suffixes, SIZE_MAX index and maximum8/16 profiles. The fault observer checks its captured count<=4 immediately before iteration, as well as at insertion; no analyzer suppression replaces this bound. |
| Integer overflow/underflow; signed/unsigned conversions | No new untrusted arithmetic or allocation size exists. Input amounts come from the assessed previous output and remain<=MAX_MONEY;0/1/MAX and large values pass independent comparisons. Index remains size_t until bounded, branch is uint32 data without conversion, script length comes from checked reconstruction. Existing opening overflow checks, fixed deadline and monotonic-time comparisons remain unchanged. Fuzzer IDs issue at most65 times, step spans require10 bytes and read_time consumes exactly8; all profile choices use bounded modulo/masks. |
| Use-after-free; double-free; leaks; dangling pointers | No production allocation/free/handle exists. One invocation-local work object owns parsed transaction, script, script length and digest through publication and cleanup. The assessment row is borrowed only under the same serialized review lock. Review opening has already copied all previous-source-derived fields; later source destruction or mutation of returned snapshots/wire cannot substitute them. Fault-observed spans are inspected and retired only inside the live zero callback; later assertions inspect flags/NULL markers, never expired pointers. |
| NULL dereferences; uninitialized memory | NULL owner/output refuses before liveness or clock mutation. Whole work representation is memset to zero, including padding, parsed object, script length and digest. Parse, script and hash statuses gate the next phase. Faults deliberately dirty their output, including SIZE_MAX script length, yet no later phase or caller publication follows. Early capacity/index/P2SH/lifetime refusals never enter private work. Shared internal liveness explicitly requires a checked non-NULL owner. |
| Pointer arithmetic; format strings | Production uses bounded fixed arrays and the already qualified parser/script/hash helpers; it introduces no alignment cast, serialized pointer or C-string parsing. The only new row subscript follows the two index bounds. Output/owner must not overlap and remain stable for the synchronous call. Fixture byte traversal follows explicit lengths and fixed guards. Diagnostics use fixed text and line numbers with public data only; no user bytes become a format string. |
| Stack usage; allocation limits; resource exhaustion | Fixed public work is2272 bytes on measured64-bit builds, with an optimized host2328-byte frame. This is per-frame evidence, not whole nested-call usage. Parser/hash/provider work stays within existing fixed caps and the4096-byte frame gate. There is no recursion/VLA/heap, I/O, timer, thread or production mutable global. At most one parse, one script reconstruction and one bounded four-stage hash occur per successful call; failures stop earlier. Large unit/fuzzer fixtures use serial test storage outside production stacks. Fuzzer input/time/RSS and per-case runtime remain bounded. |
| Malformed serialization; races; explicit lifetimes | Only a live unsigned review produced by the existing complete preparation/assessment path is accepted. Canonical P2SH remains reviewable but cannot obtain this P2PKH digest. No caller-supplied script/amount or reopened previous source exists. Existing stale-ID, inclusive expiry, rollback and clear rules are shared without weakening; a live refused read advances last_ms but cannot extend its deadline. Every call requires the same enclosing lock as other review access. State-machine fuzzing models serialized interleavings and makes no concurrency-lock proof. |
| Secret leakage, cryptography and authority | The operation processes only public reviewed bytes and clears its entire private work after every entered work path. It neither obtains entropy/key material nor signs, selects current branch/height, authenticates ownership, proves funding/unspentness or grants consent. Raw digest bytes do not bind network or maximum-fee policy themselves; those remain bound to the exact review and future authenticated authorization. P2SH/redeem-script signing is refused. An externally cached digest cannot become an authorization handle. No custody, persistence, TLS or sealed-core boundary changes. |

Final Clang/GCC analysis and all 69 native ASan/UBSan/LSan groups pass in 45.92
seconds. Production/test complexity caps remain 10/15 (469/932 functions).
The initial GCC observer-count finding is preserved and repaired with an
explicit bound before dereferencing the captured-span array. All thirteen
mutants fail their intended assertions; the final bounded review-lifetime
fuzzer completes 296,803 executions in 121 seconds without a finding. Its oracle
mode uses the independently qualified reader/libsodium. Exact tests, scope and
device evidence are recorded in [TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/review-sighash-20260914/`.

## Consumed change-address reconstruction — 2026-09-14

Scope: a new C wrapper checks an already consumed index against authenticated
observed state; reservation shares its existing derivation helper. Unit/fault
tests, one conditional mode of the existing state-file fuzzer, and a bounded
Android-specific test-directory template provide isolated qualification.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Output must be non-NULL with capacity>=35; requested index is below65535 before further work. Existing custody preparation checks record124..140 bytes before copying and checks entropy width against the parsed header. State must be a complete aligned head with tail80 and total size80..5242880 before decode; the decoder checks its own exact record and secret spans. Exactly35 candidate bytes copy only on complete success. Capacity0..64 guards/suffixes, all partial lengths0..159 and each head-byte corruption are tested. The shared fuzzer requires6..246 bytes before reading its fixed six-byte controls or copying payload<=240. Its successful-output checker bounds file size before tail subtraction. Both fixed fixture templates fit the existing29-byte path buffer by static assertion. |
| Integer overflow/underflow; signed/unsigned conversions | MAX_RECORDS-1 is the fixed uint32 limit65535. The file-size lower bound precedes division and subtraction; the decoded counter must equal file_bytes/80-1, then requested<next. Full capacity remains readable while growth still refuses. No requested-index increment occurs. New fuzzer indexes combine two unsigned bytes with a checked-width8-bit shift; capacity uses bounded modulo65. Fixture maximum offsets cast fixed5MiB bounds to off_t, below signed32 limits. Test counters have explicit small bounds before loops; no signed-negative value becomes a length or index. |
| Use-after-free; double-free; leaks; dangling pointers | The wrapper owns one copied public record, one snapshot and candidate address; entropy stays caller-owned and stable. No pointer/secret escapes. The shared derivation helper owns and clears its64-byte blinding until return; decode independently owns/clears32 bytes. Existing key routines own checked bounded EC allocations and release them through their established cleanup paths. Storage publishes only after closing all descriptors and never retries consumed close descriptors. Fault tests compare descriptor counts. Borrowed blinding pointers are inspected/retired within the live zero callback; post-return assertions use flags/NULL only. Fixture failures preserve evidence until process cleanup; successful cleanup touches only fixed names in its owned mkdtemp directory. |
| NULL dereferences; uninitialized memory | Caller output is checked before access; existing preparation/path/codec checks reject NULL source spans. All local wallet/snapshot/candidate/counter/blinding objects initialize before use. Every preparation, observation, MAC and derivation status gates its successor and publication. Fault providers return plausible dirty head/counter data or partial blinding/address bytes with failure; no later authority step follows. Private copied header/ciphertext remains authoritative after adversarial caller-byte mutation. Internal shared custody helpers retain their successfully prepared-wallet precondition. |
| Pointer arithmetic; format strings | Only bounded fixed arrays and existing validated storage/crypto spans are used. Tail offsets follow complete size checks, and source copies are at most140/240 bytes. No alignment cast, C-string secret, unchecked path concatenation or serialized pointer is introduced. The Android fixture merely selects a shorter compile-time template. Diagnostic text is fixed and contains only public fixture locations/line numbers; no source bytes become a format string. |
| Stack usage; allocation limits; resource exhaustion | New code adds no heap, recursion, VLA, worker or retained handle. Measured optimized host frames are376 bytes for reconstruction and104 for shared derivation; nested crypto/storage costs remain separately bounded by existing gates. Successful reconstruction performs one bounded head observation, one authenticated decode and one recovered derivation, using sequential checked EC contexts rather than accumulating them. Storage reads only the tail, including at5MiB capacity; no history scan occurs. Parent fsync remains one normal call or at most16 retries; read/pread attempts remain<=256 and all handles close. Fuzzer inputs/time/RSS/per-case runtime are bounded; test descriptor enumeration caps at256. |
| Malformed serialization; races; lifetimes | Missing/empty/partial/bad-MAC/misplaced state refuses without journal initialization or repair. Unused indexes cannot publish an address; consumed indexes remain reconstructible when exhausted. Existing store opening can establish its private directory/lock and syncs the parent, but this operation never writes wallet/journal bytes or reserves again. Observation matches the exact copied committed wallet under the same nonblocking lock and authenticates that owned snapshot after release. Later appends cannot make an already consumed index unused; concurrent fresh-wallet selection and platform authorization still require their existing enclosing lifetime rules. Historical-head checking supplies no malicious-rollback or complete-history proof. Callers serialize their own mutable spans; fuzz fixtures are serial and claim no concurrent platform-lock proof. |
| Secret leakage, custody and authority | The platform must first authenticate the exact record/header/entropy with GCM and per-use hardware policy; C cannot prove that prerequisite. C checks recovered identity, head MAC/position and key derivation, returning only35 public address bytes. Existing seed/private-key/MAC-key cleanup remains unchanged; new shared OS blinding clears on every exit. Caller entropy must clear after use. No key export, signing, funding/unspentness claim, current branch/height, fresh reservation, index reuse, acceptance receipt or review approval is added. Burned/cancelled gaps remain consumed. All fixtures use published entropy and inert ciphertext; separate actual-provider GCM tests do not qualify hardware custody. |

Final Clang/GCC analysis and all 71 native ASan/UBSan/LSan groups pass in 49.93
seconds, with unchanged 10/15 complexity caps (471/958 functions). All twelve
mutants fail their intended assertions. The bounded reconstruction fuzzer
completes 28,364 executions in 121 seconds without a finding; final reconstruction
and original reservation modes replay the 57-file corpus after the Android
fixture-path addition. Original campaign binary/source and rebuilt replay
binary are retained separately. The initial fault expectation that reads do
not fsync was corrected to explicitly verify the existing parent-durability
contract and its failures; no storage gate or assertion policy was weakened.

Android/JVM/lint/artifact/architecture checks pass. Actual release-archive
standalone tests pass on x86-64 API 30/35/36; ARM64 is compiled only. All three
real-JNI record/storage/GCM tests pass on each API level after fresh debug
installation. The new consumed-address operation has no JNI entry. Exact
artifacts, evidence boundaries and reproduction are in
[CHANGE_STORAGE.md](CHANGE_STORAGE.md) and
`.cache/android-wallet/change-ownership-20260914/`.

## Live review input wallet comparison — 2026-09-14

Scope: one internal C wrapper, a bounded invocation-only claim, a reusable public
fixture, real-provider/fault tests and a state/claim fuzzer with regression mode.
No JNI, signing, platform authentication or chain predicate is added.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | NULL owner/claim refuse first; the shared live transition precedes a row bound against both assessed count and8. Only P2PKH rows enter work. Source pointers and path1..1024, record124..140 and entropy16..32 in steps4 are checked before copying. Custody parsing rechecks exact record/profile widths. Receive0 alone is supported; change indexes stay below65535. Encoder length must be exactly35, and the private derived buffer is35. Storage length equality short-circuits memcmp, including the fault's SIZE_MAX length. Unit tests cover all widths, maximum rows, NULL, SIZE_MAX and short spans; fuzzer input12..252 bounds every fixed control and payload<=240. |
| Integer overflow/underflow; signed/unsigned conversions | No untrusted allocation size, index increment or pointer offset arithmetic is introduced. Input index remains size_t and the key index is uint32. Existing fixed review deadline and monotonic checks remain unchanged. Entropy modulo/length checks precede copying. Tests bound every mutation offset; fuzzer subtracts12 only after the size floor and journal comparisons use known fixed spans. stat size must be nonnegative before uint64 conversion. Fuzzer IDs are reset per isolated case and cannot overflow their small issuance count. |
| Use-after-free; double-free; leaks; dangling pointers | The wrapper owns all copied metadata, path, record, entropy and comparison buffers for one synchronous invocation. It adds no heap/free/handle. Existing storage and crypto helpers retain their checked descriptor/context lifecycles. The live owner row is borrowed under the required exclusive adapter lock. Mutating every original caller span and the claim after copying cannot replace owned inputs. Fault observers inspect and retire captures inside their live zero callbacks; post-return checks use NULL markers and counters. Each fuzz case closes/removes only its own public mkdtemp fixture, and no pointer survives it. |
| NULL dereferences; uninitialized memory | The entire work representation initializes with memset, including padding and unused entropy/path tails; all spans are checked before access. Custody preparation, encoding, exact encoding length, storage, randomness and derivation statuses gate later phases. Tests deliberately return valid-looking dirty data with failure and verify no later step follows. Receive blinding, observed record/length/pending state and candidate buffers initialize before provider use. NULL owner/claim cannot advance the clock; invalid non-NULL claims still clear their entered work. |
| Pointer arithmetic; format strings | Production uses fixed arrays and already bounded record/path/key helpers, with no alignment cast or serialized pointer. Borrowed spans must remain stable and nonoverlapping through the synchronous call. Test observers capture bounded live objects rather than reconstructing stack addresses. Fuzzer bytes never select a pathname; only the trusted owned fixture path or a rejected NULL/zero/SIZE_MAX path claim is used. Diagnostics contain fixed text and public line/file identifiers, never secret bytes or user format strings. |
| Stack usage; allocation limits; resource exhaustion | The measured optimized host entry frame is1608 bytes, below the unchanged4096-byte per-frame gate; nested helpers retain their separately bounded costs. No recursion/VLA/production mutable global, new allocation, worker or timer is introduced. One input check performs bounded record observation and recovered derivation; change additionally checks its bounded head and MAC. Existing storage parent-sync and read retry limits remain unchanged. Filesystem completion is not a wall-clock deadline: run under the existing bounded worker/lifetime policy. Large test fixtures are serial globals, with input/time/RSS/per-case limits on fuzzing. |
| Malformed serialization; races; explicit lifetimes | The destination comes exclusively from the owned assessment of a live unsigned review. Destroyed original funding sources cannot replace it. Wallet profile/network, exact committed record and recovered key location must match. Pending wallets, missing/partial/unauthenticated change state, unused indexes and unsupported P2SH refuse. Store metadata opening can create its private directory/lock and sync the parent, while wallet/journal bytes remain unchanged. The existing nonblocking storage lock protects each observation; the adapter must separately serialize wallet selection, review and platform authorization. No concurrent-lock, malicious-rollback or complete journal-history proof is claimed. The supplied-time lifetime never extends; future signing must recheck completion-time context in its own operation. |
| Secret leakage, custody and authority | Platform GCM authentication and per-use hardware policy remain explicit prerequisites; C cannot prove them. The whole owned work, including32-byte entropy storage and copied pointers, clears on every entered path; receive OS blinding clears after success or failure. Caller entropy remains caller-owned and must also clear. Existing nested seed/private/MAC-key zeroizers remain unchanged. The result is an invocation-only comparison for one key location, with no key export, signature, digest, approval token, current-chain or unspentness assertion. All fixtures use published entropy and inert ciphertext; standalone C device tests are not platform custody evidence. |

All74 native ASan/UBSan/LSan groups pass in55.23 seconds; Clang/GCC production
and new fixture/regression/fault analysis pass. Complexity caps stay10/15
(479 production/1001 test functions). All15 deliberate mutants fail intended
assertions. The bounded fuzzer completes21,928 executions in121 seconds without
a finding, checking independent lifetime transitions and exact file preservation.
The initial test tried to recreate an existing fixture using its exclusive
writer; it was corrected to unlink only its owned synthetic journal before
recreating it. The exclusive-write guard remains unchanged. Initial and final
logs are preserved. APK bytes remain identical because the new operation has
no JNI caller. See [TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/review-ownership-20260914/` for exact evidence scope.

## Candidate context bound to live-review hashes — 2026-09-14

Scope: a bounded v4 branch lookup and internal review wrapper, source-data
projection/reference fixtures, unit/fault tests and an additional operation in
the existing review fuzzer. No sealed core, JNI, signature, chain connection,
platform custody policy or wallet persistence changes.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | NULL owner/block/digest refuses before liveness. Capacity>=32 and selected index below the private assessed count are checked before work; the count itself must be1..8 before finality scans it. Branch lookup validates network exactly before indexing its two immutable rows. Whole candidate storage initializes, then copies one bounded block value; only32 private digest bytes publish on success. Tests cover65 capacities with guards/suffixes, SIZE_MAX index, all eight finality rows and unsupported P2SH. The fuzzer still requires complete10-byte steps and caps total input640; all read_time calls consume exactly8 available bytes and boundary-table indexes are checked before use. |
| Integer overflow/underflow; signed/unsigned conversions | Candidate height is uint32 capped at INT32_MAX; cutoff is uint64 capped at INT64_MAX. Original nonnegative comparison semantics require no narrowing casts, addition or hidden tip increment. Expiry uses strict height>expiry with the zero exception; lock comparison selects height/time at the exact500000000 threshold. Sequence comparison uses exact UINT32_MAX. The independent epoch traversal checks signed activation>=0 before comparing through int64, preserving disabled -1 entries. The exhaustive sweep ends at800000, leaving room for its increment. Fuzzer raw values mask before narrowing, existing time arithmetic remains bounded, and ID issuance stays<=65. |
| Use-after-free; double-free; leaks; dangling pointers | No production allocation/free, I/O, thread, handle or retained pointer is added. The candidate block/branch/digest belongs to one synchronous stack work object. The owned review is borrowed only under the existing exclusive adapter lock. Original funding/wire destruction and caller-candidate mutation cannot replace those owned values. Fault spans are inspected and retired while their subobjects remain live inside the zero callback; post-return checks inspect NULL markers/counters only. New scalar reference tables are immutable; large fixtures remain outside production/thread stacks. |
| NULL dereferences; uninitialized memory | Whole work including padding/branch/digest initializes before use; every branch and hash return gates later phases and publication. The scalar helper publishes only after complete network/height validation. Dirty-provider tests write plausible branch/digest values before failing, yet callers retain untouched output. NULL arguments cannot advance lifetime state. Unsupported network and invalid time paths still clear entered work; capacity/index/liveness errors stop before work. Projected tables have exact row counts and checked network access. |
| Pointer arithmetic; format strings | Production adds only fixed typed objects and bounded row scans, with no alignment cast, serialized pointer, raw pointer arithmetic or variable path. Caller spans must remain stable and nonoverlapping through the call. Provider fault observations use captured live addresses, not reconstructed stack offsets. Diagnostic formats are fixed and contain public line identifiers only. The projection script quotes all paths, never evaluates extracted source as shell code, checks exact source hashes before narrow awk extraction, and refuses an existing output directory. |
| Stack usage; allocation limits; resource exhaustion | Optimized host frames measure0 bytes for scalar lookup and120 for the wrapper; the existing nested hash/parser/provider bounds remain separate. All stay within the4096-byte per-frame gate. No recursion/VLA/heap or new worker exists. Work is at most an8-input scan plus the existing four-stage bounded digest. Projection has a10-second CPU limit, fixed immutable source objects and exact7-epoch/14-activation cardinality checks. Fuzzer input/time/RSS/per-case limits remain640/121seconds/512MiB/5seconds. No original node or external peer is executed. |
| Malformed serialization; races; explicit lifetimes | Exact transaction fields come exclusively from the validated unsigned owner. The wrapper checks the original equality/zero/finality rules within the supported v4 profile and supplied candidate domain, without editing wire or silently choosing policy defaults. Both networks' complete original schedules are projected independently, including disabled/duplicate-activation epochs and repeated branch IDs. This is not full contextual consensus, relay policy or current-chain authentication. All accesses require the same adapter lock; success only describes the supplied-time operation and cannot prove completion-time freshness. Shared stale-ID/expiry/rollback clearing remains unchanged; failed live calls advance the clock without extending the deadline. The fuzzer models serialized interleavings, not concurrent lock correctness. |
| Secret leakage, cryptography and authority | Only public transaction/candidate/hash data enters this wrapper, and the complete private work clears on every entered exit. No entropy, key, signature or approval token is obtained. The candidate's network/height/time is explicit input and supplies no source authentication, consent, funding/unspentness/maturity or broadcast authority. A future signer must bind independently authenticated current context and consent to the exact live review, and recheck its own operation lifetime. Raw digests cannot become cached authorization. Existing input-wallet comparisons and platform GCM/hardware prerequisites remain separate. TLS stays owner-parked and excluded. |

Clang/GCC production and new fixture/fault/oracle analysis pass; all77 native
ASan/UBSan/LSan groups pass in54.98 seconds, with unchanged10/15 complexity caps
(484/1027 functions). All19 mutants fail intended assertions. The extended
independently modeled review fuzzer completes327,566 executions in121 seconds
without a finding; independent reader/libsodium comparisons also pass. Both
release-archive standalone tests pass on x86-64 API30/35/36; ARM64 is compiled
only. APK bytes remain unchanged because these operations are internal and
unused by JNI. Exact provenance, device limits and reproducible checks are in
[TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/review-context-20260914/`.

## Bounded raw-digest signatures — 2026-09-14

Scope: one internal signing primitive, real-provider and Linux fault tests,
an optional host OpenSSL oracle and a bounded malformed-input fuzzer. No JNI,
wallet persistence, platform authorization, consensus predicate or vendor
source is changed.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | All three caller pointers are checked before work; both input lengths must be exactly32 before copies. Output is one fixed typed object. Public key capacity33 must return33; DER capacity72 must return8..72 before tail subtraction, clearing or parsing. The pinned provider validates the scalar; both native and parsed signatures must be low-S before publication. Unit guards, lengths0..64/SIZE_MAX and dirty-provider oversized lengths exercise the bounds. The fuzzer requires exactly66 bytes before reading two controls and two32-byte spans. Oracle entry checks DER8..72 before long conversion, parsing or pointer-end checks; re-encoding length equals that checked bound before writing its72-byte buffer. |
| Integer overflow/underflow; signed/unsigned conversions | No untrusted multiplication, allocation arithmetic or input-derived loop count is introduced. Nonce attempt is unsigned and compared directly with8 before delegating to the pinned RFC6979 counter loop. UINT_MAX refuses without incrementing. DER subtraction follows both length bounds. All encoding lengths use size_t; oracle casts only positive checked lengths<=72. Low-S uses bounded OpenSSL BIGNUM arithmetic with checked results. Fixture counters have fixed stage/mode/attempt bounds and expected arrays are indexed only after a cardinality check. |
| Use-after-free; double-free; leaks; dangling pointers | Private work owns the copied secret/digest/blinding, opaque provider outputs and public result for one invocation. The existing EC helper owns its one checked allocation; a single unconditional end call clears/releases it after every entered result, including failed creation/randomization. No borrowed pointer is retained. Fault allocation wrappers verify storage is zero before free and exactly one owner/release; secret/nonce captures are inspected and retired inside live zero callbacks. OpenSSL objects have checked allocations and one cleanup path; get0 BIGNUM values remain borrowed until parsed-signature cleanup. |
| NULL dereferences; uninitialized memory | Whole work initializes with memset, including public result padding/tails. OS randomness, context setup, key creation, signing, encoding and every parse/comparison/verification result gate successors. Dirty valid-looking error output cannot publish. Provider callbacks receive valid fixed buffers by the pinned API contract; unexpected algorithm/data and disallowed attempts refuse. Fixture provider definitions use upstream implementation mode so runtime NULL assertions are not optimized away by caller-only nonnull attributes. Production declarations retain those annotations. Allocation failures and NULL span/output inputs are tested. |
| Pointer arithmetic; format strings | Production offsets are limited to the checked DER tail and fixed32-byte copies. There are no alignment casts, serialized pointers, variable paths or secret strings. Inputs must be stable and nonoverlapping with output. Fault observers capture actual live pointers and retire them before return. Oracle cursor arithmetic stays within the validated DER span; its bounded OpenSSL serializer receives a fixed72-byte array. All test diagnostics use fixed public text/line numbers; no key/digest/input becomes a format string. |
| Stack usage; allocation limits; resource exhaustion | Optimized host frames measure552 bytes for signing and8 for the nonce callback; nested provider/context stack costs remain separate and the unchanged4096-byte authored per-frame gate passes. Work is496 bytes on the measured64-bit host. No recursion/VLA, additional heap, new thread or retained context exists. Context size must be1..1024 before allocation. At most8 RFC6979 candidates are permitted; no fallback/retry key exists. OS randomness uses its existing bounded implementation. Fuzzer max_len66, per-case5 seconds,121-second campaign and512MiB RSS cap bound synthetic work. OpenSSL allocations exist only in host fixtures with bounded32/33/72-byte inputs. |
| Malformed serialization; races; explicit lifetimes | Scalar zero/order/out-of-range, malformed provider encodings, high-S, changed public key or digest, noncanonical lengths and failed verification refuse. DER/key self-consistency is checked with the same pinned provider; independent OpenSSL verification is a distinct host test claim. Every successful call publishes one whole public representation and every failure preserves all caller output bytes. Both input copies precede the first OS/provider call; source mutation fault tests exercise this ordering. The API is synchronous with no global mutable production state; callers serialize mutable inputs and retain ownership. These tests do not establish concurrent platform-lock correctness or completion-time review freshness. |
| Secret leakage, custody and authority | The whole entered work, copied secret, digest, OS blinding and transient context clear on success/failure. Refused nonce callbacks clear the provider-owned32-byte candidate; the pinned signer's internal cleanup of nonce/message/scalars and RFC6979 key/RNG state was inspected without modifying vendor bytes. Caller secret storage must also clear. No private key enters logs, Kotlin/JNI or the public output; the oracle handles synthetic public fixture scalars only and clears its owned scalar/point. Raw signatures establish no wallet/key ownership, current chain, consent, unspentness, signed transaction or broadcast authority. Future wallet callers must bind those prerequisites within the exact live operation and recheck cancellation/completion before publishing. |

All79 native ASan/UBSan/LSan groups pass in55.70 seconds. Production and new
fixture/fault/oracle Clang/GCC analysis pass with unchanged10/15 complexity caps
(490/1066 functions). All24 mutants are detected:23 by intended assertions and
one by UBSan at the invalid provider call after bypassed context failure. The
mutation runner records that precise sanitizer interception instead of requiring
it to reach a later assertion. The OpenSSL-enabled fuzzer completes36,242 runs
in121 seconds without a finding. Initial/final GCC fixture logs retain the
nonnull-annotation conflict and its provider-implementation-mode fix; no warning
or assertion is disabled. Exact Android artifact/device acceptance and boundaries
are in [TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/signature-core-20260914/`.

## Public signature verification and canonical input scripts — 2026-09-14

Scope: an internal public-data verifier/encoder, real-provider and substituted
fault tests, an extended host OpenSSL oracle and a new differential fuzzer.
No private keys, JNI, custody policy, wallet data, sealed core or vendor bytes
are changed.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Five pointers and exact digest32/hash20 lengths are checked before work. Copied DER length must be8..71 before source access or capacity arithmetic. Only that bounded DER and33 key bytes copy. The static provider consumes these exact spans. Its canonical output has72-byte capacity, and its returned length must equal the already bounded input before memcmp. Script offsets derive only from8..71, so two pushes fit107 bytes. Tests cover129 capacities, SIZE_MAX, guards, all source-byte corruptions and dirty SIZE_MAX provider lengths. Fuzzer requires exactly160 bytes before reading fixed3/72/33/32/20 spans. |
| Integer overflow/underflow; signed/unsigned conversions | DER+36 is computed only after8..71 validation; direct push length<=72 converts safely to uint8. No untrusted allocation arithmetic, signed length, offset multiplication or counter increment is used. Provider canonical length equality precedes byte comparisons. Fuzzer acceptance independently checks NULL/DER bounds before arithmetic, even though the external oracle also refuses them. High-S fixtures subtract fixed32-byte unsigned scalars with explicit byte masking and bounded borrow; no signed underflow is used. |
| Use-after-free; double-free; leaks; dangling pointers | Production owns one fixed stack work object, borrows immutable secp256k1_context_static, and allocates/frees no heap or handle. All public source metadata/bytes copy before the first provider; no source pointer survives. Fault providers mutate originals after copying and inspect/retire captures while work is live. OpenSSL object lifecycles remain in their existing one-cleanup helpers; the new hash comparison owns only bounded local arrays. Seeds use exclusive creation inside an owned private directory and check writes/close once. |
| NULL dereferences; uninitialized memory | NULL signature/digest/hash/output/length refuse before access. Whole work initializes, including unused arrays and provider outputs. Every parse/hash/normalize/serialize/verify result gates its successor. Dirty output on failure cannot publish. Invalid lengths/capacities still clear entered work. The fuzzer explicitly checks signature NULL/length locally instead of assuming an external oracle's postcondition. Fixture provider definitions preserve upstream runtime NULL assertions and match the exact RIPEMD160 array declaration. |
| Pointer arithmetic; format strings | Fixed offsets and lengths are checked before all source copies, tail access, canonical comparison and script pushes. No alignment cast, serialized pointer, variable production path or source string is introduced. Inputs and outputs must not overlap. Fault callbacks use captured live addresses, never reconstructed stack offsets. Diagnostics use fixed public text and line numbers. Public seed names use checked snprintf into32 bytes with a fixed16-file limit; no secret or user value becomes a format string. |
| Stack usage; allocation limits; resource exhaustion | Optimized host entry frame is600 bytes; nested provider costs remain separate under existing authored4096-byte frame checks. No recursion/VLA, production heap, RNG, worker, I/O or mutable global is added. One invocation performs fixed bounded key parsing, two public hashes, DER parse/re-encode and one verification. Inputs remain8..71/33/32/20; no externally selected hash algorithm or retry exists. Fuzzer input160, per-case5 seconds,121-second campaigns and512MiB RSS cap bound fixture work. Test globals are isolated and serial. |
| Malformed serialization; races; explicit lifetimes | Only compressed keys, exact HASH160 matches, strict canonical DER/low-S and the supplied verified digest can produce the two minimal direct pushes with fixed SIGHASH_ALL. High-S, padding/long-form/trailing/zero/changed data refuse. The helper checks public signature consistency, not arbitrary scripts. The original source ordering/direct-push rules were inspected without executing its interpreter. Caller spans remain stable through the synchronous invocation; no authorization or completion-time freshness is retained. Future wallet composition must use the same owned review's digest and P2PKH destination under its existing exclusive lock. |
| Secret leakage, custody and authority | Only public signatures, digest, key and hash enter or leave. Complete work clears after every entered exit and unused source DER bytes/padding cannot leak into output. No secret, entropy, wallet path, authorization token or JNI capability is added. The tests sign only public synthetic scalars through the previously qualified primitive, then clear their owned scalar storage. Matching these supplied public objects proves no wallet custody, consent, network/current chain, unspentness, signed whole transaction or broadcast authority. |

All81 native ASan/UBSan/LSan groups pass in58.04 seconds; Clang/GCC production
and all new fixtures/oracle/fuzzer analysis pass. Complexity caps stay10/15
(495/1095 functions). All24 mutants are caught, including ASan's SIZE_MAX
comparison interception and23 intended fixture assertions. Test-only initial
declaration, complexity and local-bound findings were fixed without disabling
checks. Final release-archive tests pass on x86-64 API30/35/36; ARM64 is compiled
only. Exact evidence and limits are in [TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/signature-script-20260914/`.

The final independently checked script fuzzer completes1,792,794 executions
in121 seconds without a finding, with max_len160, timeout5 and RSS512MiB
(observed274MiB). The initial698,807-run campaign's source/binary/log remain
separate from the final harness after complexity and local-bound fixes.

## Complete public signed wire from an owned review — 2026-09-14

Scope: two internal C assembly/publication units, a reusable synthetic fixture,
real-provider and failure tests, an independent reference and a bounded fuzzer.
No JNI, private key, wallet persistence, consent or consensus predicate changes.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | NULL spans refuse first. Signature count must be 1..8 and match the assessed count before multiplication/copy/indexing. Parsed input count must match that checked count. Original scripts must be empty. Each completed script length is checked against 44..107 even after helper success. The ordinary checker must return 1..1925, within private publication storage and caller capacity; serialized length must equal that checked value before copying. Tests exercise every input count, maximum outputs, all single-profile capacities, SIZE_MAX counts/lengths, guards and dirty provider output. |
| Integer overflow/underflow; signed/unsigned conversions | Production uses bounded size_t counts and lengths; count times the fixed signature size is bounded by eight entries. No untrusted offset or allocation arithmetic is added. Fuzzer controls require 16..144 bytes, read fixed-width words after the floor, and process only complete four-byte mutations. Reference append subtracts only after validating the used capacity; script arithmetic follows DER 8..71 checks. Candidate height/cutoff conversions and branch selection stay in previously qualified helpers. |
| Use-after-free; double-free; leaks; dangling pointers | Work owns all eight copied signatures, candidate and parsed transaction for one synchronous invocation; publication owns its separate private wire. No production heap/free/descriptor is added. Owner rows are borrowed under the same exclusive adapter lock. Fault observers inspect and retire captures inside live zero callbacks; expected cleanup counts fail before post-lifetime pointer-marker comparisons. Independent OpenSSL/libsodium helpers retain their checked cleanup paths. Large serial fixtures have static storage and no pointer escapes a case. |
| NULL dereferences; uninitialized memory | Whole work representations initialize, including padding and unused signature tails; each digest starts zeroed. Every parse/hash/script/check/encode status gates the successor. Valid-looking dirty failure data cannot publish. NULL arguments cannot advance liveness. Separate publication is internal and requires its already validated non-NULL stable caller objects. All entered work and every nonempty row digest clear on success/failure. |
| Pointer arithmetic; format strings | Production accesses only fixed typed objects and checked row spans. No alignment cast, raw serialized pointer, path or variable format string is introduced. Inputs/outputs must remain stable and nonoverlapping. Independent reference traversal bounds fixed unsigned input spans and every destination append; it never uses a production parser/serializer to create expected bytes. Test diagnostics expose fixed text and public line identifiers only. |
| Stack usage; allocation limits; resource exhaustion | Measured optimized host frames are 3304 bytes for assembly and 2040 for publication, individually below the unchanged 4096-byte cap. Nested codec/hash/provider frames remain separate; no whole-call-chain 4096-byte claim is made. No recursion/VLA/production heap, RNG, mutable global or worker exists. Work is bounded by eight public signature verifications and one 1925-byte serialization. Fuzzer input 144, per-case five seconds, campaign 121 seconds and RSS 512 MiB bound synthetic testing. |
| Malformed serialization; races; explicit lifetimes | Only owned unsigned bytes and assessed P2PKH input hashes select what is signed. Every original field except input scripts survives. P2SH, altered signatures, wrong candidate domains and unsupported context refuse. Shared liveness checks surround private encoding; cancellation, replacement, rollback and expiry faults preserve both outputs and cannot clear an unrelated new owner. The operation requires exclusive access and does not model concurrent lock correctness. Reusing supplied invocation time does not prove completion-time freshness; the future authorized adapter must obtain and validate an actual completion/delivery clock. |
| Secret leakage, cryptography and authority | Only public context/signatures/transaction data enter the assembler. It creates no signature, key, entropy, storage or JNI capability, consumes no consent and extends no deadline. Complete owned work clears; synthetic signing fixtures use public scalars and clear their copies. Independent host verification of completed bytes and exact manual wire construction complement the pinned provider checks, without claiming a general script interpreter, current chain, unspentness, wallet ownership, hardware custody or broadcast authority. |

All 83 native ASan/UBSan/LSan groups pass in 61.24 seconds. Production and new
fixture/fault/reference/oracle/fuzzer Clang/GCC analyses pass, with unchanged
10/15 complexity caps (500/1145 functions). All 26 mutants are detected: 25
intended assertions and one ASan negative-size copy interception. Initial test
complexity failures were repaired by bounded named operations and an exact
stage table; no warning, assertion or threshold was weakened. The differential
fuzzer completes 74,095 cases in 121 seconds without a finding. Release-archive
tests pass on x86-64 API 30/35/36; ARM64 is compiled only. See
[TRANSACTIONS.md](TRANSACTIONS.md) and
`.cache/android-wallet/review-signed-wire-20260914/` for exact evidence scope.

## Qualified host emulator child reaping — 2026-09-14

Scope: a Linux x86-64 host SDK launcher, two small C17 adapter units, native
wait/loader tests and a differential site classifier fuzzer. These objects are
excluded from Android builds and never link into the wallet or consensus code.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The classifier rejects NULL before access and rejects lengths >=4096 before reading. A readable caller span must be at least the fixed module suffix length before subtraction/comparison; embedded NUL refuses. The ELF adapter bounds its loader-owned string scan at4096. Tests exercise exact4095/4096, SIZE_MAX, short/NULL/nonterminated strings, suffix corruption and embedded NUL. The fuzzer checks17..4113 bytes before fixed controls/64-bit words and passes only its actual bounded body unless testing a length that must refuse before access. |
| Integer overflow/underflow; signed/unsigned conversions | Caller address must be >=nonzero base before subtraction and must differ by the exact measured return offset. Tests include wraparound aliases and UINTPTR_MAX; the independent reference uses checked addition instead. Module subtraction follows the suffix floor. The libc pid_t/status/options types are preserved without narrowing; only a positive exact child may qualify. Function/data pointer representation is explicitly restricted to the measured Linux ELF ABI and checked for equal size before memcpy. |
| Use-after-free; double-free; leaks; dangling pointers | Production allocates no heap, descriptor, worker or PID registry and retains no child or borrowed module pointer. The one libc function pointer initializes during loading and remains immutable for process lifetime. The measured SDK owns the exact child through kill/wait and will not revisit it; ordinary owners are never reaped opportunistically. Native fixture children set parent-death SIGKILL with a parent identity recheck; each owner collects its exact child, and concurrent workers are joined. Failed mutation paths clean a still-live owned child before asserting. |
| NULL dereferences; uninitialized memory | Missing libc symbol fails with fixed text and exit125 before SDK execution; a defensive wait entry guard covers constructor ordering. Dl_info is fully initialized, lookup success and filename non-NULL gate inspection, and the loader pointer is copied only through a size-checked representation. The pure helper reports ENOSYS for a missing provider. Dirty/failed lookup and NULL-base/name tests preserve ordinary wait behavior. Fixture implementation definitions remove only inherited mock-name nonnull declarations; their runtime assertions and production declarations stay intact. |
| Pointer arithmetic; format strings | Only validated module suffix arithmetic is performed; code addresses are compared as uintptr_t rather than dereferenced or reconstructed. The return address uses the current frame only. No input becomes a format string; diagnostics contain fixed public text. The private launch report uses exclusive mkdir and rejects loader separators. Full SDK library SHA256 qualification precedes compilation/launch; the trusted local SDK must remain stable through launch. No permanent SDK patch or global loader setting exists. |
| Stack usage; allocation limits; resource exhaustion | Final optimized host frames with the launcher's stack protector measure24 bytes for site classification,24 for wait delegation,104 for the ELF wait entry and8 for initialization. Nested libc costs are separate. There is no recursion/VLA, production allocation or added thread. Exactly the qualified post-SIGKILL wait blocks on its owned PID and retries EINTR; other errors return immediately. Uninterruptible kernel cleanup has no wall-clock guarantee, so disposable runs retain an outer timeout. The bounded stand-in fixture also has parent-death handling and a30-second alarm. Fuzz input4113, per-case5 seconds and RSS512MiB bound its121-second campaign. |
| Malformed metadata; races; explicit lifetimes | A different SDK library, caller offset, module suffix, nonpositive PID, status pointer or options value cannot enable the workaround. Unqualified calls delegate once with original arguments; ordinary EINTR is not swallowed. Incoming errno survives loader inspection and provider return/error semantics are preserved. The exact SDK timeout releases its bookkeeping lock before kill/wait and exclusively owns this child. Initialization precedes workers; concurrent child tests exercise four independent owners. This is qualified emulator test equipment, not an async-signal-safe or portable libc replacement. |
| Secret leakage, custody and authority | No wallet, key, entropy, JNI, chain predicate, network protocol or persistent app data is accessed. Stand-in commands are inert; real-device checks use synthetic public fixtures. The adapter cannot collect old zombies owned by an unrelated live emulator. Those parents and their data are preserved; no process-wide kill or wildcard wait is introduced. Fuzzers and fault providers remain host-only. |

All85 native ASan/UBSan/LSan groups pass in61.37 seconds, including both newly
registered host groups. Android/JVM, debug/release lint and scanner fixture
checks pass. Production and all new fixtures pass Clang/GCC analysis; separate
complexity reports enforce unchanged10/15 caps over5/23 functions. All16
mutants are detected (15 intended assertions, one NULL-access sanitizer).
The independent classifier fuzzer completes71,385,647 cases in121 seconds
without a finding; the registered CMake target also builds and replays its
accepted seed. An initial unknown-target build used a stale generated Makefile;
explicit CMake regeneration fixes it without changing source or build checks.
Real SDK traces, exact identities, shutdown fixture findings and minified APK
camera acceptance are recorded in [EMULATOR_REAPING.md](EMULATOR_REAPING.md)
and `.cache/android-wallet/adb-reaping-20260914/`.

## Single-link wallet ciphertext records — 2026-09-14

Scope: one additional descriptor metadata predicate in `storage_read.c`, its
public contract, dedicated real/fault storage tests and a bounded filesystem
fuzzer. Existing lock/journal link rules, GCM, record bytes and consensus stay
unchanged. The initial real-alias regression failed before this predicate.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Production retains its124..140-byte size bound,140-byte private read buffer, bounded read/EOF loops and publish-only-on-success contract. The new link predicate runs before reading. Tests preserve guarded output bytes, SIZE_MAX length and both pending flag states on refusal. Fuzzer input must be2..142 bytes; body offsets cover at most140 bytes, capacity is0..140 within its142-byte guarded array, and tail comparisons follow successful checked record length. Real fixture reads use141 bytes to catch unwanted suffixes. |
| Integer overflow/underflow; signed/unsigned conversions | The production comparison uses native nlink_t against1 with no narrowing/arithmetic. Fault tests cover0/1/2 and an explicitly cast negative-one value in that native type. Counts never control allocation or a loop. Bounded fixture lengths<=140 convert safely to off_t for metadata comparison; negative read results are rejected before size_t conversion. Fuzzer subtraction follows its2-byte floor and all source/output tail offsets stay within fixed arrays. |
| Use-after-free; double-free; leaks; dangling pointers | The existing read helper consumes each opened record descriptor once before returning, including metadata refusal; store cleanup closes its own lock/directory. Production adds no allocation, pointer, descriptor or retained state. Every real fixture owns one mkdtemp directory and only fixed names beneath its descriptor. Fuzzer owns one directory until checked atexit cleanup, replaces at most three fixed record/alias names per case and closes every raw verification descriptor. Ten thousand additional executions pass with a64-descriptor process limit. |
| NULL dereferences; uninitialized memory | Existing argument and syscall success guards precede metadata access. struct stat initializes before fstat; a failed syscall cannot authorize a read. Tests initialize every public/ciphertext/output object. The fault wrapper requires non-NULL metadata and changes only a successful regular-record result; its observed count must be exactly one for each targeted read/promotion. Its directory/empty-lock metadata remains real. |
| Pointer arithmetic; format strings | No production pointer arithmetic or format string changes. Fixture names are fixed literals and paths come only from their bounded owned mkdtemp template. Public diagnostics identify source lines, not wallet input. Fuzzer source mutation and guard/tail comparisons use validated spans; it never fuzzes an ambient locator, directory prefix or operator path. |
| Stack usage; allocation limits; resource exhaustion | Optimized host read-file/storage-read frames with the stack protector measure376/424 bytes; nested unchanged helpers remain separate. No new production buffer, recursion, VLA, worker, retry or heap exists. Existing syscall retry and140-byte record bounds remain intact. New host fixtures pass strict warnings, both analyzers and unchanged10/15 complexity caps. Fuzzing has142-byte inputs,5-second cases,121-second campaign and512MiB RSS cap; malformed/aliased files never expand the read bound. |
| Malformed serialization; races; explicit lifetimes | Exactly one link is mandatory for committed and pending record descriptors. Extra names and detached metadata refuse both read and promotion; failed read outputs, inode identities, bytes and names remain unchanged. Only the fixture removes its own extra alias before proving a normal retry. This is a metadata snapshot, not proof against a malicious same-UID actor changing the private directory after fstat; trusted path ownership and cooperating locks remain prerequisites. Normal codec validation and authenticated promotion requirements are unchanged. The fuzzer models metadata/capacity/pending policy and reuses the separately tested record parser for content status; it is not an independent parser oracle. |
| Secret leakage, custody and authority | Storage receives bounded ciphertext/public metadata only. Fixtures use published all-zero entropy and inert ciphertext; Android JNI round trips use a public test AES key in a new invocation-owned directory. No operator wallet, Keystore alias, signing capability, seed export, repair, unlink or overwrite is added to production. Refusal does not silently remove an alias or claim GCM authentication, rollback protection or chain compatibility evidence. |

All87 final sanitizer groups pass in61.38 seconds. All four link-rule mutants
fail intended assertions, including zero-link acceptance. The filesystem fuzzer
completes305,619 executions in121 seconds without a finding (observed77MiB),
followed by10,000 cases under the descriptor bound. Real aliases and controlled
metadata have separate mandatory registered tests, preserving the broader Linux
suite's FIFO and other filesystem assertions. Android refuses creation of FIFOs
and actual hard links in the attempted fixtures; those failed invocations remain
recorded rather than counted as device acceptance. The unmodified descriptor
metadata executable from the actual release archive passes on x86-64 API30/35/36,
SHA256 `6528afa300ba461c2029ba8e93af5728c2117bf38dcf6d830c740192fe211667`.
ARM64 is compiled only. Two real JNI/storage/GCM round trips also pass on each
API in1.889/18.396/5.302 seconds. Final Clang/GCC analyses,10/15 complexity caps
(500/1160 functions), Android/JVM/build/lint, APK fixture isolation and alignment
checks pass. Exact identities and results remain in
`.cache/android-wallet/storage-links-20260914/`.

## Independent camera sampling reference — 2026-09-14

Scope: test-only `camera_reference.c/.h`, `test_camera_sampling.c`, the existing
camera fuzzer and their CMake registration. Production sampling, IPC bytes,
native libraries and consensus are unchanged. The reference enumerates occupied
source bytes and candidate strides without calling a production bounds/sampling
helper or reusing its ceiling-division calculation. It verifies exact status,
header, every sampled byte, returned length and the entire unused output span.
The former fuzzer guard/length assertions are subsumed by these stronger checks.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | The reference first requires non-NULL pointers and exactly 147463 guarded bytes. Input width/height are 21..1024, pixel stride 1..4 and row stride at most 8192 before enumeration. The occupied final byte must fit the supplied input length, itself at most 8 MiB. Only a successful independently validated plan permits header/pixel reads. Output dimensions are 21..384 and returned length must equal the independently computed 5+columns*rows before tail arithmetic. Failure checks preserve all output bytes and the original length 17. The deterministic fixture corrupts every prefix/header/pixel byte and four near/distant tail bytes to test the reference itself. |
| Integer overflow/underflow; signed/unsigned conversions | Occupied-row enumeration is at most 4093; adding at most 1023 row strides stays below 8 MiB. Candidate strides are the finite nonzero values 1..3. Sample counts are bounded by 1024 before output validation and their accepted product by 147456. Post-sample integer increments can reach at most 1026*8192, still representable on supported 32/64-bit targets; no pointer is formed from an unused terminal offset. Header byte promotion uses size_t and values at most 255. Fixture pattern shifts/XOR operate on size_t, with explicit masked conversion to uint8_t. SIZE_MAX input/metadata/returned-length cases refuse before unsafe access or addition. |
| Use-after-free; double-free; leaks; dangling pointers | Reference helpers allocate nothing and retain no pointer. The unit fixture owns two constant checked allocations, frees both on allocation failure and once each after validation. A failing assertion aborts the isolated process. The fuzzer retains its existing one checked packet allocation and frees it after each successful check/decode; no second image copy or new owner is added. Caller input and output remain alive and disjoint for each call. |
| NULL dereferences; uninitialized memory | Reference entry guards all borrowed pointers. Each plan initializes to zero and becomes usable only after independent validation. The fixture initializes all 8 MiB of source markers, the entire guarded output span and the output-length sentinel. Allocation failure is checked before access. No returned production header or length is trusted to size a read. |
| Pointer arithmetic; format strings | Source indices advance only within independently validated sampled coordinates. Pixel/output offsets are bounded by the plan rather than production-returned metadata. Tail scans stay within the exact caller allocation. Diagnostics use fixed public strings and a source line number; no input becomes a format string. The unit/reference have no path, descriptor or file operation. |
| Stack usage; allocation limits; resource exhaustion | Optimized host frames with stack protection measure 88 bytes for reference matching, 40 for its pixel helper, 648 for the deterministic main, 104 for the fuzzer entry and 3592 for its existing packet decoder check; nested provider costs are separate. No recursion, VLA or new global mutable state exists. The unit's two allocations total 8536071 bytes. Its layout matrix has exactly 1200 cases plus fixed boundary/corruption cases; the registered timeout is 20 seconds. Reference checking adds no heap allocation to fuzzing. Campaigns use 147461-byte maximum input, five-second cases, 512 MiB RSS cap and explicit outer timeouts. |
| Malformed serialization; races; explicit lifetimes | Exact v1 dimension/packet caps are compile-time asserted. Invalid input geometry, insufficient input, too-small output, wrong status/header/length, altered sampled pixels and any untouched-region write fail. The reference chooses the first fitting stride and refuses over-thin results, matching the stated point-sampling contract. It is pure per-call test code; fixtures run with owned buffers and no asynchronous callback or camera driver. It independently models packing, not QR decoding or network authentication. |
| Secret leakage, custody and authority | Source images are coordinate markers or published unfunded QR fixtures. No camera, wallet, key, entropy, Keystore, network peer or canonical file is opened by the new unit/reference. Android tests execute an isolated public test executable against actual release archives. Test/reference sources are excluded from production Android libraries; no consensus predicate, custody rule or packet format changes. |

All 88 final ASan/UBSan/LSan groups pass in 63.40 seconds. Clang/GCC analysis,
strict warnings and unchanged 10/15 complexity caps pass (500 production and
1172 test functions). All 12 sampler mutations and four reference mutations
fail intended assertions. The public-QR fuzz campaign completes 4020 cases in
121 seconds (observed 262 MiB); a further 1656 cases in 61 seconds include
explicit stride-two/three and capacity-refusal seeds (observed 259 MiB).
Neither campaign reports a finding.

The release-archive test passes on x86-64 API 30/35/36 with identical SHA256
`bdaae04da7ab4ac0d2b461760e32fb1c942e62477669f3a1c803128eb3ef4fa6`.
ARM64 is compiled only, SHA256
`3c72cc9dc9c7423f5a959605c2b9c784931ff6d29e3ae53ae197c8f84d68dde9`.
Both executables have 16 KiB load alignment, RELRO, immediate binding and a
nonexecutable stack. Android/JVM/build/lint checks pass; the unsigned release
APK is byte-identical to the preview-cleanup milestone. Exact sources, archives,
mutants, seeds and evidence are in `.cache/android-wallet/camera-sampling-20260914/`.

## Refuse mandatory separators in request metadata — 2026-09-14

Scope: one existing range in `utf8_text.c`, two payment unit groups, one
independent fuzzer property and a public Android QR/JNI regression. Unicode
17.0.0 defines exactly U+2028/U+2029 as Zl/Zp. The downloaded UnicodeData has
SHA256 `2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c`.
This extends display refusal, without changing consensus or URI serialization.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Production changes only the lower bound of one constant range; the existing checked UTF-8 decoder and 200-byte field bound are unchanged. Four URI fixtures require the entire caller output representation to remain unchanged on refusal. The independent fuzzer byte scan runs only after both successful output lengths are checked at most 200, and reads three bytes only when i+2 is below that length. New direct fixtures use a four-byte initialized buffer and check the encoder's returned length. |
| Integer overflow/underflow; signed/unsigned conversions | Range endpoints and decoded codepoints remain uint32_t. No production arithmetic or conversion changes. Fuzzer indices and lengths are size_t bounded by 200, so i+2 cannot overflow. Fixed codepoint arrays use bounded size_t iteration; the only new narrowing conversion handles explicitly checked ASCII below 0x80. URI lengths are compile-time sizeof minus one. |
| Use-after-free; double-free; leaks; dangling pointers | The change adds no production allocation, ownership transfer or retained pointer. Native fixtures and new fuzzer helpers use fixed automatic/constant arrays only. Android owns each public pixel array for one synchronous decode and clears it in finally. Existing JNI/native lifetime and output publication contracts remain intact. |
| NULL dereferences; uninitialized memory | Existing entry guards and checked UTF-8 results precede table lookup. Native output and its saved byte representation initialize with memset/memcpy before any comparison, including padding. All encoded buffers initialize to zero and nonzero/expected lengths are checked. The fuzzer reads decoded fields only after successful parsing and checked lengths. |
| Pointer arithmetic; format strings | No production pointer or diagnostic changes. Fuzzer offsets are bounded by independently checked decoded lengths. Fixtures use only fixed public addresses and metadata; no attacker text becomes a format string or locator. |
| Stack usage; allocation limits; resource exhaustion | The production range table has the same size and iteration count, with no new buffer, loop, recursion, VLA, worker or allocation. Optimized host visible-text frame remains 24 bytes with stack protection; decoder/provider nested costs are separate. New fixtures pass strict Clang/GCC warnings and analysis, plus unchanged 10/15 complexity caps. The fuzzer uses 1024-byte maximum inputs, five-second cases, a 120-second campaign budget, 512 MiB RSS cap and an outer timeout. |
| Malformed serialization; races; explicit lifetimes | Raw UTF-8 separators and all four percent-encoded label/message combinations refuse. Existing malformed UTF-8 and full Cf coverage remain; ordinary space, NBSP, U+2027, narrow NBSP, CJK and emoji acceptance are retained. Six mutants prove both separators, prior bidi checks and permitted neighboring characters are enforced. The byte-pattern fuzzer property does not call the production classification predicate. No asynchronous state, normalization, wire rewrite or request authority is introduced. |
| Secret leakage, custody and authority | Every fixture uses published unfunded addresses and public text/images. No wallet, key, signing capability, entropy, authentication or network source is used. Refusing mandatory text breaks makes request display stricter; it does not claim all Unicode confusables are safe or authorize any payment. Consensus predicates, address/amount encoding and chain/network compatibility are unchanged. |

The baseline native assertion, independent fuzzer invariant and real QR/JNI
test all reproduce the defect, with all four device combinations accepted.
Final validation passes 88 sanitizer groups in 63.63 seconds, Clang/GCC analysis,
strict warnings and unchanged complexity caps. All six mutants fail intended
assertions. Fuzzing completes 179701 cases in 121 seconds without a finding,
observing 110 MiB; fuzzer SHA256 is
`319910eca5593c2fb6a5b0d9eb75b08370c24320a22d67e0d4dfdddd09d06fb5`.

Actual release-archive native payment tests pass on x86-64 API 30/35/36;
ARM64 is compiled only. Both executables retain 16 KiB alignment, RELRO,
immediate binding and nonexecutable stack. Three QR/JNI tests pass on each API,
and fresh API 30 minified camera acceptance passes in 9.385 seconds. Android/JVM,
build/lint, fixture isolation, native alignment and architecture checks pass.
Exact identities and evidence remain in
`.cache/android-wallet/request-separators-20260914/`.

## Independent UTF-8 request-text reference — 2026-09-14

Scope: test-only `utf8_reference.c/.h`, its exhaustive registered unit, payment
fuzzer assertions and CMake wiring. No production behavior or Android library
changes. The reference matches byte classes from
[Unicode 17 Table 3-7](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-3/#G27506)
and [RFC 3629 section 4](https://www.rfc-editor.org/rfc/rfc3629.html#section-4),
instead of decoding and checking the production minimum/scalar ranges. Display
classification uses binary search over the existing complete 170-scalar Cf
enumeration plus Cc and Zl/Zp predicates. The enumeration was regenerated from
pinned UnicodeData 17.0.0 and compared byte-for-byte; its SHA256 is
`ccea93e53a2df5981150ff561cafa8856fab2f59eaee60bac93c2dd1d56fb9e7`.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Reference entry rejects NULL and lengths above its independently asserted 200-byte cap before access. The selected immutable rule has length 1..4 and must fit remaining input before any tail or scalar read. Tail reads and scalar construction stay inside that rule span. The fuzzer checks both decoded field lengths before reference calls and retains all previous output-preservation/address/amount assertions. Unit encoding requires four writable bytes, width 1..4 and a bounded code value; suffix writes at offset 196 have five remaining bytes. Fixed four-byte prefix fixtures and a 201-byte boundary array require no caller-derived storage. |
| Integer overflow/underflow; signed/unsigned conversions | Offset never exceeds input length; subtraction precedes addition, and each rule consumes 1..4 bytes. Valid byte classes bound scalar multiplication by 64 to U+10FFFF, with subtraction performed only after byte validation. Binary search maintains 0<=low<=high<=170. Unit scalar counters stop below 0x200000; width/count loops and total case counts fit 32-bit size_t. Base-64 division, prefixes and byte casts are checked/bounded, including deliberately overlong forms. SIZE_MAX length refuses before access or arithmetic. |
| Use-after-free; double-free; leaks; dangling pointers | The reference retains no input, returns no borrowed pointer to its caller, and uses only immutable constant tables. Its private rule pointer refers to static storage throughout the synchronous call. Unit and new fuzzer checks allocate nothing and open no resource. Existing fuzzer ownership and caller-output contracts are unchanged. |
| NULL dereferences; uninitialized memory | NULL input refuses even at zero length. Missing byte rules and insufficient remaining spans refuse before dereference. All unit byte buffers initialize before calls; every selected rule initializes all five fields. Unit comparisons use explicit expected statuses for every canonical codepoint, overlong form and out-of-range ceiling pattern, not uninitialized production results. |
| Pointer arithmetic; format strings | Every text+offset is within an entry-validated span and each tail read is bounded by its checked rule length. Unit suffix pointer arithmetic stays within its fixed array. Diagnostics print only a fixed public message and source line. No text becomes a format string, path, callback or command. |
| Stack usage; allocation limits; resource exhaustion | No heap, VLA, recursion, thread or mutable global is added. Optimized host frames with stack protection measure 48 bytes for reference text, zero for classification, 296 for the unit main, and 1064 each for existing fuzzer entry/request frames; nested calls/provider costs are separate. Tables contain 170 uint32_t values and nine five-byte rules. The unit performs 2427023 comparison cases under a 20-second registered limit, measured 0.98 seconds in its first sanitizer run. Fuzzing uses 1024-byte inputs, five-second cases, a 120-second campaign, 512 MiB RSS cap and an outer timeout. |
| Malformed serialization; races; explicit lifetimes | All 1114112 codepoint positions, including 2048 invalid surrogates, are checked. The expected refusal count independently binds 65 Cc, 170 Cf and two Zl/Zp values. Units additionally cover every overlong form below U+10000, all four-byte patterns above U+10FFFF through 0x1FFFFF, every first/second-byte pair at lengths 1..4, and full/truncated 200-byte fields. Twelve decoder mutants and six reference mutants all fail intended assertions. The fuzzer now compares raw text against the reference and requires every successful decoded field to satisfy it; this subsumes the former separator-only property. This is an independent UTF-8/display-policy reference, not a complete URI/address parser or spoof detector. |
| Secret leakage, custody and authority | Fixtures are public Unicode bytes, unfunded request text and synthetic malformed inputs. No key, wallet, RNG, Keystore, network or operator path is used. Test code is excluded from application libraries. No consensus, normalization, address/amount serialization, signing or consent rule changes. |

All 89 final ASan/UBSan/LSan groups pass in 64.34 seconds. Production/providers
and all new test/fuzzer sources pass strict Clang/GCC analysis; unchanged 10/15
complexity caps cover 500 production and 1187 test functions. All 18 mutants
fail intended assertions. The expanded payment fuzzer completes 181911 cases
in 121 seconds without a finding, observing 109 MiB; its SHA256 is
`5aa226a5cb5eedd7cc9a131bf87165a0352db91f6d7d58d8404412099e501955`.

The exact release-archive executable passes on x86-64 API 30/35/36 with SHA256
`ec927faba469e48505baae04d8b1a3a1a92019230064c2b581a4db8a7ef8ecc6`.
ARM64 is compiled only, SHA256
`a8abe09b59d99c5aec3150bb7eaef3523fc79b68e853e711d39d776895c237af`.
Both retain 16 KiB alignment, RELRO, immediate binding and nonexecutable stack.
Android/JVM/build/lint, fixture isolation, APK alignment and architecture checks
pass. All three application/test APKs compare byte-identically with the prior
review-concealment milestone, retaining that exact device evidence. All source,
standards, mutants, seeds and artifacts are in
`.cache/android-wallet/utf8-reference-20260914/`.

## Observe RNG scratch erasure and exact retry limits — 2026-09-15

Scope: the existing registered `test_random.c` and its host-only linker flags.
Production C, the entropy API and Android libraries are unchanged. The fixture
interposes OS reads and the real zeroization call, observing the scratch bytes
only while the production call still owns them. No allocator or production
fault-control interface is added.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Every intercepted read requires the exact remaining length and destination offset in a maximum 64-byte scratch buffer before invoking the provider. The zeroization observer requires the captured base pointer and full 64-byte length before reading the cleared bytes. Caller fixtures use 66-byte arrays with untouched boundaries; retry-boundary tests compare every caller byte. |
| Integer overflow/underflow; signed/unsigned conversions | Read counts are converted only after positivity checks and must fit the remaining span before incrementing the observed offset. Requested length is at most 64 before pointer arithmetic; completed reads are strictly below 128 before increment. For lengths 1..64, the fixture's three-byte read count is 1..22, making 128 minus that count plus zero/one safe. Oversized provider returns remain synthetic bounded values. SIZE_MAX is refused without provider or wipe calls. |
| Use-after-free; double-free; leaks; dangling pointers | No allocation or descriptor is added. The scratch pointer is captured inside the synchronous OS-read callback, inspected during the synchronous wipe, and retired there. If a mutant omits wiping, the caller assigns NULL immediately after return without evaluating or dereferencing the expired pointer. Existing caller output wiping runs outside observation. |
| NULL dereferences; uninitialized memory | The observer requires a non-NULL captured scratch pointer and checked length before access. State is reset before every call. Caller arrays initialize before checks, and synthetic bytes initialize with memset. Invalid NULL/length calls must make no OS or wipe call. |
| Pointer arithmetic; format strings | Scratch plus filled is formed only after the filled offset is checked below a request of at most 64 bytes. The provider sees exactly the remaining admitted span. Diagnostics contain a fixed message and source line, never random bytes or caller data. |
| Stack usage; allocation limits; resource exhaustion | The expanded registered test performs 330 bounded API cases, including success and exhaustion exactly at attempt 128 for every admitted width. No heap, VLA, recursion or worker is introduced. Host optimized test main measures 224 bytes; observer frames measure 16 bytes. Strict warning, format, conversion, shadow, VLA and 4096-byte frame checks apply. The initial helper exceeded test complexity 15; a named interruption predicate removed the violation without changing the cap. |
| Malformed input; races; explicit lifetimes | The host-only observer is single-threaded. It checks partial reads, zero and oversized returns, nonblocking flags, immediate/partial EAGAIN, EINTR and exhaustion after a partial secret. Failure must leave all caller bytes unchanged. Ten isolated mutations fail intended assertions; the old test accepts the omitted-wipe mutation. Mutants never replace working-tree production source. |
| Secret leakage, custody and authority | Synthetic marker bytes and the existing unfunded OS-RNG exercise are used; no wallet, seed derivation, private key, authentication, network or signing operation occurs. The real zeroization primitive clears each scratch span before inspection; the fixture retains no secret bytes or expired-pointer reads. This proves these observed buffers and failure paths, not erasure of all compiler, kernel or hardware copies. |

All 89 ASan/UBSan/LSan groups pass in 64.28 seconds, with production/provider
Clang/GCC analysis and unchanged 10/15 complexity caps. The changed fixture
also passes separate strict Clang/GCC analysis. After adding its stricter
compile flags, the final focused target is rebuilt and rechecked. The exact
release-archive test passes on x86-64 API 30/35/36, with SHA256
`7686e6e5ead470e6c2cce0a8eb68b5b57fe68f12e3260772490db5ed6baa6bd1`.
ARM64 compiles with SHA256
`59dc5743ae5ed72468c519cec7967152688de675b96668054345d5a8827173b0`;
hardware execution remains unobserved. Android/JVM/build/lint, fixture isolation
and native alignment pass. The release APK remains identical to the canvas
checkpoint. Source, mutants, hashes, analysis and device evidence are retained
in `.cache/android-wallet/resume-20260915/random/`.

## Refuse a phrase allocation accompanied by a JNI exception — 2026-09-15

Scope: `new_phrase` in `jni_keys.c`, the existing fake-VM regression and its
shared fuzzer. If an allocation callback supplies both an array and a pending
exception, the native helper now returns NULL, matching the byte-array helper.
The old helper retained the array as its native return value while skipping
the copy. This is a synthetic VM fault-contract test; no production Android
occurrence, exception bypass or disclosure of phrase contents is claimed.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Production adds only a NULL assignment in the existing refusal branch. The 215-character bound, checked JNI copy and 430-byte native wipe remain unchanged. The fixture initializes the full fake-array union before allocation return and checks every byte remains zero when copying is refused. |
| Integer overflow/underflow; signed/unsigned conversions | No production arithmetic or conversions change. Six allocation ordinals are fixed unsigned values 1..5, indexed only by an operation below the existing seven-operation cap. The union inspection uses size_t and sizeof. The fuzzer consumes a previously unused bit of its already-checked two-byte control prefix. |
| Use-after-free; double-free; leaks; dangling pointers | The native array remains a local JNI reference released with the invocation; no new pin, global reference, allocation or owner is introduced. Native scratch retains its existing cleanup path. Fake arrays have static fixture storage, reset between synchronous cases; tracked scratch pointers are still retired during their live wipe. |
| NULL dereferences; uninitialized memory | NULL and pending-exception allocation paths both force NULL return before any region copy. The test supplies an initialized non-NULL fake array together with a pending exception, then verifies no later forbidden VM call occurs. Existing NULL-without-exception and ordinary allocation-failure cases remain. |
| Pointer arithmetic; format strings | No production pointer arithmetic or diagnostics change. Fixture byte inspection stays within the initialized result union. Failure diagnostics print only fixed text and source lines. |
| Stack usage; allocation limits; resource exhaustion | Production adds no storage, loop, recursion, task or allocation. The unit adds six bounded cases. Unit/fuzzer and production retain strict warnings, 4096-byte frame checks and 10/15 complexity limits. Fuzzing uses at most 217 input bytes, five-second cases, 512 MiB RSS and a bounded campaign with an outer timeout. |
| Malformed input; races; explicit lifetimes | The fake VM checks that allocation is its final ordinary call while the exception is pending, all touched native secret spans are cleared, and borrowed inputs remain intact. All six array-producing key/header adapters share the new scenario. The boolean confirmation entry has no allocation. Fuzzer NULL-only and array-plus-exception modes are mutually exclusive. No concurrency or lifecycle state changes. |
| Secret leakage, custody and authority | Fixtures use published unfunded mnemonic material and deterministic marker RNG bytes. The Android check selects only the existing public-vector method. No wallet, Keystore policy, production seed, network, transaction authority or consensus predicate changes. Pending exceptions remain pending; this defensive native return contract does not claim erasure of every managed/provider copy. |

The preserved native regression and seeded fuzzer both fail intended assertions
on the old source. The corrected source passes all 89 ASan/UBSan/LSan groups in
64.56 seconds, strict Clang/GCC analysis of production and both fixture modes,
and unchanged complexity caps. Seeded fuzzing completes 52726 cases in 121
seconds without a new finding; fuzzer SHA256 is
`14be9d33108cd52162bc41d2dc36926f1861489ac2e82de7342bf16d07b18158`.
Android/JVM/build/lint, fixture isolation and native alignment pass. The public
mnemonic/JNI method passes on API 30/35/36 in 0.334/2.147/0.700 seconds. No
hardware-custody or physical-camera acceptance is added. Source, baseline
assertions, corpus and exact APK identities are retained in
`.cache/android-wallet/resume-20260915/jni-phrase/`.

## Allocate only the measured camera packet span — 2026-09-15

Scope: shared C camera geometry, the new no-pixel-read sizing API, camera JNI
allocation and existing native/sampling/fuzz tests. The prior JNI path requested
and cleared 147461 bytes per frame. Its captured 640x480 fixture produces a
76805-byte packet. The new path requests and clears exactly those 76805 bytes,
70656 fewer (about 48%). These are observed allocation requests and wipe spans;
allocator overhead, process RSS, camera-driver storage and CPU time are not
measured. No packet, pixel-sampling, QR policy or Zclassic wire format changes.

| Required hazard | Explicit review |
| --- | --- |
| Buffer overflow/underflow; out-of-bounds access | Sizing and packing share one geometry function and the existing source-span validator. Input width/height 21..1024, pixel stride 1..4, row stride at most 8192 and total span at most 8 MiB precede arithmetic. Sampling retains the same proven coordinates. JNI allocates the measured 446..147461 bytes and passes that exact capacity to the independently guarded pack call. Eight allocation cases check all output pixels, header dimensions and unused output; existing full-wipe checks inspect every allocated byte before free. |
| Integer overflow/underflow; signed/unsigned conversions | The unchanged ceiling divisions produce a sampling step of 1..3 and accepted target dimensions 21..384. Products are at most 147456, and adding the five-byte header is safe on 32-bit size_t. JNI signs/ranges and direct-buffer capacity/offset checks remain before pointer access. Fixture source products use positive jint dimensions at most 1024; known expected output dimensions are independent constants. SIZE_MAX and malformed layout bounds retain explicit tests. |
| Use-after-free; double-free; leaks; dangling pointers | One synchronous JNI invocation owns its checked allocation and its single wipe/free path. The stack-owned layout cannot change between sizing and packing. The Android caller retains the Image and direct buffer through that call. No cache, reusable global buffer, pin, handle, retained pointer or additional allocation is introduced. Allocation failures and all existing VM failure points retain cleanup tests. |
| NULL dereferences; uninitialized memory | Sizing refuses a NULL result pointer and delegates NULL-layout refusal to the existing validator. It publishes length only on success. Target layouts and local lengths initialize before use. JNI reaches malloc only after successful size and direct-buffer checks. The fixture initializes public pixels and allocation memory, and verifies malformed sampled dimensions reach neither the VM buffer nor allocator. |
| Pointer arithmetic; format strings | All pixel offsets remain covered by the original image-span proof. Pack capacity is checked before packet+5 or any write. The sizing function has no image pointer and cannot read pixels. New diagnostics contain only public dimensions, allocation/packet byte counts and source lines. |
| Stack usage; allocation limits; resource exhaustion | Production adds only small fixed geometry locals and no VLA, recursion, worker or retained state. Optimized host frames measure 32 bytes for sizing, 64 for packing and 128 for camera-plane JNI; decoder costs remain separate. The largest new public source fixture is a fixed 1 MiB test-only static array. Clang/GCC warnings/analysis and 4096-byte per-frame/10-and-15 complexity caps pass. Fuzzing retains five-second cases, a 512 MiB RSS cap and a bounded 120-second campaign. |
| Malformed serialization; races; explicit lifetimes | Independent reference comparison still checks 1200 layouts and every sampled byte. Unit/fuzzer checks now bind the size query to that independently checked packet result, including output preservation and insufficient capacity. Three isolated mutations for maximum allocation, missing wipe and wrong size fail intended assertions. Camera ownership, cancellation, request review and source-memory lifetimes are unchanged. |
| Secret leakage, custody and authority | Fixtures use constant grayscale markers and published unfunded QR requests. No wallet, recovery key, authentication policy, signing, endpoint or consensus code changes. The complete allocated scratch span is still wiped on every exit. This does not claim erasure of all camera/framework/GPU copies or physical-device qualification. |

All 89 ASan/UBSan/LSan groups pass in 64.98 seconds. Changed production and all
three fixture sources pass strict Clang/GCC analysis; complexity remains within
10/15. The seeded camera fuzzer completes 3231 executions in 121 seconds with
no finding, observing 258 MiB RSS. Its SHA256 is
`624971af0fb7fec6661e868a315d5de56cc47f626b3c83e506e9f9f8adf3a181`.
The JNI fixture linked against the exact release archives passes on x86-64
API 30/35/36, SHA256
`b7ba84a3ee5a716b856e06fcb2d2483b5da5bf2518222b1b86ea458193f82ff8`.
ARM64 compiles only. Its initial harness build exposed the NDK/OpenJDK JNI
table-tag naming difference; a test compile-time tag mapping fixes that build
without changing assertions or production headers.

Android/JVM/build/lint, fixture isolation and 16 KiB native alignment pass.
The locally signed minified APK has SHA256
`ed509f52527502c8278e3e2e303da95cebe129dcf2558eb88f99d3b6359a50fb`;
all unsigned ZIP entries retain their bytes. The full permission-denial/retry/
grant/exact-camera-review/cleanup fixture passes on a fresh isolated API 30
profile in 9.379 seconds. The initial reused profile failed before camera
capture because instrumentation could not find its expected Deny control;
the screenshot still shows the permission dialog. The exact cause is not
established. Its log, screenshot and permission state are retained, and no
assertion or deadline was relaxed. Both owned launches use the qualified reaping
adapter and normal console shutdown. All source, baseline measurements,
mutations, archives and evidence remain in
`.cache/android-wallet/resume-20260915/camera-allocation/`.

## Prepared HMAC state for mnemonic seed derivation — 2026-09-15

Reviewed `secret_hash.c`, its private header and `mnemonic_seed.c`, the HMAC
and provider-failure fixtures, the public seed benchmark, the optional OpenSSL
seed fuzzer and their CMake registration. Key preparation and each digest use
the existing vendored SHA512 provider. Its initialized inner/outer pad contexts
belong to one call; each digest clones those contexts into one working context.
The BIP39 profile remains exactly 2048 HMAC rounds and a 64-byte result. The
one-shot HMAC path shares preparation/digest/cleanup instead of maintaining a
second HMAC implementation. This changes no blockchain or transaction rule.

| Hazard | Review and evidence |
|---|---|
| Buffer overflow/underflow; out-of-bounds access; pointer arithmetic | HMAC/KDF inputs are bounded to 256 key bytes, 512 data bytes and 64 output bytes before preparation. Normalization writes 64 bytes into an initialized 128-byte key block; pad loops cover 128 bytes, digest/XOR/copy operations cover 64. Public mnemonic validation retains the 215-byte text, 128-byte passphrase and 140-byte salt bounds. The private KDF block helper receives the caller-appended counter; the public seed caller still appends big-endian block 1. Canary, all-capacity refusal, published-vector and 63 independent OpenSSL boundary cases cover the exact spans. |
| Integer overflow/underflow; signed/unsigned conversions | All lengths are size_t; iterations are fixed at 2048 and XOR indices at 64. No allocation multiplication or input-controlled iteration count is introduced. Passphrase length is checked before salt-offset additions. Fixture integer casts into OpenSSL are preceded by bounds <=256/508 or <=215/136. Native builds retain conversion/sign warnings as errors. |
| NULL dereferences; uninitialized memory | External spans pass the existing HMAC/public-seed validation before any provider access. Both prepared contexts are initialized before normalization can fail. Working contexts are initialized before cloning; clones are only reached after successful pad preparation. Key, digest and KDF arrays start cleared. Every fallible provider operation is checked before dependent work or output publication. |
| Use-after-free; double-free; leaks; dangling pointers | Prepared contexts never escape the owning C call. One-shot HMAC and KDF free both contexts on every admitted exit; each working/normalization context is freed by its own helper. The test observer admits at most three live SHA512 contexts, rejects duplicate initialization or access to unowned/released contexts, inspects erasure while the real object is alive, and retires its integer identity. It retains no pointer after a potentially faulty owner's frame has ended. Every observed context must close exactly once before the caller returns. |
| Stack usage; allocation limits; resource exhaustion | The core introduces no heap allocation, cache, global key state, recursion, worker or extra KDF round. Measured GCC-O2 authored frames are 704 bytes for the KDF, 480 for preparation/one-shot HMAC, 336 for a digest and 256 for the public seed entry. Prior one-shot/public-seed frames were 288/464 bytes; prepared state trades fixed stack storage for fewer hashes. All authored frames pass the 4096-byte gate. These figures are per-function measurements, not a whole-call-stack or process-RSS claim. The new host fuzzer has max_len=160, five-second cases, a 120-second campaign and a 512 MiB RSS cap. |
| Malformed serialization/input; error handling | Canonical English mnemonic checksum and printable-ASCII passphrase rules remain in the public entry. Invalid pointers, oversize lengths and all output capacities 0..63 refuse atomically. Provider failures are injected during every key-preparation step and all four digest steps in rounds 1, 2, 1024, 2047 and 2048 for both short and long keys. Tests also force a provider to overwrite its context and/or partial digest before failing; caller output and borrowed inputs remain unchanged. Error statuses propagate through existing JNI/receiving-address boundaries. |
| Races; cancellation; ownership | All added state is automatic and call-local. There is no shared prepared-key object or borrowed pointer retained across calls, JNI returns, worker cancellation or Activity transitions. Existing JNI and lifecycle ownership contracts remain in effect; reducing bounded derivation work shortens that operation without adding a new cancellation or authority mechanism. |
| Secret leakage; format strings; cryptographic behavior | Both pad contexts, every working context/digest, normalized key block and current/next/accumulator arrays are wiped on success and failure. Fixtures require one key-block wipe and exact digest/KDF scratch wipe counts, not only a lower bound on total cleanup calls. Omitted wipes and either omitted pad-context cleanup fail their intended assertions. The existing vendored SHA512 primitives and optimization-resistant erasure remain in use; there is no new primitive or changed KDF work factor. Timing fixtures use only published BIP39 vectors and constant diagnostic formats; they print durations, lengths and counts, never seed/key bytes. |

Validation: all 89 ASan/UBSan/LSan groups pass (48.93 seconds), with Clang/GCC
analysis and unchanged production/test complexity caps 10/15. The added
standalone benchmark separately passes its sanitizer execution and analysis;
its source bytes match the measured Android benchmark exactly. The architecture
placement gate passes. OpenSSL independently checks 63 key/salt boundary cases
and 96 complete receive/change derivations; the existing 24 published mnemonic
vectors remain exact. Three unmodified mutation controls pass, then all 13
deliberate cleanup/publication/round/XOR/pad/normalization defects fail intended
assertions. The initial mutation harness used an absent archive path; its
log is retained and the corrected path comes from the canonical linker inputs.

The seed oracle fuzzer completes 7842 cases in 121 seconds without a finding,
observing 403 MiB under its 512 MiB cap. Full core/provider sanitizer instrumentation
remains enabled. Android/JVM builds/tests, debug/release lint, fixture isolation
and 16 KiB alignment pass. Release-linked HMAC, mnemonic and failure fixtures
pass on x86-64 API 30/35/36; ARM64 is compile-only. The public-vector JNI fixture
passes on API 30/35/36 in 0.306/1.200/0.588 seconds. An initial invocation named a
nonexistent instrumentation component; installed metadata established the
correct `org.zclassic.wallet.dev.test` component, and the same test/deadline
then passed. Existing emulator profiles and wallet state were preserved.

Performance acceptance uses the exact before/after release archives on the same
API 30 x86-64 emulator, five 200-call batches after ten warm-ups for each public
vector. Median thread CPU milliseconds per seed change 20.523340→11.640076 for
12 words and 30.425060→11.726748 for 24 words (43.28%/61.46% lower). All computed
seeds are checked inside the timing loop. The packaged/tested archive matches
the benchmarked candidate. The unsigned release APK grows 672 bytes to 612903,
SHA256 `8d88ec9944e4ac00e2ba3222e17f53ac2725e2487680ef6da7128688c5847d4d`.
These are emulator CPU/size measurements, not physical-device, battery,
hardware-custody or TLS acceptance. Exact source/archive/fixture identities,
baseline and candidate samples, failure logs and mutant sources are preserved
in `.cache/android-wallet/resume-20260915/seed-measure/`.

## EC context lifetime regression — 2026-09-15

Reviewed `native/tests/test_ec_lifetime.c` and its host-only CMake registration.
Production `ec_context.c` and the pinned provider remain unchanged. The previous
key-failure fixture accepted removal of context destruction; the new fixture
independently observes the contract rather than assuming that a zeroed free
alone establishes correct provider teardown.

| Hazard | Review and evidence |
|---|---|
| Buffer overflow/underflow; out-of-bounds access; pointer arithmetic | Fixture allocations are checked nonzero and at most 1024 bytes. Poisoning/inspection covers exactly the recorded allocation; provider point/encoding writes use their declared 64/33-byte spans. A 35-byte caller array supplies a 33-byte interior output with two guards. No provider writes through a reported oversized length; SIZE_MAX and short lengths are returned only after the bounded real serialization. |
| Integer overflow/underflow; signed/unsigned conversions | Twelve enum cases and five invalid blinding lengths bound iteration. Counters are size_t and reset per case. The fault-loop cast follows a bounded unsigned increment. Zero, 1024, 1025 and SIZE_MAX context-size cases distinguish admission/refusal without performing allocation arithmetic. Strict conversion warnings pass. |
| NULL dereferences; uninitialized memory | All wrapper arguments are checked before dereference. Fixture source uses provider implementation mode to prevent caller-only nonnull annotations from deleting those assertions. Caller buffers and owner state are initialized; successful allocation is deliberately filled before provider construction. Failed construction and failed serialization may leave dirty bytes which must still be erased. |
| Use-after-free; double-free; leaks; dangling pointers | The wrapper records the sole live allocation until free; construction establishes a live provider handle and destruction retires its integer identity. Destruction must precede full erasure, and erasure must precede free. A test poison after real provider destruction ensures provider cleanup cannot mask omission of the application's wipe. Stack scratch is tracked by integer identity and inspected only through a current live zeroization argument. No dead stack pointer is inspected. A second begin refuses without losing the live owner; cleanup and reuse verify cleared fields without double-destroy/free. |
| Stack usage; allocation limits; resource exhaustion | Fixed arrays and twelve deterministic cases; no recursion, unbounded queue, network operation or test-owned secret heap object. Test allocations are exactly those of the existing bounded context; no extra allocation is introduced into production. Largest measured GCC-O2 fixture frame is 224 bytes, below 4096. Sanitizer and mutation jobs are local and finite. |
| Malformed serialization/input; error handling | Failures cover allocation, zero/oversized provider sizes, dirty failed construction, dirty failed randomization, partial point output and partial encoding. Successful encoding with length 32 or SIZE_MAX also refuses. Provider-stop assertions reject further point/encoding work after an earlier failure. Every refused public call preserves the complete caller output; success matches the published compressed generator exactly. |
| Races; ownership; cancellation | Observation state is confined to one standalone sequential fixture process. No provider overrides, mutable test globals or handles enter the Android library. Existing per-operation owner lifetime, JNI and Activity behavior are unchanged. Device runs upload and remove only each invocation's owned public-test executable. |
| Secret leakage; format strings | Tests use only publicly known scalar one and fixed fixture blinding. Inputs are explicitly cleared after their unchanged-value checks. Diagnostics use constant format strings and assertion source text, never key bytes. The observer requires all live context allocation, owner, point and encoding cleanup; ten deliberate regressions fail intended assertions. No new secret logging or custody authority is introduced. |

Validation: 90 ASan/UBSan/LSan groups pass (49.08 seconds); final focused
sanitizers pass after two additional provider-stop assertions and a serialization
failure retaining a plausible length. Clang/GCC fixture analysis, production and
fixture complexity caps 10/15, strict warnings and architecture placement pass.
The old key-failure executable accepts the preserved missing-destroy mutation;
the new fixture's unmodified control passes and all ten mutants fail at their
intended assertions. Exact release-linked execution passes x86-64 API 30/35/36,
with ARM64 compiled only. The x86 fixture SHA256 is
`b8b815c74147e99077e49d084d2d45aa3b1187d05c9280d4bf82f0eda1c76f2f`.
No production implementation, APK, provider or JNI change is part of this slice.
TLS and hardware-positive custody remain unqualified. Evidence and accepted
source/archive hashes are under
`.cache/android-wallet/resume-20260915/ec-lifetime/`.

## Independent BIP32 differential fuzzing — 2026-09-15

Reviewed the new `bip32_oracle.c/.h`, `fuzz_bip32.c`, deterministic replay/corpus
driver, the two adapted fixed-vector fixtures and host-only CMake registration.
The OpenSSL reference is extracted from the existing receive/change oracle;
master/child byte operations now serve both the existing fixtures and fuzzing.
Production key, provider, custody and consensus implementations are unchanged.

| Hazard | Review and evidence |
|---|---|
| Buffer overflow/underflow; out-of-bounds access; pointer arithmetic | Fuzz input is 1..104 bytes, expanded into one fixed 104-byte array. Separate controls precede 64 seed/parent bytes and 32 blinding bytes. Parent secret/chain offsets are 8/40, blinding starts at 72, and all five four-byte path reads end before offset 24. Seed lengths outside 16..64 refuse before provider reads. Oracle child material is exactly 37 bytes, HMAC output 64, and public output 33. Guarded typed nodes compare all output bytes plus both eight-byte guards. Corpus paths use checked snprintf into 1024 bytes. |
| Integer overflow/underflow; signed/unsigned conversions | All generated counters/lengths are bounded, with explicit SIZE_MAX refusal cases. Path indexes are assembled from four uint32_t byte shifts; the oracle uses explicit serialization bytes independently of the wallet loop. Provider integer lengths are constants 12/32 or checked spans <=64. The scalar boundary fixture adjusts only the final public order byte, without borrow/carry. Strict conversion warnings pass. |
| NULL dereferences; uninitialized memory | Oracle public entry points check pointers and size/profile before dereference; every allocated BN/EC object and provider result is checked. Typed oracle/actual outputs begin with identical sentinels, and all scratch is initialized before use. Fuzzer control bytes independently select NULL seed/parent/output/blinding without constraining the scalar's bytes. Each generated pointer still designates its full fixture allocation even when a deliberately invalid length is supplied. |
| Use-after-free; double-free; leaks; dangling pointers | Each OpenSSL owner has one matching BN_clear_free, BN_free, EC_POINT_clear_free, EC_GROUP_free or BN_CTX_free cleanup path, including allocation/provider refusal. No reference retains a pointer across calls. Candidate nodes and complete parent/seed/blinding snapshots are local values and cleared after comparison. The corpus writer closes its one FILE on short write and normal completion; exclusive mode never truncates existing files. |
| Stack usage; allocation limits; resource exhaustion | Maximum depth is five, scalar arithmetic is fixed to 32-byte values and HMAC input to 37/64 bytes. No recursion or persistent provider context is introduced. The oracle's bounded OpenSSL allocations belong only to host fixtures. GCC-O2 per-function maxima are 880 bytes for the harness and 1104 for the optional path-formatting writer, below 4096; these are not whole-call-stack measurements. Fuzzing uses max_len104, timeout5, a 120-second campaign and RSS512MiB. The ordinary replay is fixed to 860 cases. |
| Malformed serialization/input; error handling | Master lengths 0..66/SIZE_MAX and child parent/argument/blinding refusals compare exact statuses and preserved outputs with an independent reference. Success compares both private key and chain code; final public keys must match OpenSSL and two independent fixture blinding inputs. Zero/order/order+1/all-ones parents refuse; one/order-1 and normal/hardened boundary indexes derive deterministically. No implicit index retry is added. Existing rare invalid-tweak provider tests remain in the default suite. |
| Races; ownership; cancellation | The reference, harness and deterministic replay have no shared mutable wallet state, worker, JNI handle or external endpoint. Borrowed input snapshots must remain unchanged after both implementations run. OpenSSL and new test entry points are registered only inside NOT ANDROID and ZCL_ORACLE; fuzzing additionally requires the existing fully sanitized profile. |
| Secret leakage; format strings; cryptographic behavior | Only published vectors and deterministically generated public fuzz bytes are used. Owned nodes, HMAC state/digests and test seed/blinding copies are explicitly cleared. Constant diagnostics report case counts/status failures without byte values. OpenSSL reference arithmetic is for public host fixtures, with no constant-time custody claim. Fuzzer failures abort the public fixture process; no real key, wallet or production RNG is involved. No new algorithm or primitive is linked into the app. |

Validation: default 90 ASan/UBSan/LSan groups pass in 48.93 seconds. Both host
oracle-only and oracle/fuzz profiles pass the 860-case replay, 17 published
BIP32 paths and 96 receive/change comparisons (4.21/4.65 seconds). The fuzzer
completes 37985 executions in 121 seconds without a finding, observing 264 MiB
under its 512 MiB cap. The initial 852-case corpus and later eight exact scalar
cases are separately recorded; the harness/oracle source and binary did not
change when those replay cases were added. All nine deliberate wallet mutations
fail the exact differential assertion. Two isolated oracle admission-bound
mutations fail specifically in the exact scalar cases. A prior whole-order
mutation already failed on ordinary derivation before reaching those cases;
that broader experiment remains preserved, not claimed as boundary evidence.

Clang/GCC analysis and strict warnings pass on all affected fixture modes;
complexity remains capped at 10/15 (1245 fixture functions, 150 files, none over
15). Architecture, Android/JVM build/test/lint, fixture isolation and 16 KiB
alignment pass. The release APK is byte-identical to the accepted seed milestone,
SHA256 `8d88ec9944e4ac00e2ba3222e17f53ac2725e2487680ef6da7128688c5847d4d`.
No new device custody, physical camera, TLS or transaction-authorization claim.
Source/artifact hashes, failures, mutations, generated public corpus and logs
are preserved in `.cache/android-wallet/resume-20260915/bip32-fuzz/`.
