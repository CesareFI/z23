<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 -->

# Native Mach-O component mapping: Mac observations

Source baseline: `259597b6a3840a228824c91d09ede51278a92fb6`.
This is a native arm64 macOS feasibility and falsification checkpoint.
It does not qualify component admission, transport, or activation.

## Compiler and inputs

Apple Clang 17.0.0 (`clang-1700.3.19.1`), target
`arm64-apple-macos14`, SDK 26.0. Compiler executable SHA-256:
`bcac6febe0148c3696bd3f7c42fb5bb475720fee4a1412de56e3b16ea1c04e5f`.

The object survey used C23, `-O2 -Wall -Wextra -Werror -fPIC`, and
`-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector`.
These flags describe the fixtures; they are not a frozen production capsule.
All builds and runs used `devbuild --wait` on the Mac.

The pure function was `int component_behavior(int value)` returning
`value + REVISION`, compiled once with revision 1 and once with revision 2.
The import fixture called `host_callback(values[(unsigned)value & 3u])`,
with static constant values `{11, 23, 47, 89}`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Pure revision 1 | 384 | `08c5860c8afc262b2100f0f2f985797394634e08d2a43c08438c0929ab4a69f6` |
| Pure revision 2 | 384 | `07355277b1a21516efa96e611df39f819856382cba3687ed15e38c1df5b0031f` |
| Callback and constants | 616 | `59523073bb0983b5f43274a7f477a2f50ad83911088ddd72c60e733990d7bb26` |
| Existing module ABI fixture | 1184 | `948c3718e5119e94c1640bc4374859ec7419a795b7c84f4f1b4b39cfb4b3de25` |

The pure objects contain one eight-byte `__TEXT,__text` section and no
relocations. The callback fixture contains 20 bytes of text, 16 bytes of
constants, and three ARM64 relocations: `BRANCH26`, `PAGE21`, `PAGEOFF12`.
Ordinarily linked native executables returned 4, 5, and 178 respectively.
The callback executable passed the system code-signature verification.

## Executable mapping mechanism

A private C23 mapping probe read the pure fixtures, allocated anonymous
read/write memory, copied eight code bytes, flushed the instruction cache,
changed the mapping to read/execute, called it, and unmapped it.
The host executable was built once. Switching function revisions did not
invoke a linker or rebuild that host.

The first measured mapping-and-call durations were 33 microseconds and
21 microseconds. The host allocation granularity was 16 KiB. Five fresh
processes executed both revisions at five different mapping addresses;
observed mapping-and-call durations ranged from 18 to 37 microseconds.
These are individual small-fixture observations, not a fleet benchmark or
evidence of production hardened-runtime compatibility.

The probe executable passed system code-signature verification. This does
not sign the anonymous code mapping or establish hardened-runtime policy.

A copy signed with `codesign --options runtime` passed strict on-disk
verification and reported CodeDirectory flags `0x10002(adhoc,runtime)`.
Running the same mapping probe then ended with SIGKILL (exit 137), without
a completed mapping result. The exact failing syscall was not instrumented.
Its SHA-256 was
`eaff2761a4334c439b92b54d1d8dc6a8fd283fc5f9ffdfadf0e04b6c03e2fb8c`.
Thus the anonymous-code mechanism cannot be assumed to work under the
tested hardened policy. The product must qualify executable staging under
its actual host policy and refuse unsupported policies before execution.

## Component-only bundles

The same pure objects were linked separately with Clang's `-bundle`, without
relinking the host. Each signed bundle was 16,800 bytes and passed strict
on-disk signature verification. SHA-256 values were
`576ea48017ba505f2570ec3f875953c91ab91dc3bf798255795f462a751adfa3`
and `17fd547a32a54f3ecff8489d92bfd5efb149975c34106958419483dc55742b90`.
Each component-only link took approximately 0.12 seconds in the fixture.

One C23 host loaded each bundle with `RTLD_NOW | RTLD_LOCAL`, resolved the
entry, observed behaviors 4 and 5, and unloaded each image. The first
load/resolve/call observations were approximately 291 and 302 milliseconds.
Three later fresh processes using those same bundles observed 0.400 to
0.580 milliseconds. The experiment did not isolate the cause of that
difference; it must not be reported as the binder's cold cost alone.

A hardened-runtime copy of that host returned a structured `dlopen`
refusal: the mapping process and non-platform mapped file had different
Team IDs. The child exited 3, rather than being killed. No entitlements,
certificate trust, or host policy were changed to obtain a pass.

