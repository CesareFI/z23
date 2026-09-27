<!-- Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0 -->

# Semantic manifest v1: format, sensor, and measured value

A semantic manifest is a canonical byte string that records what a C front end
saw in one translation unit (TU): the compiler, target, argv and include search
list; every file read, by content; each include lookup with its negative
probes; repo macros; declarations; record layouts; enum values; and every
function definition, identified by a hash of its tokens. Z23 reads manifests
only. It never links a compiler to do so.

| Piece | Path | Dependency |
|---|---|---|
| Reader, builder, roots, diff, dump | `contexts/commons/modules/vcs/src/semantic_manifest.c`, `semantic_manifest_build.c`, `semantic_manifest_dump.c` | SHA3 only |
| The producer (the "sensor") | `tools/sensors/clang_manifest.c`, `clang_manifest_ast.c` and the compiler-API-free core `clang_manifest_core.c`, `_paths.c`, `_lookup.c`, `_records.c`, `_facts.c`, `_producer.c` | libclang C API; built by `make clang-manifest` |
| Tests | `tests/harness/src/test_semantic_manifest.c`, `tests/fixtures/semantic_manifest/*.bin` | groups `semantic_manifest` (no libclang) and `semantic_sensor` |
| Facts extension and snapshot namespace | `contexts/commons/modules/vcs/src/semantic_manifest_facts.c`, `semantic_namespace.c`, `vcs_path_policy.c` | SHA3, a ZVCS tree object |
| Fuzz harness | `tools/fuzz/fuzz_semantic_manifest.c`, `tests/harness/fuzz_seeds/semantic_manifest/` | libFuzzer; a `FUZZ_TARGETS` entry |
| Windows acceptance | `tests/harness/src/semantic_manifest_windows_acceptance.c`, `semantic_manifest_windows_fixtures.h`, `tests/fixtures/semantic_facts/*.zsm` | row `semantic_manifest` in `platform/modules/platform/tests/windows_acceptance.mk` |

The sensor is an external tool. The link gate lists it as outside the base
toolchain (`tools/lint/check_standalone_tools_link.sh`), and it never enters
z23, z23-dev, the test harness, or core. The exact root is meant to become one
input, the "semantic root", to the v2 action root later. Its conventions match
the v2 action preimage: fixed field order, length-prefixed and domain-tagged
fields, repo-relative paths, and `@sys` spellings for system paths. Ordered
argv is part of the identity, and the environment allowlist is sorted.

## Byte format

```
manifest     = MAGIC || section(1) || ... || section(9)
MAGIC        = "zcl.semantic_manifest.v1"            (24 bytes, no NUL)
section      = u8 tag || u32le payload_len || payload
payload      = u32le record_count || record*
record       = u32le record_len || record_bytes
root         = SHA3-256("zcl.semantic_root.v1" || manifest)
section_root = SHA3-256("zcl.semantic_section_root.v1" || u8 tag || payload)
hint_root    = SHA3-256("zcl.semantic_hint_root.v1"
                        || (u8 tag || section_root(tag))  for tag in 1,3,4,5,6,7,8)
fn_tokens    = SHA3-256("zcl.semantic_fn_tokens.v1" || (u32le len || token)*)
```

Field primitives inside `record_bytes`:

- `text` is a u32le length followed by the bytes, which contain no NUL and are
  at most 1 MiB long.
