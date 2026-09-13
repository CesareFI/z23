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