These bundles use the native object format and system loader in private
fixtures. They do not define another transport or settle the shared
component artifact contract. Neither successful load bypassed production
admission: no production admission was exercised in this experiment.

## Falsification and limits

The probe refused a changed CPU type, corrupt magic, truncated header,
and the callback object's unsupported section shape before executing an
entry point. The last refusal was **not** a verified missing-import test:
the probe stopped at the multiple-section shape before inspecting imports.

Independent inspection found that this mechanism probe does not validate
the export table, ABI, compiler capsule, evidence, full-file hash, descriptor
stability, or relocation semantics. It ignores non-segment load commands
and reads at most 4096 bytes. It is not a production loader and must not be
used to accept transported objects.

## Existing ABI changes the minimum binder

A fixture including the existing `hotswap/hotswap_module.h` and emitting
`struct zcl_hotswap_module` ABI v3 produced three sections: text, C strings,
and constant data. Its pointer fields required ten `ARM64_RELOC_UNSIGNED`
relocations. Thus the zero-relocation pure function is insufficient for
the existing module ABI, even without external imports.

That fixture is structural only. Its placeholder leaf and section paths
are not admissible node declarations, and no activation was attempted.

The Mac binder must account for:

- ARM64 `ADRP` page arithmetic uses 4 KiB pages, despite the Mac's observed
  16 KiB allocation pages.
- `BRANCH26` displacement is signed, four-byte aligned, and range bounded.
  Arbitrary host import addresses require bounded veneers or a refusal.
- `PAGEOFF12` requires instruction-specific scaling and pairing checks.
- Section indices are one based; object symbol values require section
  rebasing rather than being interpreted as mapped addresses.
- Existing ABI pointers require checked `UNSIGNED` relocations.
- The callback object also carries `LC_LINKER_OPTIMIZATION_HINT`; handling
  that command must be explicit rather than accepting unknown commands.
- Unsupported relocation forms, arm64e, TLS, initializers, and unqualified
  unwind structures require explicit handling or refusal.
- Text, constants, and writable data need separate final protections;
  all relocation writes and cache flushing precede executable protection.

## Shared contract and remaining acceptance

Linux native mail 4883 requested freezing one shared contract. Mac reply
4884 requested its root, writer/lane, shared paths, and artifact kind.
Mac result 4887 supplied these native observations. At this checkpoint no
frozen shared schema or ownership split had been supplied.

The existing CAS/P2P and fastobj carrier remain the transport authorities.
Carrier admission verifies bytes and does not promote execution evidence.
The existing module ABI's admission, probing, publication, and retirement
boundaries must remain intact.

Remaining acceptance includes the agreed binder and evidence envelope,
real component receipt over existing Z23 P2P/CAS, CPU/ABI and missing-import
refusals, corrupt payload and stale-evidence refusals, relocated address
variation, restart/recovery, changed behavior, and native staging policy.
Ordinary linking and this private mapping probe do not satisfy that journey.

## Retained local evidence

Producer scripts and logs are retained on the development Mac:

- `/tmp/z23-native-component-macho-survey.sh` and its `.log`.
  Compilation and ordinary execution passed; the first inspection used
  a GNU `stat` from PATH and failed. No source failure was hidden.
- `/tmp/z23-native-component-macho-inspect.sh` and its `.log` reran only
  artifact inspection with the native absolute `stat` path; passed.
- `/tmp/z23-native-component-map-probe.sh` and its `.log`; passed.
- `/tmp/z23-native-component-map-negative.sh` and its `.log`; passed.
- `/tmp/z23-native-component-module-abi-survey.sh` and its `.log`; passed.
- `/tmp/z23-native-component-hardened-probe.sh` and its `.log`; signing
  verification passed, but the child was killed with exit 137. The script
  records the child result rather than treating its own zero exit as a
  successful mapping test.
- `/tmp/z23-native-component-bundle-probe.sh` and its `.log`; ordinary
  component loading passed, hardened child refused with exit 3.
- `/tmp/z23-native-component-bundle-repeat.sh` and its `.log`; three fresh
  processes loaded both retained bundles successfully.

The probe C source SHA-256 is
`55a6e43e6a2d2d409d0992860cc920f3d069a4cf7aa84fdc2ae447e50a4c455e`;
its executable SHA-256 is
`713cd35d6a5deaee8d19c053dd43daf07ab4350e7a8122da8746b60c0057fb70`.
The module ABI fixture C source SHA-256 is
`e4f3d35cf2f6d85ccfaba5d694ff5cc15f3d7306d437e1f7363abe868abee507`.
Temporary paths locate local observations and are not published proof roots.