- `u8`, `u32le`, `u64le` and `i64le` (two's complement) are fixed-width
  numbers; a digest is 32 raw bytes.
- `list<X>` is a u32le count followed by that many X elements.

**Ordering is canonical, not normalized.** Every section is present exactly
once, in tag order. Records inside a section are strictly increasing by
`memcmp` over `record_bytes`, and a record that is a proper prefix of another
sorts first. A duplicate record is therefore refused. Sorted inner lists use
the same rule over each element's encoding. Because the length prefix is
little-endian, the order is the byte order of the encoding, not lexical order
(`"g"` sorts before `"(indirect)"`). A producer sorts the encoded bytes; it
never compares the strings. The builder
(`vcs_semantic_builder_v1_finish`) sorts and deduplicates, then runs the strict
decoder on its own output and refuses anything the decoder would refuse.

**Paths** are repo-relative (`inc/a.h`) <!-- doc-path-ok: illustrative example path, not a real file --> , `"."` (directory fields only), or
`@sys/<absolute path>` for anything outside the checkout. The components `.`
and `..`, empty components, a leading `/` and a leading `@` other than `@sys`
are refused. The producer refuses a path under `$HOME` that lies outside the
checkout, because such a path has no host-independent spelling, and it
refuses to run at all when `$HOME` is unset, relative or does not resolve,
since it could not then tell such a path from a system one. **Argv/env
text** may contain `@root`, meaning the checkout root. A `/` at a path
boundary is refused unless it begins `/zclassic23` or `/zbuild`. A path
boundary is the start of the text, the position after one of the characters
`=,:;"'` or a space, or the position right after a glued option such as `-I`,
`-isystem`, `-iquote`, `-o` or `-MF`
(`vcs_semantic_argv_path_boundary_v1`).

### Manifest field table

| Tag | Section | Record fields, in order | Records per TU |
|---|---|---|---|
| 1 | identity | text compiler; path-or-empty resource_dir; text target; path main_file; list<argv> argv (order kept); list<dir> quote_dirs; list<dir> angled_dirs; list<dir> ignored_dirs; list<env> env where env = text name, u8 set, argv value | exactly 1 |
| 2 | files | path; digest sha3(content); u8 origin (1 main, 2 repo, 3 system) | every file read |
| 3 | lookups | path includer; text spelled; u8 form (1 quoted, 2 angled); u8 kind (1 include, 2 has_include, 3 include_next); path-or-empty hit; u32 hit_slot; u8 miss_evidence; list<u32> present_slots (strictly increasing) | per directive in a repo file |
| 4 | macros | path; text name; u8 function_like; text body (non-comment tokens after the name, one space apart); u8 used_in_main | per repo definition |
| 5 | decls | path; text kind (function, variable, typedef, struct, union, enum); text name; text canonical type; u8 linkage | per repo file-scope declaration |
| 6 | layouts | path; text name; u8 kind (1 struct, 2 union); u64 size; u64 align; list<field> where field = text name, u64 offset_bits, u32 bit_width (0xffffffff when the field is not a bit-field), text type | per repo record definition |
| 7 | enums | path; text enum_name; text constant; i64 value | per repo enumerator |
| 8 | functions | path; text name; u8 linkage; text type; digest fn_tokens; sorted list<text> callees; sorted list<text> macros expanded inside | per repo function definition; key (path, name) unique |
| 9 | spans | path; text name; u32 begin_line; u32 end_line; u64 begin_offset; u64 end_offset | one per function |

Linkage numbers are 0 invalid, 1 none, 2 internal, 3 unique-external and
4 external. An unnamed tag takes the name of the first repo typedef that names
it, or `""` when there is none. The fields of an anonymous struct or union
member are flattened into the enclosing layout at their absolute offsets.
Callees are direct calls to a declared function, by name. Every other call is
recorded as `(indirect)`.

**Lookup slots.** For a quoted lookup, the slots are: slot 0 is the includer's
directory, then the quote dirs, then the angled dirs. For an angled lookup,
the slots are the angled dirs alone. Every slot below `hit_slot` is asserted
absent unless `present_slots` lists it. A miss has an empty hit and
`hit_slot` = slot count. The miss evidence byte records how the absent slots
were established:

- `1 derived_stat`: the hit is the compiler's. The slots come from the
  compiler's own printed search list, and every probe below the hit was
  confirmed absent by `stat`.
- `2 replayed_stat`: a `__has_include` found by a text scan, resolved by
  replaying that search list, with every probe confirmed by `stat`.
- `0 none`: `include_next`, a computed include, or an absolute include. No
  negative claim is made for these.

The compiler never reports probes directly, so no miss here is compiler-exact.
Each miss is derived from exact inputs and is stat-confirmed at sensor time.

**Hint root: a candidate-match hint only.** The hint root leaves out FILES
(content bytes) and SPANS (positions). A comment-only, blank-line or
whitespace edit therefore keeps it equal while the exact root changes. It is
not an identity. System headers enter it only through the identity and
lookups, and a token hash is no proof of equal object code. A hint match may
only nominate a reuse candidate. Acceptance still needs the exact root or a
stronger equivalence check.

### What the libclang sensor does and does not observe

- **Parse.** The sensor parses with the caller's argv and appends `-v
  -Wno-unknown-warning-option -Wno-error`. The appended flags are not part of
  the identity. `-v` makes the front end print its exact search list,
  including ignored nonexistent dirs, which the sensor captures from stderr.
- **Identity argv.** The identity argv drops output-only controls (`-c`,
  `-o X`, `-MD`, `-MMD`, `-MP`, `-MF/-MT/-MQ X`, `-fsyntax-only`) and the
  source path. Any front-end error refuses the manifest.
- **Stripped comments.** `clang_tokenize` keeps comments as tokens. The sensor
  drops them from function token hashes and from macro bodies. The
  `comment_only` seed puts a comment inside a function body and inside a
  macro body to pin this.
- **Not observed:**
  - `#undef`. MACROS lists every repo definition, not only the definitions
    still visible at the end of the TU.
  - Entities in system headers. They are covered only by FILES content hashes
    and lookup hits.
- **Clang-specific spellings.** Canonical type spellings come from clang's
  pretty-printer with anonymous-tag locations off, for example
  `union json_value::(unnamed)`. Everything else in the format is
  compiler-neutral. A future native emitter must reproduce these spellings to
  share bytes, or define its own type grammar under a new MAGIC.
- **Two type grammars, never compared.** `clang_getTypePrettyPrinted` is not
  exported by every libclang image: Apple's `libclang.dylib` (Clang 17)
  declares it in its header and does not export it. `make clang-manifest`
  therefore decides by a link probe, not by `CINDEX_VERSION_MINOR`, and
  builds the sensor with `CM_TYPE_PRETTY_PRINTED=1` or `0`. Without the call,
  types are spelled by `clang_getTypeSpelling` of the canonical type, with
  the `(unnamed at path:line:col)` location cut to `(unnamed)`. On the 3
  hotswap TUs tried (`hotswap_loader.c`, `hotswap_activate.c`,
  `hotswap_islands.c`), the two grammars wrote the same records; the grammar
  still differs in general (sugar, spacing, clang versions). The grammar in
  use is a fixed token (`CM_TYPE_GRAMMAR`) that enters the producer digest as
  its own entry (tag `G`, see "Producer digest"). Two manifests written
  under different grammars therefore never name the same producer, so a
  reader can tell them apart.

## Golden vector

`semantic_manifest` builds this manifest with the builder, pins its length,
root and hint root, and checks that adding every record twice, in a different
order, yields the same bytes. The root was cross-checked with an independent
SHA3 (`openssl dgst -sha3-256` over the domain followed by the bytes).

```
identity "golden-cc 1.0" "" "x86_64-pc-linux-gnu" "src/a.c" ["-std=c23","-I@root/inc"] [] ["inc","@sys/usr/include"] [] ["CPATH"=0:"","C_INCLUDE_PATH"=0:""]
files "inc/a.h" ee66867d36d6568cc6df87baed5d1d11ca8fc3bedf059b377474fff50c8419b2 2
files "src/a.c" a051dcdb2edfd0747256f2cd01abacab47279568195e39e163c796d927db772c 1
lookups "src/a.c" "a.h" 1 1 "inc/a.h" 1 1 []
macros "inc/a.h" "A_MAX" 0 "16" 1
decls "src/a.c" "function" "f" "int (void)" 4
layouts "inc/a.h" "pair" 1 8 4 ["x"@0:4294967295:"int","y"@32:4294967295:"int"]
enums "inc/a.h" "mode" "MODE_ON" -1
functions "src/a.c" "f" 4 "int (void)" 6daa7722bb3b1cda0220425a56ffed8b671753a4cc78e67cf3c468ace5aed041 ["g","(indirect)"] ["A_MAX"]
spans "src/a.c" "f" 2 2 18 46
```

- length: 775 bytes
- root: `7ece87deea7ec79254851b5e02eef5f01ecc49829958ef22514208d026c6bb77`
- hint: `5b3caf953603b4c347bd0423e2d52a079d8b8c8b17d4fb22192a7b40a388baf6`

```
7a636c2e73656d616e7469635f6d616e69666573742e763101ae000000010000
00a60000000d000000676f6c64656e2d636320312e3000000000130000007838
365f36342d70632d6c696e75782d676e75070000007372632f612e6302000000
080000002d7374643d6332330b0000002d4940726f6f742f696e630000000002
00000003000000696e6310000000407379732f7573722f696e636c7564650000
00000200000005000000435041544800000000000e000000435f494e434c5544
455f5041544800000000000264000000020000002c00000007000000696e632f
612e68ee66867d36d6568cc6df87baed5d1d11ca8fc3bedf059b377474fff50c
8419b2022c000000070000007372632f612e63a051dcdb2edfd0747256f2cd01
abacab47279568195e39e163c796d927db772c01033000000001000000280000
00070000007372632f612e6303000000612e68010107000000696e632f612e68
0100000001000000000424000000010000001c00000007000000696e632f612e
6805000000415f4d415800020000003136010533000000010000002b00000007
0000007372632f612e630800000066756e6374696f6e01000000660a00000069
6e742028766f696429040660000000010000005800000007000000696e632f61
2e68040000007061697201080000000000000004000000000000000200000001
000000780000000000000000ffffffff03000000696e74010000007920000000
00000000ffffffff03000000696e74072e000000010000002600000007000000
696e632f612e68040000006d6f6465070000004d4f44455f4f4effffffffffff
ffff086b0000000100000063000000070000007372632f612e63010000006604
0a000000696e742028766f6964296daa7722bb3b1cda0220425a56ffed8b6717
53a4cc78e67cf3c468ace5aed0410200000001000000670a00000028696e6469
72656374290100000005000000415f4d41580930000000010000002800000007
0000007372632f612e630100000066020000000200000012000000000000002e
00000000000000
```

## Tests

Both groups are in `tools/dev/test_group_catalog.def`. Both are listed as
external-input groups in `tests/harness/src/testcache.c`, so the test cache
never serves either one.

- `semantic_manifest` needs no libclang. It covers the golden vector, the
  canonical folding, and 11 refusals: truncation, empty input, magic, section
  order, an absolute path, `..`, a host path in argv, an off-allowlist env
  name, record order, a main-file mismatch, and a trailing byte. It then
  replays every seed below over the checked-in fixture manifests.
  It also leaves seven sections empty and requires `finish()` to succeed
  with zero records in each, reads facts revision 2 back, renders the dump's
  composites exactly, and recomputes the Windows acceptance ledger digest from the
  fixture files on disk (see "Windows acceptance").
- `semantic_sensor` runs the sensor live, and prints a visible SKIP when the
  sensor binary has not been built. It proves:
  - the fixture tree gives byte-identical manifests before and after
    `git commit` and at two different absolute roots, with an absolute
    `-I<root>/...` in argv on both;
  - each seed changes the exact root, changes the hint root (except
    `comment_only`), changes exactly the listed sections, and names the one
    changed function where there is one.

Setting `ZCL_SEMANTIC_FIXTURE_OUT=tests/fixtures/semantic_manifest` refreshes
the fixture manifests and prints the golden vector.

| Seed | Edit | Sections changed (exactly) | Hint root |
|---|---|---|---|
| header_decl | add a prototype to the header | files, decls | changes |
| flag_define | append `-DEXTRA=1` | identity | changes |
| flag_opt | `-O2` → `-O0` | identity | changes |
| flag_reorder | swap `-std=c23` and `-O2` | identity | changes |
| macro_body | `SHAPE_MAX 16` → `17` | files, macros | changes |
| layout_add | add a struct field | files, layouts, spans | changes |
| layout_reorder | swap two struct fields | files, layouts | changes |
| shadow_header | copy of `shape.h` in the earlier `-I` dir | every section but identity (lookups explain the move) | changes |
| fn_body | `+ SHAPE_MAX` → `- SHAPE_MAX` in `helper` | files, functions (only `helper`) | changes |
| comment_only | comments before a function, inside a body, inside a macro body | files, spans | equal |

Run against the sensor before the comment-token fix, `comment_only` fails
with "hint root changed". The group therefore catches the defect it pins.

## Benchmark (2026-09-25, shared development host)

Each TU was run 41 times per cell. The three commands ran interleaved in the
same window and were timed by bash `EPOCHREALTIME` around each process. The
flags were the repo's dev compile flags (`DEV_COMPILE_CFLAGS`, 147 `-I`
dirs):