## Follow-up: shared HOT_FORK contract and paired relocations

Linux proposed the shared contract in native mail 4909. Mac acknowledged
the exact bytes in mail 4920 after two independent read-only reviews:
SHA-256 `0f7651af9cbd2ecc4a8542ff8f80fd1adef7c1cb9f9dd3f0041942a372ff8314`,
Git blob `0e723cbc786b1f087389f596fc1dccc1f8d26aef`. Linux's existing lane
retains document ownership. The contract uses existing action-v2 and
HOT_FORK ABI version 1 in disposable children. The earlier ABI3 experiment
above measures the specialized node command adapter, not a generic ABI.

The follow-up fixtures were built from checkout
`259597b6a3840a228824c91d09ede51278a92fb6`; this writer subsequently advanced
the same checkout to `8d22dd265e041a1a09ebe350dc2786cd5f32832f`, preserving
the staged report and previous signed candidate. The machine reports macOS
26.0.1 build 25A362, SDK 26.0, Apple Clang 17.0.0. Components explicitly
target arm64 macOS 14. Ordinary probe hosts used the compiler's native default
deployment floor, macOS 26; they do not qualify a macOS 14 host.

Two private C23 fixtures use the unmodified HOT_FORK descriptor. Their
`run_story` reads volatile input 3, adds the selected revision, and writes
the actual result into its observation: 4 or 5. The descriptor is 72 bytes.
Its owner/story/root strings are explicit placeholders, so this remains
trusted fixture execution, not receiver-admitted work.

| Revision | Object bytes | Object SHA-256 |
| --- | ---: | --- |
| 1 | 1,616 | `d11ddff13659a86e15c95f008a12e0e5a5f287341a93af2d95d6d9628fc2a8fb` |
| 2 | 1,616 | `a26b737fec10487962a9666c5d37d15ced04d99735c2b976aebfe4561964f9df` |

Each component-only linked bundle is 33,392 bytes. One unchanged host
loads both bundles and observes results 4 and 5. Its SHA-256 before and
after is `4e95ea312eb56978c16c7e63f9bf66338e503e08c891ef359b12a4ee174d84bd`.
Bundle signature verification passes. This host checks success and magic;
the reported 1/1 checks remain component-reported.

Each raw object has two PAGE21, two PAGEOFF12 and seven UNSIGNED
relocations. The private raw binder checks adjacent instruction pairs,
matching symbols/base registers, approved ADRP/ADD/LDR-D opcodes, zero
encoded addends, nonoverlapping fixup sites, and logical target bounds.
ADRP uses architectural 4 KiB pages, independently of the host's 16 KiB
allocation pages. Unsupported forms refuse. It hashes the captured buffer
before parsing and maps from that same buffer; expected hashes are fixture
inputs, not authenticated execution evidence.

The raw binder executes both revisions in six fresh processes, checking
the observation's actual result against an independently supplied expected
value. It applies 11 fixups across five 16 KiB allocations. Reported
mapping/binding/protection/call times are 71–95 microseconds, excluding
capture and hashing. Actual mappings vary between an ordinary ASLR range
and `0x7000000000`; requested hints are not guaranteed placements. This
does not measure full-system or peer-to-execution latency.

Its instrumented successor additionally bounds an LDR-D target to eight
logical data bytes. Nine malformed fixtures, each supplied with its correct
SHA3 digest, refuse before mapping: duplicate sites, broken pairs,
unsupported branch relocation, encoded addend, shifted ADD opcode,
truncated literal extent, unresolved import, incompatible CPU and old ABI.
The instrumented host independently checks the result, magic and check counts.

## Follow-up: execution policy

The instrumented hardened copy passes strict disk-signature verification.
Every cache synchronization and `mprotect` step completes. Its last marker
is `phase=before_first_instruction`; it is then killed with signal 9,
exit 137. No first-call completion is observed. This narrows the earlier
failure to entering candidate execution rather than an RX-protection
refusal; it does not identify the exact kernel enforcement cause.

A separate ordinary linker-signed ad-hoc host links the repository's actual
`os_sandbox_stub.c`, applies `os_sandbox_package_leaf_restrict` in a fresh
child, and then executes the raw fixture or signed bundle. No sandbox policy,
entitlement or protection is changed. Each child independently observes
result 4 and exits 0. Before candidate execution, sensitive file reads,
writes, loopback connections and fork attempts return EPERM. Socket creation
itself succeeds, so this is connection-denial evidence, not socket denial.
These canaries are host-executed; candidate-executed denial probes remain
necessary. The fixture parent reports child status; its own zero exit is
not sufficient evidence of child success.