- (a) sensor startup, parse and manifest emit;
- (b) `clang-20 -fsyntax-only` with the same flags;
- (c) `gcc -c` with the same flags (`-Og -g1`), the repo's normal dev compile
  of the TU.

Host load was 12.08 at the start of the run and 10.87 at the end (1-minute
loadavg). Clang is 20.1.8; gcc is 14.2.0.

| TU (lines) | (a) sensor p50 / p95 ms | (b) clang syntax p50 / p95 ms | (c) gcc -c p50 / p95 ms | a/b p50 | a/c p50 | manifest bytes |
|---|---|---|---|---|---|---|
| `platform/modules/platform/src/disk_space.c` (62) | 42.3 / 52.9 | 35.4 / 42.1 | 20.7 / 27.6 | 1.19 | 2.05 | 16,883 |
| `tools/dev/devloop_watch.c` (2,890) | 113.4 / 171.0 | 79.7 / 99.0 | 191.6 / 234.0 | 1.42 | 0.59 | 215,332 |
| `tools/command/native_dev_command.c` (4,914) | 176.2 / 224.0 | 111.1 / 153.4 | 339.5 / 449.7 | 1.59 | 0.52 | 670,141 |

An earlier run in a quieter window (loadavg 8.01 → 8.30) used the pre-fix
binary, which differs only in skipping comment tokens. It gave p50 values
within 6% of these: sensor 41.0 / 108.6 / 175.2 ms, clang 34.3 / 77.8 /
108.1 ms, gcc 19.6 / 187.4 / 331.1 ms.

**Reading.**

- The sensor costs 1.2–1.6× a stock clang syntax-only parse.
- It costs about 0.5–0.6× the real gcc dev compile of a mid or large TU, and
  2× on a tiny TU, where process and libclang startup dominate.
- It is an extra pass, not a replacement for the compile. So it adds latency
  unless its hint nominates reuse that a later exact check then accepts.

## Structure value: 10 real origin/main commits

The study used the 10 most recent non-merge commits reachable from `1ce8f96209` that
modify at least one existing `.c` file. For each commit, the sensor ran on
every modified `.c` at the parent and at the child: 16 file pairs and
32 manifests, all of which parsed. Each pair was classified from the
record-level diff of the two dumps.

| Category | Meaning | File pairs | Commits (by their widest file) |
|---|---|---|---|
| function_localized | only main-file function records moved (plus content/spans) | 3 | 1 |
| function + file-scope | main-file functions plus main-file file-scope decls (new static functions/vars) | 2 | 2 |
| header fan-out | records owned by an included repo header moved (layout, decl, macro, include set, header inline function) | 10 | 6 |
| non-semantic | hint root equal, exact root differs | 1 | 1 |

- **Localization inside the main file.** 48 of 886 main-file function
  definitions (5.4%) moved across the 16 pairs. Over the 15 pairs that moved
  any function, the mean touched fraction per file was 13.2%. Examples:
  - `tools/command/native_dev_land.c`: 2 of 223 functions (`dl_rebase`,
    `dl_rebase_autoresolve`);
  - `tests/harness/src/test_dev_land.c`: 2 of 182;
  - `tests/harness/src/test_zcode_dev_objects.c`: 1–2 of 77–78.