For this ordinary development posture, the captured-buffer raw binder avoids
the bundle's hash-then-pathname-load race. Hardened execution remains
unavailable. Neither path is a qualified production receiver: accepted
load-command payloads, overlapping regions, exports, function extents,
descriptor identities, inherited descriptors, memory/output/deadline bounds,
receipt freshness and P2P delivery still require qualification.

The existing native toolchain command independently reports target
`darwin-arm64` and capsule root
`b3f20040a6be8fd13f63c2100412905ab96cf5bbd72be3ca032c32aa4c5d77ab`.
This is a capture observation, not an action-v2 association or signed receipt.
It reports the process unjoined; no worker or node was activated.

Retained evidence includes `/tmp/z23-mac-hotfork-abi-survey-final.log`,
`/tmp/z23-native-hotfork-rebase.log`, `/tmp/z23-native-hotfork-negative.log`,
`/tmp/z23-native-hotfork-seatbelt-v2.log`, and the toolchain JSON and stderr.
The raw binder's original source SHA-256 is
`a168f22f786aaded823a1c4615e3f61b2122e3d451ae7acad4e47aa04a3fb758`;
the instrumented successor is
`570fb89dd590c987f868dba09c75240ead87c7cda0496faed353d70fd2340e66`.
The Seatbelt successor source is
`dc68513420b67e7539a5cbb7b06c063d6c2ec924fb3bb739a15cfce35f1dd9dd`.
Earlier source versions and failure logs remain intact.

The original documentation candidate's exact integration proof
`50cc736b3556edb7b0bf830705d8cb81ce769de4` against
`8d22dd265e041a1a09ebe350dc2786cd5f32832f` was refused with
`test_accounting_incomplete`: 1,228 groups ran with zero failures, but Tor
bootstrap lacked its required observation inside 90 seconds. The native
gate correctly refused `env_unobserved=1`. This report is not landed or
publication-qualified by those results.

## Follow-up: pre-descriptor object identity

Independent closure review confirmed the existing HOT_FORK producer hashes
the candidate object before generating and compiling a separate descriptor.
The final linked image has a different identity; embedding its own final
hash would introduce a cycle. The native follow-up follows that existing
two-compile pipeline, then performs either a component-only partial link
(`MH_OBJECT`) or bundle link. No host link is performed.

From source `8d22dd265e041a1a09ebe350dc2786cd5f32832f`, the candidate object
SHA-256 values are
`47305e185fed89dc4eff58bc9596008734c3eaf78adba375432c62010145c839` and
`da4a6c633cce1244f1d3f898eb4871db03e5eb462ba72562c5dc9a9bccf8403b`.
Each generated descriptor embeds its respective value. The component-only
partial links are 1,520 bytes; the bundles remain 33,392 bytes. Both bundles
execute the expected changed result in the original unchanged host.
Compiler-driver invocations are recorded; backend and assembler child
process counts remain unobserved.

The raw v2 binder correctly refused these partial links because they add
`LC_DATA_IN_CODE`. Its successor accepts only an empty, bounded table;
checks build-version, symbol-table and linker-hint command shapes; rejects
overlapping file and virtual section ranges; and restricts public exports.
It validates the measured linker hints and executes the original relocated
instructions without applying their optional optimizations. Both partial
objects then execute results 4 and 5 with 11 fixups. The instrumented
297–319 microsecond measurements include diagnostic writes and must not be
compared directly with the earlier uninstrumented 71–95 microseconds.

The successor source SHA-256 is
`1fd069cd4ce4ecbeba2b0053733224232e149538a8c274650f706880615e1296`;
its executable is
`109c91a01e8a57dc88da99f9cadc49ce1a7b8addccf27b32dfdd2620185bfbfa`.
Evidence is retained in `/tmp/z23-native-hotfork-closure-survey.log`,
`/tmp/z23-native-hotfork-closure-bind.log` (the expected refusal), and
`/tmp/z23-native-hotfork-parser-v3-final.log`. Actual source/header/flags,
descriptor compile, partial-link inputs and final outputs still need their
canonical action-v2 producer associations and qualified receipts.

The follow-up local `lint-fast` again refused only the preserved registered
`w-ed` and `w-gui` root checkouts. Other local gates passed. Those checkouts
were not removed and the gate was not weakened.