- **Fan-out is named, not just detected.** In `1ece915b06`, the manifest for
  `tools/command/native_dev_hotswap.c` shows one main-file function changed.
  It also shows the layout of `zcl_hotswap_service_report` in
  `engine/modules/hotswap/include/hotswap/hotswap_service.h` growing from 416
  to 552 bytes: an ABI change that every includer inherits. In `bd8e9f98d1`,
  it names the one new static inline function (its decl, function and
  span records) in
  `package_swarm_priv.h`.
- **Hint root equal while the exact root changed:**
  - 1 of 16 real file pairs (1 of 10 commits). `2a158c1ded` only appended a
    `// platform-ok` comment.
  - 10 of 10 synthetic edits on the three benchmark TUs: a leading comment
    line, a comment inside the first function body, a blank line inside it,
    and a comment in the included `tools/dev/devloop.h`.
  - The `comment_only` fixture seed.

  Before the comment-token fix, the in-body comment edits changed a function
  hash in 2 of 3 TUs; the fixed sensor keeps all of them equal.

## Recommendation

Land the reader, tests and doc, and land the sensor as an optional tool. The
numbers show measurable structure value, and they do not show a latency win.

- **Structure value.** The manifest localizes a real edit to named functions:
  5.4% of main-file definitions moved across 16 real file pairs. It names the
  exact header layout, declaration or macro records that fan out. And it
  separates a comment-only commit from a semantic one, which a content hash
  cannot do.
- **No latency win yet.** The sensor is an additional 42–176 ms p50 pass
  (1.2–1.6× clang `-fsyntax-only`). A latency gain needs a consumer that uses
  a hint-root match to nominate reuse and then accepts only on the exact root
  or a stronger equivalence check. That consumer is not built here.

## Facts extension and its producer

The optional extension `zcl.semantic_facts.v1` adds six sections after SPANS.
They are present as a block or absent. A manifest without them keeps its exact
v1 bytes.

| Tag | Section | Record fields, in order |
|---|---|---|
| 10 | facts | text extension name; digest namespace_root (ZVCS tree hash, or zero); digest producer (or zero); u32 max_records; u64 max_section_bytes; u8 complete |
| 11 | symbols | text canonical id; path; text kind; u8 linkage; u8 defined |
| 12 | refs | text from-id; u8 kind (1 call, 2 address, 3 variable, 4 type, 5 enumerator, 6 macro); text to-id |
| 13 | unknowns | text site id; u8 kind (1 indirect call, 2 inline asm, 3 external call, 4 volatile, 5 atomic, 6 unresolved); text detail; u32 occurrences |
| 14 | probes | path includer; text spelled; u8 form; u8 kind; u8 absence claimed; list of (u32 slot, u8 reason) for every slot whose absence is UNKNOWN |
| 15 | truncated | text section name; u32 records kept; u32 records dropped |

The builder writes FACTS and TRUNCATED itself; a producer never adds to them.

- **Canonical ids** are compiler-neutral: `prefix:name` for external linkage,
  `prefix:path:name` otherwise. Prefixes: `f` function, `v` variable, `t`
  typedef, `s` struct, `u` union, `e` enum, `k` enumerator, `m` macro
  (`m:@builtin:NAME` for a builtin macro, `m:@predefined:NAME` for one defined
  outside any file, such as `-D`).
- **Unknown effects are records, never omissions.** A call through a pointer,
  inline asm, a call of a function declared in no repo file, and each
  volatile or `_Atomic` access is written as an UNKNOWN at its site.
- **Negative lookups need the snapshot owner.** The preprocessor only reports
  hits. A probe slot is proved absent only against the immutable ZVCS tree
  named by `namespace_root` (`vcs_semantic_namespace_v1_absent`, which reloads
  the tree object and re-derives its hash). Without it every claimed slot is
  UNKNOWN, with a reason: no snapshot, outside the checkout, excluded by
  snapshot policy, stale (the snapshot holds the path), not a directory, or
  unresolved.
- **Bounded.** The default caps are 65,536 records and 16 MiB per section.
  A section cut by a cap leaves a TRUNCATED record and `complete = 0`; such a
  manifest is valid but not authoritative.
- **Producer digest.** FACTS names the code that wrote the facts:
  SHA3-256 over `zcl.semantic_producer.v1` and, sorted by file name, the
  producer image's own bytes (the sensor executable, read through
  `os_proc_open_self_exe`, so neither argv[0] nor a wrapper script can choose
  it), the GNU build id of every loaded `libclang*` or `libLLVM*` image
  (the front end), and the type grammar token (entry name `type-grammar`),
  each as
  `u8 tag ('S' bytes, 'B' build id, 'G' type grammar) || u32 name_len || name || u32 id_len || id`
  (`tools/sensors/clang_manifest_producer.c`). Build ids are the linker's
  content hash, so no front end library is re-read per run. An image
  without a build id, a platform without `dl_iterate_phdr`, or no type
  grammar leaves the digest zero: the manifest is valid, but it names no
  producer.
- **Darwin.** On Mach-O the digest hashes the bytes of each image actually
  selected (the sensor and the libclang it loaded): the file and its
  read-only mapped segments, bound to the backing vnode and revalidated
  before and after the parse, so a replaced file cannot be hashed in place
  of the mapped one; the facts emitter refuses rather than write a zero
  digest (see "Darwin producer identity" below). An `LC_UUID` alone is not
  accepted as an image identity: a link can pin or choose it (this
  repository's own Mach-O fixture builds pin one), so it does not bind the
  bytes.

### The producer: the libclang sensor

`build/bin/z23-clang-manifest emit --facts` is the only producer of facts
manifests. It reads the front end through the libclang C API and hands every
observation to a compiler-API-free C23 core (`tools/sensors/clang_manifest_core.h`
and its `.c` siblings), which alone spells paths, probes include slots,
replays `__has_include` and encodes every record.

The sensor is a **second parse**, not part of the compile. Its measured cost
is in the benchmark above: 1.2–1.6× a stock `clang -fsyntax-only` of the same
TU (42–176 ms p50 on the three measured TUs), about 0.5–0.6× the real gcc dev
compile of a mid or large TU, and 2× on a tiny one, where process and
libclang startup dominate. It never replaces the compile, so every manifest
adds that latency. It never changes an object either, because it writes none.

`make clang-facts` runs the sensor over every TU of one component
(`CLANG_FACTS_COMPONENT`, default `engine/modules/hotswap`) with the real dev
compile argv (`$(DEV_COMPILE_CFLAGS)`), writing `build/clang-facts/<src>.zsm`.
`CLANG_FACTS_TREE=<hex>` names the ZVCS tree the namespace probes are proved
against. The target is opt-in: nothing else depends on it, and the sensor
never enters z23, z23-dev, the test harness link or core. A manifest is
rewritten when its source, a header of the component or the sensor changes.
Nothing on main reads these manifests to plan yet (see "Not on main yet: the
consumer").

Known limits:

- An ignored (nonexistent) search dir is recorded in IDENTITY but not
  snapshot-probed.
- The canonical type spellings are clang's (see "What the libclang sensor
  does and does not observe").

**Second entries.** An `alias`, `weakref` or `ifunc` attribute, or an asm
label, gives a body a second entry the facts would otherwise not name. The
sensor records each as an ADDRESS ref to the target: the file-scope
definition of that spelling, or both `f:` and `v:` of the name when none is
visible. An asm label also takes its own definition's address.
A reader therefore sees a changed target as address-taken.

An in-compile clang plugin that wrote the same bytes (except the producer
digest) from inside the running compile was measured on a candidate branch
and is not part of the tree: it is C++ against clang's unstable plugin API,
and Z23 keeps compiled code to C23 over a compiler's C API. Its measured
extraction overhead (about 10 percent of compile CPU, objects byte-identical)
is the bar a future in-compile producer must meet; the sensor's second parse
is what the tree pays today.

### Facts revision 2

Facts revision 2 (`zcl.semantic_facts.v2`) adds the `@scope` and `@cond`
pseudo-sites, which record file-scope and conditional macro uses, and names
an anonymous member's nearest named record. Readers accept both extension
names and report the revision (`vcs_semantic_facts_v1_info`).
`vcs_semantic_manifest_v1_each` hands a reader every record with its exact
encoded bytes, so a reader can digest records without re-encoding them. No
v1 byte changes.

### Darwin producer identity

The Mac lane's commits 12892e0ca1 and cf39ed1765 give the sensor a producer
digest on Mach-O: each selected dyld image's file and its read-only mapped
segments, bound to the backing vnode and revalidated before and after the
parse; the facts emitter reuses that pre-parse digest and refuses rather
than write a zero one. The link probe that builds the sensor without
`clang_getTypePrettyPrinted` (26d38abd92) is the Mac lane's too. On Linux
the records are byte-identical. The `__APPLE__` branches are verified on the
Mac only.

### Fuzzing the reader

`tools/fuzz/fuzz_semantic_manifest.c` (a `FUZZ_TARGETS` entry, built with
libFuzzer under ASan and UBSan) drives every reader entry point:
validate, both roots, `vcs_semantic_section_v1_each`,
`vcs_semantic_manifest_v1_each`, `vcs_semantic_absent_v1_each`,
`vcs_semantic_facts_v1_info` and the dump. It rebuilds each accepted input
from its decoded records and requires byte-identical output and an equal
root, because the canonical form admits exactly one encoding. The dump must
be deterministic and free of NUL bytes, and the facts info must agree with
the sections present. Seeds are under
`tests/harness/fuzz_seeds/semantic_manifest/`; they include facts
revision 1 manifests, which the reader still accepts.

The first seed found undefined behavior: the builder sorted every section,
including an empty one whose record array was still NULL. `finish()` now
skips the sort for an empty list, and a `semantic_manifest` case leaves
seven sections empty and requires each to read back as zero records.

### Windows acceptance

`semantic_manifest` is a row in
`platform/modules/platform/tests/windows_acceptance.mk`. Its program,
`tests/harness/src/semantic_manifest_windows_acceptance.c`, is strict C23
and calls no Windows API. It builds the golden vector and pins its length,
root and hint root. It strict-decodes every fixture under
`tests/fixtures/semantic_manifest/` and `tests/fixtures/semantic_facts/`,
which `tests/harness/src/semantic_manifest_windows_fixtures.h` embeds at
compile time, and folds each fixture's name, root and hint root into one
SHA3 ledger digest. It also checks the decoder's refusals. The
`semantic_manifest` group case `smt_t_ledger` computes the same ledger from
the fixture files on disk and pins the same digest. The header is
generated with `od -An -v -tu1 -w16` per fixture, in `LC_ALL=C ls` order;
regenerate it and both pinned digests whenever a fixture file changes.
`make windows-portability-acceptance` cross-links the program. Running the
`.exe` on Windows is not observed on the Linux host.

## Not on main yet: the consumer

The branch that produced this format also carries a consumer: an optional
`"facts"` input to `dev.change.plan` that narrows the feedback closure from
the before and after manifests of each changed file
(the facts planner, not in this tree), with its falsification set, a
declaration-identity consumer for header changes, differential fuzzing of
that consumer, and measured narrowing on real commits. None of it is on
main. On main, nothing reads these manifests to plan, so `make clang-facts`
and the facts extension change no plan. The consumer's design and evidence
will be documented here when it lands.

## A future native C23 compiler

A native front end emits the same contract by linking the same core and
calling its `cm_*` entry points in the same order: files, identity, lookups,
macro definitions and expansions, declarations, layouts, enums, function
definitions with their token hash and spans, then the facts sites. The core
has no compiler API. What the front end must reproduce is the libclang
sensor's reading of the front end: type sugar normalization, raw token
spelling with comments dropped from token hashes and macro bodies, the
cursor visit order, the preprocessing record's expansion and inclusion
rules, the rebuilt search list, the identity argv, and clang's canonical
type spellings. A different type grammar needs a new MAGIC. Emitting from
inside the compile, as such a front end would, removes the second parse.
