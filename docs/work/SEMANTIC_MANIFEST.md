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
| The warm session (`z23-clang-manifest session`) | `tools/sensors/clang_manifest_session.c` and the compiler-API-free checks `clang_manifest_warm.c` | libclang C API; the same binary |
| Tests | `tests/harness/src/test_semantic_manifest.c`, `semantic_sensor_session.c`, `tests/fixtures/semantic_manifest/*.bin` | groups `semantic_manifest` (no libclang) and `semantic_sensor` |
| Facts extension and snapshot namespace | `contexts/commons/modules/vcs/src/semantic_manifest_facts.c`, `semantic_namespace.c`, `vcs_path_policy.c` | SHA3, a ZVCS tree object |
| Fuzz harness | `tools/fuzz/fuzz_semantic_manifest.c`, `tests/harness/fuzz_seeds/semantic_manifest/` | libFuzzer; a `FUZZ_TARGETS` entry |
| Windows acceptance | `tests/harness/src/semantic_manifest_windows_acceptance.c`, `semantic_manifest_windows_fixtures.h`, `tests/fixtures/semantic_facts/*.zsm` | row `semantic_manifest` in `platform/modules/platform/tests/windows_acceptance.mk` |
| Facts-narrowed closure | `tools/dev/devloop_facts.c`, `devloop_facts_text.c`, `devloop_facts_bind.c`, `devloop_facts_json.c` | SHA3, the code index |
| Facts tests | `tests/harness/src/test_semantic_facts.c`, `test_semantic_facts_live.c`, `tests/fixtures/semantic_facts/*.zsm` | groups `semantic_facts` (no clang) and `semantic_facts_live` |

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
  negative claim is made for these. The text scan also writes a `none`
  record (kind `has_include`, no hit) for every conditional lookup it finds
  in a repo file and cannot replay: a `__has_include` whose operand is not a
  literal `"x"` or `<x>` (a macro, say), `__has_include_next`,
  `__has_embed`, the GNU `__has_include__` and `__has_include_next__`
  spellings, an `#embed` (or `%:embed`) directive, and any of these that a
  line continuation runs through. Its spelled name is the occurrence's text
  (for example `__has_include(OPT_HDR)` or `#embed "blob.bin"`). The scan
  reads the file's text after line splicing, not tokens, so a word in a
  comment or a skipped group is recorded too: that costs warm reuse and
  narrowing, never truth.

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
- **Object compiler.** The front end that parses is not the compiler that
  builds the object. `emit --cc CC` names the object's compiler (a path, or
  a name looked up on `PATH` as `execvp` would) and `--toolchain-id HEX`
  the build's toolchain identity (Make's `$(BUILD_COMPILER_ID)`, from
  `tools/dev/build-epoch-key.sh compiler-id`: the bytes of every program
  the driver names for `cc1`, `cc1plus`, `collect2`, `lto1`, `as` and `ld`,
  the linkers, the runtime and startup objects, the include search roots
  and the compile environment). The identity's `compiler` text becomes
  `<front end version>; object-cc <path> sha3-256 <hex> libs <closure>
  toolchain <id>`: the compiler's realpath, spelled like any other path (a
  compiler under `$HOME` outside the checkout is refused), the SHA3-256 of
  its bytes, and `libs none` for a script or static image or `libs sha3-256
  <hex>` over the realpath and bytes of every shared object the dynamic
  loader maps for it (a dynamically linked clang's `libLLVM` and
  `libclang-cpp`). On Linux the loader's trace mode lists those objects;
  an image whose objects cannot be listed (any other format, or a failed
  trace) is unknown. A compile cache is never the compiler: `--cc` that
  resolves to a `ccache`, `sccache` or `zcc` image is resolved through, as
  the cache's masquerade link runs the next program of the link's name on
  `PATH` that is not a cache, and is unknown when there is none. Without
  `--cc` or `--toolchain-id` the text ends `; object-cc unknown`; a `--cc`
  that resolves to no executable file, or a `--toolchain-id` that is not
  64 lowercase hex digits (or is Make's all-zero unfingerprinted value),
  refuses the manifest. The shared objects of the programs the driver runs
  (gcc's `cc1` links `libisl`, `libmpc` and `libgmp`) are bound only as far
  as the toolchain identity binds them, by those programs' bytes. The byte
  format is unchanged (the field was always free text), so v1 readers and
  older manifests, which carry the front end version alone, still decode.
  A compiler upgrade, or a switch between compilers, changes the IDENTITY
  record and so is identity drift to every consumer. A manifest whose
  IDENTITY says `object-cc unknown`, or names no `object-cc` at all (one
  written before the field existed), cannot show that the object's compiler
  did not change: both consumers fall back for it even when both sides
  carry the same text. The TU consumer calls it `identity-drift`; the
  function-level consumer finds its code generation unbounded
  (`inline-closure-unknown`, `no known object compiler`).
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
  under different grammars therefore never name the same producer, and a
  consumer never narrows across them (`producer-changed`).

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
  fixture files on disk (see "Windows acceptance"). It also dry-runs the
  `clang-facts` rule against the dev object rule (see "The producer: the
  libclang sensor").
- `semantic_sensor` runs the sensor live, and prints a visible SKIP when the
  sensor binary has not been built. It proves:
  - the fixture tree gives byte-identical manifests before and after
    `git commit` and at two different absolute roots, with an absolute
    `-I<root>/...` in argv on both;
  - each seed changes the exact root, changes the hint root (except
    `comment_only`), changes exactly the listed sections, and names the one
    changed function where there is one.
  - the warm session writes the bytes a cold process writes after every
    edit of "Warm session", reports the TU reuse the contract requires, and
    with the fault flag never writes a warm manifest the oracle rejected.

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
  manifest is valid but not authoritative, and every consumer falls back.
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
  grammar leaves the digest zero: the manifest is valid, and no consumer
  narrows with it.
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
(`CLANG_FACTS_COMPONENT`, default `engine/modules/hotswap`) with each TU's
own dev object compile, writing `build/clang-facts/<src>.zsm`: `--cc` is the
object's compiler (`$(CC)` without its compile-cache wrapper),
`--toolchain-id` is `$(BUILD_COMPILER_ID)` (omitted, so the compiler is
unknown, when the parse fingerprinted none), and the argv after `--` is the
object's `$(DEV_COMPILE_CFLAGS)` as its target sees it, plus
`$(ZCL_TU_RANDOM_SEED)`. The hot directories' `-O2` reaches the object
and the manifest from one assignment (`DEV_HOT_SRC_DIRS`); the
`semantic_manifest` group, which needs only `make` and so runs where the
sensor is not built, dry-runs both rules and requires equal compiler and
argv for one TU of every `DEV_HOT_SRC_DIRS` directory, every dev object or
object directory the Makefile gives its own `DEV_COMPILE_CFLAGS` or `CC`
(read from `make -p`, which must list every hot directory), and ordinary
samples. The identity TU
(`platform/modules/util/src/clientversion.c`) is refused: its object also
bakes a host-local build receipt that only its own object rule may name, so
a plan that reads it has no manifest and falls back.
`CLANG_FACTS_TREE=<hex>` names the ZVCS tree the namespace probes are proved
against. The target is opt-in: nothing else depends on it, and the sensor
never enters z23, z23-dev, the test harness link or core. A manifest is
rewritten when its source, a header of the component or the sensor changes,
and when its compile does: the rule keeps two stamps, recomputed on every
run and replaced only when their bytes change. `build/clang-facts/<src>.argv`
holds the TU's sensor arguments (compiler, toolchain identity and flags, so
a `ZCL_DEV_HOT_OPT` or `CFLAGS` change re-senses every TU it reaches), and
`build/clang-facts/.object-cc` the object compiler's IDENTITY text
(`z23-clang-manifest object-cc`, so a new compiler, or new shared objects
under an unchanged driver, re-senses every TU). The `semantic_sensor` group
runs the rule twice unchanged (no re-sense), then with another hot
optimizer and another toolchain identity (each re-senses). The stamps are
the re-sense trigger, not a check: the planner does not compare an after
manifest's IDENTITY with the current compile.
The planner binds every after manifest to the tree it plans against (rule 9
below), so a dependency the target misses can only cause a fallback.

Known limits:

- An ignored (nonexistent) search dir is recorded in IDENTITY but not
  snapshot-probed.
- The canonical type spellings are clang's (see "What the libclang sensor
  does and does not observe").

**Second entries.** An `alias`, `weakref` or `ifunc` attribute, or an asm
label, gives a body a second entry the facts would otherwise not name. The
sensor records each as an ADDRESS ref to the target: the file-scope
definition of that spelling, or both `f:` and `v:` of the name when none is
visible. An asm label also takes its own definition's address. The
consumer's `address-taken` rule then refuses to narrow a changed target.

**Attributes that name a function.** `cleanup(f)` on a local, and any
other attribute whose argument names a function, makes the compile call or
pair that function from the declaration's owner where no expression names
it. The sensor tokenizes each attribute libclang leaves unexposed and
records a CALL ref from the owner (the enclosing function, or the declared
function or variable) to every identifier argument that names a file-scope
function, with an external-call UNKNOWN when that function is external. An
identifier argument that names nothing visible is an UNRESOLVED UNKNOWN,
unless the attribute is one whose arguments never name a function
(`format`, `access`, `aligned`, `section` and the like). An attribute clang
does not know is dropped by the front end with no cursor, so the sensor
cannot see it; gcc's `malloc(f)` form is a clang error, so such a TU gets
no manifest and its plan falls back.

An in-compile clang plugin that wrote the same bytes (except the producer
digest) from inside the running compile was measured on a candidate branch
and is not part of the tree: it is C++ against clang's unstable plugin API,
and Z23 keeps compiled code to C23 over a compiler's C API. Its measured
extraction overhead (about 10 percent of compile CPU, objects byte-identical)
is the bar a future in-compile producer must meet; the sensor's second parse
is what the tree pays today.

### Warm session

`build/bin/z23-clang-manifest session [--verify-cold] [--no-warm]
[--max-tus N]` serves a batch of emits from one process. Each stdin line is
one request: the arguments `emit` takes, separated by TAB (a leading `emit`
field is optional). A relative `--root` is resolved against the directory
the session started in. Each request writes its manifest to its `--out`,
exactly as `emit` would, and prints one JSON line on stdout:

```
{"seq","ok","source","out","root","bytes","tu","written","verify","trust",
 "reason","sections","warm_ms","warm_parse_ms","cold_ms","bind_ms"}
```

A refused request prints `{"seq","ok":false,"source","why"}`.

**Protocol.** A line ends at LF, and trailing CRs are dropped. An empty line
ends the session: nothing after it is read. End of input ends it too, after
a last line without an LF is served as a request. Either way the session
prints one summary line (`requests`, `refused`, `warm_written`,
`cold_written`, `verified_equal`, `mismatches`, `evicted`, `created`,
`reparsed`, `recreated`). A request line is refused whole, never truncated
or partly served, when it holds a NUL byte (`request line has a NUL byte`),
is not valid UTF-8 (`request line is not UTF-8`), or is longer than
256 × `PATH_MAX` bytes, 1 MiB on Linux (`request line too long`; the reader
holds one buffer of that size and reads an overlong line to its end without
keeping it). Every reply string is ASCII: quote, backslash, control bytes
and DEL are escaped, and every code point past ASCII is written as a `\uXXXX`
escape (a surrogate pair past U+FFFF), so a JSON decoder returns a source or
out path's exact UTF-8. A byte of a reason that is not UTF-8 (a path the tree
spells so) is written as U+FFFD. `--max-tus` takes digits only, 1 to 256; a
sign, space or trailing byte (`64x`) is a usage error (exit 2). The exit
status is 3 if any request was refused, else 0. There is no daemon, socket
or service: the warm state lives
only as long as the process. The source is
`tools/sensors/clang_manifest_session.c`; the compiler-API-free checks are
in `tools/sensors/clang_manifest_warm.c`.

**What stays warm.** Only the `CXTranslationUnit` and its preamble. The
session parses with `DetailedPreprocessingRecord | PrecompiledPreamble |
CreatePreambleOnFirstParse`, keeps preambles in memory where libclang has
`StorePreamblesInMemory` (not Apple's), and reparses with the main file
handed in as an unsaved buffer. The buffer holds the exact bytes this emit
read and hashed. Every extraction starts from a fresh `cm_state`: no cursor,
source location or file handle survives an emit. The table holds up to
`--max-tus` TUs (default 64, at most 256) and disposes the least recently
used one when it is full.

**When a TU is recreated** (disposed and parsed again), with the `reason`
the reply carries:

| Reason | Trigger |
|---|---|
| `first-parse`, `no-live-tu` | no TU yet, or the last one was dropped |
| `argv-untrackable` | checked on every emit, even with argv unchanged: an argument starts with `@` (a response file) or `-fmodule`, or starts `--config`, `-fimplicit-module-maps`, `-fbuiltin-module-map` or `-fcxx-modules`. No manifest hashes a response or config file, and an implicit module map can pull in headers the file set never names, so such a TU is never reparsed |
| `argv-changed` | the request's argv, and so flags, C mode, target, sysroot or resource dir, differs |
| `producer-changed`, `producer-unnamed` | the producer digest (sensor bytes, libclang build ids, type grammar), recomputed on every emit, moved or cannot be named |
| `file-changed <path>` | a non-main file the accepted manifest read no longer has its SHA3; this also catches an edit that keeps size and mtime, which libclang's own preamble check misses |
| `lookup-unbound <name>` | the accepted manifest has a lookup with no negative claim (`include_next`, computed, absolute, or a conditional lookup the text scan cannot replay: a macro-operand `__has_include`, `__has_include_next`, `__has_embed`, `#embed`), which cannot be re-checked |
| `include-shadow-appeared <dir>/<name>`, `include-shadow-vanished ...` | the shadow candidates changed (below) |
| `warm-unusable: <why>` | a reparse failed a post-check: `main file bytes differ`, `preamble-file-differs <path>`, `identity-moved`, `lookup-moved <name>`, or its own manifest failed the binding checks (`file-changed <path>`, `lookup-unbound <name>`, `include-shadow-appeared ...`, `include-shadow-vanished ...`); the reparse is discarded and retried once as a fresh parse |

**Shadow candidates.** A reused preamble keeps how every include inside it
resolved, including includes made inside system headers, which no LOOKUPS
record covers. libclang revalidates the files it read, not the search slots
it skipped. So a header copied into an earlier `-I` dir under a name a
system header includes left a warm reparse stale, while a cold parse read
the copy (reproduced with glibc bits/wordsize.h under `<stdint.h>`, before the
check existed). The candidates are: for every non-main file of the manifest,
under every search dir (quote, then angled) that holds it, each earlier
search dir that now holds the same relative name, or that no longer exists.
The list is taken right after the parse that builds a preamble, before that
parse's cold check, and every later reparse needs the same list. A header
that appears between the parse and the listing therefore makes the cold
check fail instead of hiding in the baseline. Each dir is read with
`readdir` once per check, and deeper names are checked with `stat`.

**Verified and qualified.** A warm manifest is written only when it is
*verified* or *qualified*:

- *Verified*: a cold parse in the same process (`cm_cold_front`, the same
  code path `emit` runs) produced the byte-identical manifest. The first
  emit after a TU is created or recreated is always verified this way, and
  it writes the cold bytes. `--verify-cold` verifies every emit.
- *Qualified*: a reparse of a TU that has been verified since it was
  created, whose pre-checks passed (producer, argv, file SHA3s, shadow
  candidates, no unbound lookup) and whose manifest passed the post-checks.
  The post-checks are: the main file's FILES digest equals the SHA3 of the
  unsaved buffer; every non-main file both manifests read has the same
  digest; IDENTITY is equal; and every LOOKUPS record whose key (includer,
  spelled, form, kind) the accepted manifest also has is byte-identical. The
  warm extraction probes each slot below a hit with a fresh `stat`, so a
  header newly beside the includer, a slot that went away, or a moved hit
  changes that record. Then the reparse's own manifest is bound exactly as
  the next emit's pre-checks would bind it, because no cold parse checks
  it: every non-main file it read still has the SHA3 it records, it made no
  lookup without a negative claim, and its shadow candidates equal the TU's
  baseline. This catches a file or shadow that moved after the pre-checks
  ran, and anything only this reparse read (an include or a computed
  include added below the preamble).

**Trust in the reply.** Every successful reply carries `"trust"`, the
session's statement of what the written bytes rest on:

| `trust` | `written` | `verify` | The bytes written |
|---|---|---|---|
| `verified` | `cold` | `equal` | the cold front end's, and a warm parse of this same emit produced the identical bytes |
| `cold` | `cold` | `skipped` or `mismatch` | the cold front end's; no warm result matched them this emit (none was tried, it was unusable, or it differed) |
| `qualified` | `warm` | `skipped` | a warm reparse's, used on the pre- and post-checks above alone, with no cold parse this emit |

A `qualified` manifest is **advisory only**. A consumer may use it to narrow
work (an impact plan's closure, which files to look at first), but must
never treat it as equal to a cold emit where the result needs proof, for
example to publish a proof receipt or to accept a reuse: for that, emit cold
or run the session with `--verify-cold`, where every reply is `verified` or
`cold`. The label lives only in the reply line, a signal of this session
run: the manifest bytes and format carry no trust label, and a `.zsm` file
alone does not say how it was produced.

On a mismatch the cold bytes are written, never the warm ones. The reply
reports `"verify":"mismatch"` and the changed sections, stderr says so, and
warm reuse stays off for that TU (`"tu":"none"`, `"reason":"warm-disabled"`)
while the TU keeps its table slot. The disabled flag lives in that slot. When
the table is full, the least recently used slot is evicted, a disabled one
included; a later request for the same source then creates a new TU with
warm reuse on, and like every new TU its first emit is verified against the
cold oracle again. So a disabled TU stays cold for the rest of the session
only while the session serves no more distinct TUs than `--max-tus`.
`--no-warm` makes every emit cold.

**Fault flag.** `ZCL_CLANG_MANIFEST_INJECT_WARM_MISMATCH=1` flips the last
byte of every warm manifest before it is used. It is registered in
`engine/composition/flags.def`, and the `semantic_sensor` group uses it to
prove that the oracle catches a bad warm manifest and never writes it.

**libclang behaviour the session works around.** With a preamble,
`clang_getInclusions` omits files included after the preamble (for example
`hotswap_loader.c`'s late `#include <dlfcn.h>`). It also reports such a file
at depth 0, the main file's depth. The sensor therefore names the main file
by identity (`clang_getFile` of the source), and closes the file table with
`clang_findIncludesInFile` over every file read. Every front-end instance
prints its own `-v` search list block, so the sensor parses the last one.

**Measured** at commit `29d490b7fc`, on the branch this work was ported from
(before main added the attribute and alias refs), on the
development host (28 CPUs under the `devbuild` slot; 1-minute loadavg 11.5
at the start of the three runs and 6.8 at the end). The TUs were the 12 TUs
of `engine/modules/hotswap` with the dev compile argv
(`DEV_COMPILE_CFLAGS`, 147 `-I` dirs), `--facts`. Each run
did the same three rounds over all 12 TUs:

- r1: the tree as committed;
- r2: after a body edit to each `.c` (`(void)0;` before its last closing
  brace);
- r3: after a header edit on top of it (a `#define` in
  `hotswap/hotswap.h`, which 4 of the 12 include).

Cold is one `emit` process per TU. The session is one process for all 36
requests. The three runs agreed within 3 percent, except run 2's cold rounds,
which ran up to 50 percent slower under a load spike; run 3 is shown.

| Round (12 TUs) | cold: 12 processes, wall / CPU | session wall (per request) | session in-process: parse / emit / cold check / pre-checks, sums |
|---|---|---|---|
| r1 first parse | 725 ms / 580 ms | 1,215 ms (101 ms) | 502 / 689 / 453 / 0 ms |
| r2 body edit | 707 ms / 580 ms | 399 ms (33 ms) | 128 / 316 / 0 / 45 ms |
| r3 header edit | 738 ms / 590 ms | 763 ms (64 ms) | 4 recreated: 224 / 322 / 218 / 5 ms; 8 reparsed: 66 / 149 / 0 / 23 ms |

Cold and session wall times are driver wall times per round (bash, one
request at a time). The cold CPU column is user plus system time from
`/usr/bin/time`.

| Whole run | processes | wall | user + sys CPU | max RSS |
|---|---|---|---|---|
| cold, 36 emits | 36 | 2.2 s (sum of rounds) | 1.75 s | 90 MB each |
| session, default | 1 | 2.4 s | 2.3 s | 137 MB |
| session, `--verify-cold` | 1 | 3.1 s | 3.0 s | 140 MB |
| session, `--max-tus 8` (12 TUs cycling) | 1 | 3.7 s | 3.5 s | 127 MB |

**Reading.**

- A qualified reparse after a body edit costs 33 ms per request end to end
  against 60 ms per cold process, 1.8 times faster. About 23 ms of the
  saving is the process itself: start-up, loading libclang, and writing the
  file (a cold emit takes about 38 ms inside the process). About 11 ms is
  the parse: it falls from about 22 ms in a cold emit (derived: the cold
  emit less the extraction) to about 10.7 ms.
  Extraction and the post-checks (about 15.6 ms) do not change. The
  pre-checks cost about 3.7 ms per TU, most of it reading the search dirs
  for the shadow candidates. These runs predate binding a reparse's own
  manifest after it (file SHA3s, unbound lookups, shadow candidates), which
  repeats about that pre-check cost once more per qualified reparse.
- A new or recreated TU costs about 101 ms per request: a warm emit whose
  parse builds the preamble (about 57 ms, 42 ms of it parsing) plus the
  mandatory cold check (about 38 ms). That is about 41 ms more than a cold
  process, so a TU pays back after its second qualified reparse. A header
  edit recreates exactly the TUs that read the header; that round roughly
  matches cold.
- `--verify-cold` is an oracle mode: every emit pays a warm and a cold
  parse, slower than cold alone.
- A table smaller than the working set evicts every TU before its next
  request (`evicted` 28 of 36 here), so every request pays the first-parse
  price, about 1.65 times cold per round. The default of 64 TUs is sized for
  a module-sized batch; a hotswap TU costs about 4 MB resident with its
  preamble in memory.

**Cold oracle.**

- Cold bytes are unchanged by this work. Against a sensor built at the base
  commit (`a0220f0735`), 15 TUs (the 12 hotswap TUs, `disk_space.c`,
  `devloop_watch.c`, `native_dev_command.c`) gave byte-identical plain
  manifests. Their facts manifests differed only in the FACTS producer
  digest, which names the sensor binary.
- `--verify-cold` over those 15 TUs, twice each, gave 30 of 30 equal.
- Every warm manifest written in the three measured runs (qualified,
  verified, and the thrashing table) was compared with a separate cold
  process's manifest of the same round: 180 of 180 per run were
  byte-identical, with 0 mismatches reported.
- The `semantic_sensor` group proves it live on a fixture tree through these
  edits: a body edit, a main-file macro edit, a header macro edit, a header
  edit that keeps size and mtime, a header layout edit, a header shadowing
  in an earlier `-I` dir, a header shadowing beside the includer, a header
  shadowing one a system header includes, a flag change, and a body edit
  after it. Each one checks the reported TU reuse, and each written manifest
  is compared byte for byte with a cold process. The group then runs the
  qualified mode and the fault flag. A second fixture adds a `cleanup(f)`
  local, an `alias("x")` definition and an asm label declared in a header
  inside the preamble; after a body edit, an alias retarget and a label
  edit, each written manifest carries the attribute and alias refs a cold
  process derives, verified and qualified.
- Mutants were checked against the group:
  - dropping the file pre-check is caught (`preamble-file-differs`);
  - dropping the lookup post-check turns the beside-the-includer shadow into
    an oracle mismatch;
  - dropping the shadow candidates fails both the verified case (TU
    reparsed, not recreated) and the qualified case.

**Known limits** (`--verify-cold` covers each):

- Includes that a system header makes by a name no manifest file carries
  are not listed, for example a `__has_include` in a system header that
  found nothing. A quoted include from a system header's own directory is
  not listed either. Both need a write into a system directory or an
  unrelated new name in a search dir.
- A header edited between the pre-check and the reparse, with its size and
  mtime restored, is not seen. The front end would reuse the preamble, and
  the FILES post-check hashes what the front end read.
- A file read through `#embed` inside the preamble is not in FILES, so it
  is not bound.
- The environment is fixed for the life of the process. The IDENTITY env
  allowlist is read from it, and the cold oracle runs under the same
  environment.
- USRs are not used: every identity is still the core's canonical id, so
  the bytes cannot change.
- The Darwin paths (`producer_before`, no `StorePreamblesInMemory`) compile
  in the code but were neither built nor measured on a Mac for this change.
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

## The facts consumer

Two consumers read these manifests to narrow the feedback closure of
`dev.change.plan`: the `.c` path, and the declaration-identity consumer for a
change set that holds a header. Neither makes a plan proof-admissible.

### Facts-narrowed change closure

`dev.change.plan` takes an optional `"facts"` directory. For each changed
file it reads `<facts>/<file>.before.zsm`, `<file>.after.zsm` and
`<file>.before` (the before source); the after source is the working tree.
`tools/dev/devloop_facts.c` then decides, in this order, and the first failed
rule names the fallback to the unchanged file-seeded closure:

0. no changed file is a source compiled into the producer: the sensor's own
   sources and every repo file they include (`producer-source-changed`);
1. every changed file is a `.c` with a manifest pair (`not-c-source`,
   `no-manifest`);
2. each manifest is valid, carries facts, is complete and names its
   producer, and both name the same producer (`invalid-manifest`,
   `no-facts`, `manifest-truncated`, `producer-unknown`,
   `producer-changed`);
3. each manifest's main file is the changed file and its FILES digest is the
   SHA3 of the source bytes given (`source-mismatch`);
4. only files, functions, spans, refs, unknowns and facts differ
   (`identity-changed`, `include-resolution-changed`, `macro-changed`,
   `declaration-changed`, `layout-changed`, `enum-changed`,
   `symbols-changed`, `namespace-probes-changed`). This whole-section rule
   is the `.c` path's only: a change set with a header in it is planned by
   the declaration-identity consumer below, which taints DECLS, LAYOUTS,
   ENUMS and MACROS records per canonical id instead of refusing on any
   section difference;
5. no function was added or removed (`function-added`, `function-removed`);
6. the main file's text outside its function definitions is unchanged after
   collapsing comment and whitespace runs, with literals, splices and
   pp-numbers kept verbatim (`file-scope-changed`); and so is each
   definition's head, the text before its body where storage class,
   `__attribute__`, C23 `[[attributes]]`, return type and declarator live,
   with every whitespace run one space unless the head holds a `#`
   (`function-head-changed`): a constructor, weak or visibility attribute
   changes what the program does without touching the body's tokens;
7. every seed resolves to its FUNCTIONS record (`seed-unresolved`); no
   changed function has an UNKNOWN other than an external call, and none has
   its address taken in either manifest (`unknown-effect`, `address-taken`);
8. refs, unknowns and included files outside the changed functions are
   identical (`facts-changed-outside-seeds`);
9. the after manifest is the parse of the tree the plan runs against
   (`tools/dev/devloop_facts_bind.c`): every other file it read, repo or
   system, still has its SHA3; every include slot it saw absent is still
   absent and every ignored search dir still does not exist, so no header
   created since shadows one it used (`after-stale`); and it has no lookup
   without a negative claim, such as `include_next` or a computed include,
   which could not be re-checked (`lookup-unbound`);
10. the compile identity names an optimizer whose re-emitted code the facts
    can bound (`inline-closure-unknown` otherwise; see "Code generation"
    below), and no function the closure reaches is defined in a header,
    where every reader emits it;
11. every main-file function that expands `__LINE__`, directly or through
    a macro body, and whose lines moved or whose line bytes changed joins
    the seeds; file-scope code that expands `__LINE__`, and any use of
    `__COUNTER__`, is `position-dependent`.

When every rule holds, the seeds are the functions whose FUNCTIONS record
changed, the moved `__LINE__` users, and every main-file function the
code-generation closure adds to them on either side. The reverse-caller walk is the planner's own: `codeindex_callers` by
name, `CI_CLOSURE_DEFAULT_DEPTH` levels, the same proof-owner terminals and
the same group folding, over the index `codeindex_open` keeps current with
the tree. A caller batch that fills or the symbol cap is `closure-bounded`,
which also falls back. A comment-only edit has no seeds and reaches only the
changed file.

**A narrowed plan is feedback only.** Its SEMANTIC dimension is INCOMPLETE
with reason `facts-narrowed`, so `zcl_devloop_plan_proof_admissible` refuses
it. Token or AST similarity never authorizes proof reuse. The plan's
`"facts"` object names the verdict, the reason, the seeds and the number of
files reached.

Positions are bound: by rule 11 on the `.c` path, and by the header path's
position and code-moved rules below. The address of an external function
taken in a TU with no manifest here is caught only through the include
graph: every reader of the header that declares a seed must have a
manifest, and when the graph cannot list the readers (truncated or
unavailable) the plan falls back (`indirect-unknown`, or the universe's
`include-graph-*` reason). A TU without a manifest that takes the address
through its own `extern` declaration, without including the declaring
header, is outside that check; the file-seeded closure has the same blind
spot.

### Code generation

The facts name what the source says, not what the compiler emits. A changed
function can change the bytes of others: a caller that inlines it, an
internal callee that takes its constants, any function that reads a static
variable whose read-only or addressability summary changed. Rule 10 bounds
this by the optimizer the IDENTITY record names
(`tools/dev/devloop_facts_codegen.c`):

| model | when the identity holds | what joins the seeds |
|---|---|---|
| unbounded | `-O2`, `-O3`, `-O4`, `-Os`, `-Oz`, `-Ofast`, `-flto`, `-fwhole-program`, an `-fipa-*` clone, merge or propagation flag, `-finline-small-functions`, `-finline-functions`, or profile feedback, anywhere in the record | nothing: `inline-closure-unknown` |
| callers | no `-O`, or `-O0` | the transitive callers of a changed function (only `always_inline` bodies move) |
| component | `-O1`, `-O`, `-Og` | every function that names a member by call, address or variable reference, and every internal function or internal variable a member names (a header's static too: its reader may fold the value it stores), to a fixed point |

The component model covers inlining and callee summaries flowing up, and
constant propagation, dead-argument elimination and coldness flowing down
into internal callees, as gcc and clang do at `-O1`. It is an assumption
about the compiler, not a fact the manifest records: IDENTITY names the
object compiler's bytes (so a change of compiler is drift), not how much
interprocedural work its `-O1` does, so a compiler whose `-O1` does more
must be added to the unbounded list. The dev build is
`-Og` without `-ffunction-sections`.

### Declaration-identity consumer

A change set that holds a header (or any file that is not a `.c`) is planned
by `tools/dev/devloop_facts_consumer.c` and its parts (`_tu.c`,
`_obligations.c`, `_walk.c`, `_hdr.c`, and the index in
`devloop_facts_index.c`, `_graph.c`, `_codegen.c`). It reads every
`<tu>.after.zsm` under the facts directory (the candidates) with its
`.before.zsm`, and decides each TU from its own pair.

**The index.** One manifest becomes a graph of canonical ids. Every record
of an entity is attached to its id: MACROS to `m:<path>:<name>` and the
name group `m:<name>`, LAYOUTS to `s:`/`u:<path>:<name>`, ENUMS to the
enumerator and its enum, DECLS and FUNCTIONS to `f:`/`v:<name>` or the
path-qualified id, REFS and UNKNOWNS to their site, including the revision-2
pseudo-sites `@scope:<path>` (a macro expanded outside any function) and
`@cond:<path>` (an identifier a conditional directive tests). Edges are
every REFS record, a tag named in a record's canonical type text, a typedef
to its tag, a macro to the name group of every identifier in its body, and
a bare tag id `s:N` (how REFS and SYMBOLS name a tag the sensor gives
external linkage) to the path-qualified `s:<path>:N` its DECLS and LAYOUTS
rows use. Each entity has two digests: one over its records outside the
main file (a header's view of it), one over all of them.

**The universe.** A candidate that read a changed file (in either
manifest) is a member, and so is one whose LOOKUPS hit a changed path or
whose PROBES claim a changed path absent: a `__has_include` of a header
created or deleted reads no file on the side where it is absent, and gcc's
depfiles omit such a probe altogether. The members are cross-checked against the depfile
graph (`codeindex_reverse_includes`): a TU the graph says reads a changed
file but no manifest pair describes is affected (`facts-missing`, or
`include-resolution-change` when it has an after manifest). A graph that
is truncated or unavailable leaves the universe incomplete. A candidate
that read no changed file is checked too: a changed IDENTITY, include
resolution, file set or unrequested file read, or a missing before side,
affects it whole and makes the universe incomplete. A changed file that is
neither C text (`.c`, `.h`) nor prose (`.md`, `docs/`) and that no
manifest read, such as a makefile or a flag file, affects every candidate
(`build-input-changed`). An unreadable candidate manifest leaves the
universe incomplete (`facts-invalid`).

**Each member**, in order; the first rule that fires is its reason:

| reason | affected | broadened | what fired |
|---|---|---|---|
| `truncated`, `producer-unknown`, `producer-mismatch` | yes | yes | the evidence cannot be trusted |
| `identity-drift` | yes | yes | compiler, target, flags or environment changed, or either side's IDENTITY names no known object compiler (`object-cc unknown`, or no `object-cc` at all) |
| `include-resolution-change` | yes | yes | LOOKUPS, PROBES or the file set changed |
| `source-changed` | yes | yes | the TU's own main file changed |
| `position-dependent` | yes | yes | the TU expands `__COUNTER__` on either side: its values count every expansion before them in the TU, so any edit above one may renumber it |
| `macro-unattributed` | yes | yes | a revision-1 manifest cannot attribute a macro change |
| `unrequested-change` | yes | yes | a file it read changed that the request does not name |
| `after-stale` and the rule-9 reasons | yes | yes | the after manifest is not the parse of this tree |
| `macro-conditional`, `header-unattributed`, `position-unknown` | yes | yes | a conditional or other directive of a changed header changed, a changed text names no id the header declares, or the header's positions could not be read |
| `interface`, `macro-conditional`, `header-text` | yes | no | a dirty id (by digest, by `@cond` site, by a changed text chunk naming it) reaches a root: a main-file entity, an `@scope`/`@cond` site, a function or variable a header defines; a root that is not a main-file function broadens the TU, except a function another file defines with internal linkage (a header's `static inline`), which is the TU's own copy and seeds instead |
| `code-moved`, `position` | yes | no | a header function's code moved (a `__LINE__` it expands moves too); a header declaration the debug info records moved |
| `debug-position` | yes, compile only | no | none of the above, while the compile writes debug information (`-g1` and above, or any `-g` spelling it does not know; the last `-g` option decides, but a bare `-g`, `-ggdb` or `-gdwarf-N` keeps a higher level already set) and a file it read changed: clang's DWARF 5 line tables (its default since clang 14) record the MD5 of every file in the TU's file table, so any byte of such a file changes the object, a comment that keeps the line count too; `-g2` and above also record the line and column of every type, function and variable the TU uses, and `-g3` (or `-fdebug-macro`) every `#define`, and `-gembed-source` (with any level, unless `-gno-embed-source` follows) every byte of the file itself. The TU is in the compile set (`compile_only`) and adds no seed and no test obligation. Only a compile whose object compiler is known to write no checksum (gcc) and that embeds no source may narrow this, at `-g2` and above, to a changed file with a token outside comments on a moved or changed line; the identity does not name the object compiler yet, so nothing narrows it. `-g0` or no `-g` never fires it |
| `interface-changed`, `implementation-changed` | yes | yes | the TU's interface or implementation root differs although no reached id is dirty |
| `name-collision` | yes | yes | an id shares its name with a new or removed external id |
| `unaffected` | no | no | no changed id reaches its code |

An id is dirty when its digest differs between the two sides, when a
changed header text chunk names it, or when its whole-record digest differs
(a main-file entity whose records a header change re-expanded).

**Obligations.** A broadened TU contributes its whole file-seeded plan
(`tu-broadened`). For the others, the seeds are the main-file functions a
dirty id reaches, grown by the code-generation closure; a non-main
function in that closure with external linkage (a header definition every
reader emits) broadens the TU instead, and an unbounded optimizer refuses
(`inline-closure-unknown`). A function another file defines with internal
linkage, such as a header's `static inline`, is compiled by each reader as
its own copy under the id `f:<header>:<name>`: it seeds like a main-file
function, so the walk reaches its callers in every reader and a manifest
that takes its address refuses. Such a copy is also a root of its TU even
where nothing there calls it: gcc emits an unreferenced plain `static` at
`-O0`, and a `used` or `constructor` attribute, which the facts do not
record, emits it at any level. The caller walk keeps a caller only when its
manifest names the callee's canonical id. A seed whose address some
manifest takes (`address-taken`), a header-declared seed with a reader
that has no manifest or with readers the graph cannot list
(`indirect-unknown`), a walk that fills its bounds (`closure-bounded`), or
an incomplete universe restores the file-seeded plan; when the
incompleteness is a build input, an outsider's drift or an unreadable
candidate, the plan also carries `closure_universal`, the whole catalog.
The `.c` path applies the same incomplete-universe fallback. Once its rule
chain narrows, it also takes the members' seeds: a TU that `#include`s a
changed `.c` (a unity build, a test reaching its statics) compiles its
functions under the names its own manifest records, perhaps renamed by a
macro. When a member ends broadened, whichever rule broadened it, every
changed `.c` it read on either side seeds it: every function that `.c`
defines there, and every includer function that reaches one, joins the walk
on both sides, whatever the dirty flags marked. The seeding covers every
changed `.c` whose digests differ, not only the first one a rule met. A member
broadened because its own evidence cannot be trusted (`truncated`,
`producer-unknown`, `producer-mismatch`, `facts-missing`) seeds nothing
instead, and when it read a changed `.c` other than its main file the
universe is incomplete. So is it when the depfile graph says a TU reads a
changed `.c` that no manifest pair here records reading. The walk runs again
over every changed file and affected TU whenever a member adds a seed or a
TU other than a changed file is affected (a moved declaration alone changes
the includer's `-g1` bytes), and each broadened member other than a changed
file's own TU adds its file-seeded plan, as on the header path.

**The reply.** `dev.change.plan` with `"facts"` adds a `facts` object:
the verdict (`narrowed`, `reason`, `detail`, `seeds`, `seeds_total`,
`reached_files`), `consumer` (`zcl.semantic_consumer.v1`), `obligations`
(`reason`, `plain`, `plain_universal`, `facts`, and each group with its
reason, up to a byte bound), `universe` (`applied`, `complete`, `reason`,
`detail`, `total`, `affected`, `offset`, `listed`, `next_offset`) and
`tus`, one page of entries from `facts_offset` (at least one per page),
each with its path, the four identities (`source`, `fact`, `interface`,
`implementation`; `action` and `artifact` null with a reason when their
evidence is absent), `affected`, `broadened`, `compile_only` (affected for
its object bytes only: no obligation), `reason` and `detail`.

### Falsification

`semantic_facts` plans each edit below from a fixture pair the sensor wrote
(`tests/fixtures/semantic_facts/*.zsm`, refreshed by
`ZCL_SEMANTIC_FACTS_FIXTURE_OUT`), over a tree holding that edit.
`semantic_facts_live` writes the same pairs with the sensor at test time and
requires the same verdicts. Every fallback must be exactly the file-seeded
plan: the same groups, dimensions and rendered JSON. Only comment and
whitespace edits inside the main file may narrow, and they must leave every
fact unchanged.

| case | edit | facts evidence | verdict |
|---|---|---|---|
| (a) | `struct fx_pair` gains a field in the public header | LAYOUTS changes | `layout-changed`; the header among the changed files, first or last, is `not-c-source` |
| (a) | a header field's type changes, the layout does not | LAYOUTS changes | `layout-changed` |
| (b) | body of `FX_SCALE`, which `fx_helper` expands | MACROS changes; `fx_helper` refs `m:<header>:FX_SCALE` | `macro-changed` |
| (c) | `typedef int fx_num` becomes `long`; the header gains a prototype | DECLS changes | `declaration-changed` |
| (d) | a new `fx.h` beside the source shadows the `-I` one | LOOKUPS changes | `include-resolution-changed`; a stale after manifest (the shadow created after the parse) is `after-stale` |
| (d) | `"fx.h"` becomes `<fx.h>` | LOOKUPS changes | `include-resolution-changed` |
| (d) | a search dir is prepended to argv | IDENTITY changes | `identity-changed` |
| (e) | producer digest forged, zeroed or different before and after; a sensor one byte different | FACTS | `producer-changed`, `producer-unknown` |
| (e) | a sensor source or a file it includes is among the changed files | none needed | `producer-source-changed` |
| (g) | `__attribute__((constructor))`, `((weak))` or `((visibility("hidden")))` on a definition | FILES only; the head text changes | `function-head-changed` |
| (g) | `[[gnu::cold]]` before a definition | the attribute lies before the definition's span | `file-scope-changed` |
| (h) | a seed resolves to no FUNCTIONS record (test fault) | none | `seed-unresolved` |
| (f) | comment in a function, whitespace reflow of a function and its head | FILES and SPANS only | narrowed, seeding only `fx_where`, which expands `__LINE__` below the edit (rule 11) |
| (f) | comment in the public header | FILES only | `facts-changed-outside-seeds`: an included file's digest changed outside the changed functions, so it falls back although no fact changed (conservative) |
| (i) | `fx_other`'s body gains `0 * __COUNTER__` | FUNCTIONS; the body expands `__COUNTER__` | `position-dependent` (rule 11) |

The mutants below were run against `tools/dev/devloop_facts.c` on the
candidate branch, before the head, seed and producer-source rules were
added; each was rejected by the tests:

| mutant | result |
|---|---|
| rule 9 always passes | stale-shadow case narrows; test fails |
| producer compare removed | a different producer narrows; test fails |
| zero-producer check removed | an unnamed producer narrows; test fails |
| section rule removed | the five section-changed cases still fall back, via rule 8; reason assertions fail |
| section rule and rule 8 removed | all six header-side cases (a to d and the header comment) narrow; test fails |
| `.c` check applied to the first changed file only | a header listed after a `.c` still falls back, as `no-manifest`; reason assertion fails |

The section rule and rule 8 are each enough on their own to stop a narrowing
after a header-side edit.

### Falsification of the consumer

`semantic_consumer` runs the consumer over a five-TU fixture tree around one
header (`tests/harness/src/semantic_consumer_fixture.c`), from manifests the
sensor wrote (`tests/fixtures/semantic_consumer/<variant>/`, refreshed by
`ZCL_SEMANTIC_CONSUMER_FIXTURE_OUT` through `semantic_consumer_live`). Each
variant names the affected TUs, each TU's reason, the obligations verdict,
the universe's completeness, whether the whole catalog is in scope, and the
seeds the compile may re-emit. `semantic_consumer_live` compiles every
variant (`-std=c23 -Og -g1`) and fails on any TU whose object changed that
the consumer left unaffected. At the fixture's `-g1` every reader of a changed
header that no other rule reaches is affected compile only (`debug-position`);
a reader that reaches only a bare prototype whose declaration moved is
compile only too (`position`; a moved declaration with a body still folds
in as `code-moved`, fully affected, since its own span carries the code).
The table names the TUs with a test obligation.

| variant | edit | affected TUs | obligations |
|---|---|---|---|
| layout | `struct cx_big` gains a field | its two users (one through `cx_big_t`) | narrowed |
| macro | `CX_CAP` 64 to 65 sizes `cx_big` | the same two | narrowed |
| cond | `CX_MODE`, tested in one TU's `#if` | that TU (`macro-conditional`) | narrowed |
| nested | `CX_BASE`, used only in `CX_SCALE`'s body | the TU expanding `CX_SCALE` | narrowed |
| typedef | `cx_count` becomes `long` | the TU naming it | narrowed |
| tail | a comment after every declaration | none with a test obligation; all five compile only (`debug-position`: each line table records the header's MD5) | narrowed |
| top | a comment before every declaration | the four TUs whose debug positions move (`position`); each reaches only a bare header prototype, so each joins the compile set with no test obligation | narrowed |
| signature | `cx_sum`'s parameter type, header and definer | the definer and its callers | narrowed |
| static | a static's body; another TU has a same-name static | its TU; seeds the static and its caller | narrowed |
| address | a body whose address another TU takes | its TU | `address-taken` |
| shadowed | a byte-identical header beside the TUs | all five (`include-resolution-change`) | narrowed, every TU broadened (the include graph keeps the quoted include no depfile lists as an edge, so it is complete) |
| drift | a flag every TU compiles with, and a tail comment | all five (`identity-drift`) | narrowed, every TU broadened |
| local | `CX_PAD` sizes a struct named only inside `cx_sum` | the definer; seeds `cx_sum` | narrowed |
| makefile | a makefile no compile records reading | all five (`build-input-changed`) | `build-input-changed`, whole catalog |
| tool | the makefile, and every compile gains a flag | all five (`identity-drift`) | `identity-drift`, whole catalog |
| body | `cx_sum`'s body, declared in the header, alone | the definer; seeds `cx_sum` | narrowed |
| counter | a header function gains a `__COUNTER__` (against a tree where one TU already expands it) | all five (`position-dependent`) | narrowed, every TU broadened |
| hstatic | a TU stores 6, not 5, in a header's static (against a tree that already has it) | its TU; seeds the setter and the reader | narrowed |
| alias | `cx_sum`'s body, which an `alias` names too (against a tree with the alias) | the definer | `address-taken` |
| hasinc | a header only a `__has_include` probes is created | all five (`include-resolution-change`) | narrowed, every TU broadened |
| hasdel | the same header deleted again (planned against hasinc) | all five | `include-graph-truncated` (the include graph cannot list a deleted input's readers) |
| cleanup | the body of a `cleanup` handler another function's local names (against a tree with both) | the definer; seeds the handler and the function that runs it | narrowed |
| unity | `cx_sum`'s body, which another TU compiles by `#include "cx_c.c"` under `#define cx_sum cx_e_sum` (and `cx_hook`, `cx_get`, `cx_tail` as `cx_e_hook`, `cx_e_get`, `cx_e_tail`, so nothing is defined twice) | the definer, and the includer (`header-unattributed`) | narrowed; seeds `cx_sum`, `cx_e_sum` and `cx_use_e` |
| unity_move | a comment line moves `cx_tail`'s declaration below `cx_get` in the included `cx_c.c` | the definer, and the includer (`position`); the plan reaches both | narrowed |
| unity2 | `cx_sum`'s body, and a line that moves `cx_sum2` (which returns `__LINE__`, renamed `cx_e_sum2` in the includer) | the definer, and the includer (`header-unattributed`) | narrowed; seeds `cx_sum2`, `cx_e_sum2` and `cx_e_sum` |
| ctr_unity | `cx_sum`'s body, included by a TU that also expands `__COUNTER__` | the definer, and the includer (`position-dependent`, broadened before any text rule) | narrowed; seeds `cx_sum` and `cx_e_sum` |
| unity_ab | the bodies of `cx_sum` and `cx_top_a`, both `.c` files included by one TU under renames | `cx_a.c`, `cx_c.c`, and the includer (`header-unattributed`) | narrowed; seeds `cx_e_sum` and `cx_e_top_a`: every changed `.c`, not the first |
| unity_addr | `cx_sum`'s body, while `cx_d.c` takes the address of `cx_e_hook`, which only the includer defines from `cx_c.c` | the definer, and the includer (`header-unattributed`) | falls back `address-taken` |
| unity_trunc | as `unity`, with the includer's after manifest cut by a record cap | the definer, and the includer (`truncated`) | falls back, universe incomplete `truncated` |
| unity_nobefore | as `unity`, with the includer's before manifest withheld | the definer, and the includer (`facts-missing`) | falls back, universe incomplete `facts-missing` |
| unity_nofacts | the static `cx_twice` in `cx_a.c`, which an includer with no manifest compiles | `cx_a.c`, and the includer (`facts-missing`, from the depfile graph) | falls back, universe incomplete `facts-missing` |
| unity_add | `cx_sum`'s body, and `cx_e.c` gains its `#include "cx_c.c"` (both requested) | the definer, and the includer (`include-resolution-change`) | falls back `include-resolution-changed` |
| hinl | the body of the header's `static inline cx_inl`, which `cx_b.c`'s `cx_inl_b` calls (against a tree with both), at `-O1` | all five: every reader defines its own copy (`interface`) | narrowed; seeds `cx_inl` and `cx_inl_b` |
| hinl0 | the same at `-O0`, where `cx_b.c` emits `cx_inl` out of line | all five (`interface`) | narrowed; seeds `cx_inl` and `cx_inl_b` |
| hinl_addr | the same, while `cx_d.c` returns `cx_inl`'s address | all five (`interface`) | `address-taken` |
| gline | a comment line above every declaration of the header, compiled `-g`, while `cx_e.c` names only `struct cx_small` (against a tree with that use) | the four TUs whose named declarations moved (`position`, compile only: each reaches only a bare prototype), and `cx_e.c` (`debug-position`, compile only, not reached by the walk) | narrowed |
| gline0 | the same at `-g0` | the four (`position`, compile only); `cx_e.c` unaffected | narrowed |
| lineinl | a comment line above the header's `static inline cx_lineinl`, which expands `__LINE__` (`cx_b.c` calls it) | all five: every reader's own copy's span overlaps the moved region (`code-moved`; its token hash reads the same, since it hashes `__LINE__`'s spelling, not its expansion, so span is what catches this) | narrowed; seeds `cx_lineinl` and `cx_lineinl_b` |

The seven from `counter` to `unity` are minimized reproducers of dependencies
a differential comparison against cold clang objects found missed;
`unity_move` and `unity2` come from the review of the `unity` fix, and
`ctr_unity`, `unity_ab` and `unity_addr` from its re-review: without the
includer seeding the first two miss `cx_e_sum` and `cx_e_top_a`, and
`unity_addr` narrows with no obligation. `unity_trunc`, `unity_nobefore`
and `unity_nofacts` come from the final review: each narrowed before the
incompleteness rule above. `hinl0` is the fuzz family F12
(`F12_header_static_inline_O0`): before the reader's own copy seeded, the
consumer left `cx_inl` out of the seeds and every reader but `cx_b.c`
unaffected, and `hinl_addr` narrowed with no obligation. `gline` is the
replay of 45fb85e113, where two lines above a header struct moved its
`DW_AT_decl_line` in a reader the consumer called unaffected. A narrowed
plan must reach every changed file and every affected TU but a
compile-only one, which the fold must not name (the walk may still reach it from a seed another TU adds), checked as sets through test hooks on the files the fold named and
the files the walk reached. A count check would not do: with a fold that
drops the affected TUs, `unity2` reaches 3 files where 2 are needed and
`unity_ab` 3 where 3 are needed, yet both miss `cx_e.c`, and the set check
fails them. Each is planned against its own before tree, a `p_` variant the
fixture produces but does not judge.

A `.c`-only change whose seed a header declares falls back when the depfile
graph is absent and a reader's facts are withheld (the review finding
behind b4f2b17ddd; the test failed before that change).

Each mutant drops one rule and must leave a table-affected TU unaffected,
a required seed out or the whole catalog out of scope; the counts are over
every variant:

| mutant | rule dropped | table-affected TUs or seeds missed |
|---|---|---|
| `NO_TYPE_CLOSURE` | typedef and tag edges | 2 |
| `NO_MACRO_CLOSURE` | macro-body edges | 1 |
| `NO_POSITION` | header positions, and moved `__LINE__` users on the `.c` path | 6 |
| `NO_TAG_ALIAS` | `s:N` no longer reaches `s:<path>:N` | 2 |
| `NO_CODEGEN_CLOSURE` | seeds are the changed functions only | 3 |
| `NO_OUTSIDER` | TUs that read no changed file never drift; build inputs and probed paths ignored | 22 |

The `__COUNTER__` member rule has no mutant switch; with it disabled by
hand, the counter variant misses all five TUs and the test fails.

`semantic_facts` holds the `.c` path's rule 11: `fx_where` expands
`__LINE__` through `FX_WHERE`, and the comment and whitespace edits above it
must seed it.

**A behavior mutant on real code.** An independent run on the canonical
test runner (tree frozen at a0220f0735) flipped `memcmp(...) == 0` to
`!= 0` in the static `has_suffix` of
`engine/modules/hotswap/src/hotswap_loader.c`. It turned
`test_hotswap_loader`, `test_hotswap_rollback` (`test_hotswap_rollback.c:365`)
and `test_os_sandbox_hotswap_interaction` red. The narrowed plan the
consumer gave for an edit of that static before the code-generation closure
(dc5ad717fa; 22 groups down to 5, path groups only) left out the last two:
a real false negative. With the closure (rule 10 and the consumer's seeds),
every function the compile may re-emit with the static joins the seeds,
among them `hotswap_dump_state_json`, whose address a manifest takes, and
the plan falls back (`address-taken`, 22 groups of 22), which selects all
three. The rule this motivates: a seed is every function whose code the
compile may change, not the function whose source changed, and any such
function whose address is taken refuses the narrowing. The fixture's
`static` and `address` variants and the `NO_CODEGEN_CLOSURE` mutant hold it.

The same run saw gcc `-O1` fold a benign `has_suffix` edit to an identical
object. Which TUs rebuild is decided by object bytes, not by source: a
source-level seed can leave its object unchanged, and the witness below
counts a TU as changed only when its object bytes differ.

### Differential fuzzing: `semantic_facts_fuzz`

`semantic_facts_fuzz` holds the whole chain (sensor, facts, `.c` path,
consumer, closure) to a cold compiler. A case is a small C23 project of
three to eight TUs and one to four headers before and after one edit:

- **generated** (`tests/harness/src/semantic_fuzz_gen.c`,
  `semantic_fuzz_templ.c`): 42 function templates (inline, macro,
  `_Generic`, layout, enum, weak, constructor, `__LINE__`, `__COUNTER__`,
  `__has_include`, alias, cleanup, header-static, `constexpr` and more)
  under one of seventeen mutation kinds (plus the data and path kinds
  below), the same bytes for the same seed
  and profile. The `no-ctr-line` profile drops `__COUNTER__` and
  `__LINE__`, whose users make most plans `position-dependent`, so its
  plans narrow; `gcc-deps` writes the depfiles with gcc, which omits
  `__has_include` probes;
- **fixed** (`tests/harness/src/semantic_fuzz_repro.c`): every minimized
  reproducer a run found (F1 to F14) and the shapes that must keep
  passing. They run on every run.

Each TU is compiled with the clang of the LLVM whose libclang the sensor
links (the sensor's `DT_RUNPATH`), `-std=c23 -O1 -ffunction-sections
-fdata-sections` plus the project's flags, and sensed with the same argv,
on both sides, unless a toolchain case says otherwise (below). The change
set is planned in process exactly as
`dev.change.plan` with `"facts"` plans it, and two oracles judge the plan:

1. every TU the plan leaves unaffected, or out of its universe, has a
   byte-identical object;
2. in a changed object, every function and every data object
   (`STT_OBJECT`) whose bytes changed, or whose relocations now address
   other content, is a seed of the narrowed plan, or is covered by a
   fallback, or by a broadened TU after a header change; and every other
   symbol at a seed's address (an alias) is a seed too. The symbols,
   sections and RELA entries are read from the ELF tables. A relocation
   against a section or local symbol is resolved to what it addresses,
   never to the section's name: the NUL-terminated string in a merged
   string section, the constant in a merged constant section, the object
   whose bytes hold the addressed offset (its bytes and the position
   inside it), the function that holds it (by name), else the section
   bytes from there to the next symbol or the section end. A change only
   in a relocation's addend or target symbol counts as benign when every
   such relocation resolves to the same content on both sides; an addend
   into an undefined or global symbol is never benign. A data object is
   covered by a variable seed (`v:...`), or, for a function's static
   (`t0_kk.a`), by its function's seed. A
   seed covers a function by canonical id, not by bare name: `f:<name>`
   an external symbol, `f:<path>:<name>` a local one whose path is the TU
   itself or a header (a header static), so a same-name static in another
   file is not covered by it.

fails the group. Every fixed reproducer always runs. On top of those, the
default run adds a fixed list of 6 (seed, profile, kind) cases (about 45s
standalone with the fixed reproducers at SFZ_CASES_AT_ONCE), and must yield narrowed
verdicts that seed a changed function, so the group cannot pass on
fallbacks alone; `ZCL_STRESS_TESTS=1` runs the full 51-case list instead,
held to the same invariant.
`ZCL_SEMANTIC_FUZZ_SEEDS=FIRST:COUNT[:PROFILE[:KIND[:CC[:OPT]]]]` runs a
long range instead of either, and wins over `ZCL_STRESS_TESTS`; only such
a range run may draw an edit that changes no file (NOOP), which fails a
fixed reproducer or a default seed. `KIND` `any` draws the kind. `CC`
names the object compiler of both sides (`clang`, the sensor's, by
default; `gcc` or any name on `PATH`), or `BEFORE>AFTER` for a compiler
that changes between the sides; `OPT` replaces `-O1` with comma-separated
flags in both argvs, or is `COMPILE/SENSOR` when the sensor is handed
other flags than the compile. The sensor is told each side's actual
object compiler with `--cc` and a fixed stand-in toolchain identity with
`--toolchain-id`, as the `clang-facts` rule tells it. The toolchain
reproducers (`k_sfz_tool_repros`) set the same fields. A reproducer marked
known-RED names the fix it waits for and the exact false-negative lines it
reports until then; it holds only when it fails with exactly those lines,
and any other outcome (an ERROR, a different miss, or a PASS, which means
the mark is stale) fails the group.
F7 (a `cleanup()` handler inlined into a function that is not seeded),
F7_cleanup_same_name (the same miss while another file's same-name static
also changes, which a seed matched by bare name would have hidden) and
both F8 shapes (a `.c` that `#include`s another `.c`: an edit to the
included file does not seed the includer's functions) were known-RED at
64370952d5; the consumer's cleanup-handler and `.c`-includes-`.c` fixes
now cover all four, and they run as ordinary fixed reproducers.
F9 was known-RED until the consumer sensed each TU with its own object's
compiler and flags (ff12233071): the IDENTITY record now names the object
compiler's realpath and byte SHA3-256, and it now must pass. F11 forces
the sensor to a different optimizer level than the compile (OPT
`COMPILE/SENSOR`) regardless of what the real Makefile rule passes, so
ff12233071's fix to that rule's own drift (which F9 exercises) leaves F11
red: measured, it still misses exactly `t0_q` and `t0_eq`.

The known-RED reproducers are four families: two toolchain families (no
text edit the consumer misreads), one optimizer level, and one file
that only a preprocessor probe names:

| reproducer | toolchain | missed | root cause |
|---|---|---|---|
| F10_opt_spelling_O02, _long | gcc at `-O02`, `--optimize=2` | `t0_s.constprop.0` | `fxg_optimizes` (`tools/dev/devloop_facts_codegen.c`) reads `-O0` as a prefix and needs `-O`: gcc's `-O2` is modeled as `-O0` or no optimizer (callers only) |
| F10_opt_spelling_O5 | gcc at `-O5` (gcc's `-O3`) | `t0_w.constprop.1` | `k_fxg_unbounded` lists `-O2` to `-O4` only; `-O5` falls to the `-O1` component model, which never seeds an external callee gcc clones |
| F11_hot_icf | gcc `-O2` objects, sensor told `-Og` | `t0_q` (an alias of `t0_p` after ipa-icf), `t0_eq` | a sensor deliberately handed other optimizer flags than the compile (`OPT` `COMPILE/SENSOR`) models the wrong codegen no matter what the real Makefile rule passes; `ff12233071` fixed the real rule's own drift (`F9_cc_drift`), not a sensor forced away from it |
| F12_header_static_inline_O0, _gcc | clang or gcc at `-O0`; a header `static inline` body changes | `h_inl` (each reader's out-of-line copy) | `fxc_seed_marked` (`tools/dev/devloop_facts_tu.c`) seeds only main-file functions and broadens only on a root, and `fxi_on_function` (`tools/dev/devloop_facts_index.c`) makes a header definition a root only with external linkage; at `-O1` the call is inlined and the caller seed covers it, at `-O0` (the callers model) the reader's own copy changes unseeded |
| F13_has_embed_deleted | sensor's clang; the case file docs/banner.txt, which only `__has_embed` probes, is deleted | `t0.c` (object changed, out of the universe) | `cm_scan_has_include` (`tools/sensors/clang_manifest_lookup.c`) scans only `__has_include` with a literal operand and nothing scans `__has_embed`, so no manifest names the probed path; clang's depfile names a probed file only while it exists; the include graph refuses the deleted path (`include_input_missing`, `codeindex_impact.c`), `fxc_cross_check` (`tools/dev/devloop_facts_consumer.c`) marks the plan incomplete but, unlike `fxc_build_inputs`, puts no candidate in scope, and the file-seeded fallback has no TU for a `.h`, `docs/` or `.md` path |
| F14_has_include_macro, _next | sensor's clang; the case header inc2/cfg_local.h, probed as `__has_include(CFG_LOCAL)` or with `__has_include_next`, is deleted | `t0.c` (as F13) | as F13: the text scan skips a macro operand and `__has_include_next` |
| F15_has_include_macro_gcc_deps, _next_gcc_deps | sensor's clang, gcc depfiles (as the dev compile's); the case header inc2/cfg_local.h, probed as in F14, is created | `t0.c` (out of the universe of a narrowed plan), `t0_local` | as F14 for the sensor; gcc's depfile omits `__has_include` probes, so the created path, a regular file, has no reader and the include graph answers that as complete: the plan narrows past `t0.c` |

The passing shapes beside them hold the model where the spelling is
canonical: gcc `-O1` seeds the specialized static, and gcc `-O2` and
`-O3` fall back (`inline-closure-unknown`).

The decoy reproducers (`pass_opt_decoy_*`) guard the optimizer parse
against spellings of a level that are no optimizer flag: gcc at `-O2`
followed by `-DMODE=-O0` or `-Ix-O0`, or with `CFLAGS_EXTRA`
`-DLVL=-O0` or `-Wl,-O0` last in the argv (and clang with the
`-DLVL=-O0` one), gcc at `-O0` followed by `-Xlinker -O1`, and the level
flipping `-O0` to `-O2` and back between the sides while the
`-DLVL=-O0` decoy stays last and unchanged (`OPT` `BEFORE>AFTER`). A
reader that took the last `-O` spelling for the level would model the
`-O2` compiles as `-O0` and miss `t0_s.constprop.0`, as
F10_opt_spelling_O02 does; each must pass.

Measured at a88daa4ffd (2026-09-28), 2,250 generated cases over the new
dimensions, each run also repeating the fixed set:

| range | cases | generated FAIL | family |
|---|---|---|---|
| gcc `-O1` (no-ctr-line) | 500 | 0 | none |
| gcc `-Og` | 400 | 0 | none |
| gcc `-O2` objects, sensor `-Og` | 300 | 0 | none drawn (F11 is hand-made) |
| clang `-O0` | 250 | 18 | F12 |
| gcc `-O0` | 250 | 25 | F12 |
| gcc `-O1` (gcc-deps) | 200 | 0 | none |
| gcc `-O1,-g` (all) | 150 | 0 | none |
| clang to gcc drift | 100 | 80 | F9 |
| clang `-O3` | 100 | 0 | none (falls back) |

Four generator kinds edit only data: `data_string` (a string literal in
a body, of the same length or longer, so a later literal moves),
`data_table` (an entry of a file-scope static const table read at a
runtime index), `data_hconst` (an entry of a header static const table
that several TUs read) and `data_index` (a header index macro into a
global array another TU defines). They render a data layer only in their
own cases, so every other kind's project stays byte-identical for its
seed, and an unforced draw picks one of them one time in five. The
reproducers D1 to D6 pin the same shapes plus a header macro that
initializes a global const and a static const table inside a function.

Seven path kinds change what a path resolves to while the includer's
text stays the same. They too render their layer only in their own
cases, and a range runs one only when it names it (never drawn):
`symlink_retarget` (a header that is a symbolic link names another
body), `file_macro` (a header returning `__FILE__` and `__FILE_NAME__`
moves to the other include directory as a file or a link, or gains a
same-bytes copy that shadows it), `pragma_alias` (a `#pragma once` header
reached through a second path, a link that becomes a same-bytes file or
names a same-bytes copy, so the header is read twice, or the reverse),
`macro_include` (`#include SEL_HDR` names the other header), `embed_data`
(a byte or the length of a file `#embed` reads), `hasembed` (a file
`__has_embed` probes is created or deleted) and `hasinc_macro` (a header
probed through a macro operand or `__has_include_next` is created or
deleted). A case lays a link out as a link and compares it as git does,
by its target text, so an edit to the file a link names changes only
that file's path. The sensor records the path a link resolves to, so a
TU that reads a header through a link is a reader of its target; a
changed link is no regular file to the include graph, so a retarget
falls back (`include-graph-truncated`). In 64 no-ctr-line seeds of each,
only `hasembed` (26) and `hasinc_macro` (10) missed, every miss the
deletion of a file only a probe names (F13, F14); with gcc depfiles
(48 gcc-deps seeds) `hasinc_macro` also missed 12 creations, under
narrowed plans (F15). 48 gcc-deps seeds each of `symlink_retarget`,
`pragma_alias`, `macro_include` and `file_macro`, and 48 all-profile
seeds each of `pragma_alias` and `embed_data`, found no miss. The
`#embed` kinds need a compiler with `#embed` for the depfiles, so they
do not run under gcc-deps with gcc 14. The reproducers
`pass_has_embed_created`, `pass_has_embed_build_input`,
`pass_link_retarget` and `pass_link_target_edit` pin the shapes around
them that pass.

Cases run four at once, each in its own
process with two compiles or sensor runs at once. A compile or sensor
run still going after 120 s, or a case process that has not reported
after 240 s, is killed with SIGKILL and the case is an ERROR. Every
compile, sensor and gcc run gets only `PATH`, `LC_ALL=C`, `HOME` and a
`TMPDIR` inside the case, so an inherited `CPATH` or `C_INCLUDE_PATH`
cannot change what they read. The default
run of 94
cases (43 fixed, 51 seeds) takes about 11 s and yields 73 narrowed verdicts and 52 seeded
changed functions.

### Measured: the consumer on engine/modules/hotswap (2026-09-27)

A scratch witness (not shipped) extracted the 66 TUs of
`engine/modules/hotswap` and their headers with `git archive`, compiled each
with the real dev flags (gcc, `-Og`, no `-ffunction-sections`) and sensed
it, applied one edit per seed, compiled and sensed again, and ran
`dev.change.plan` with `"facts"` through every `facts_offset` page. It
compares three things per seed: (1) compile: every TU whose object bytes
changed must be predicted affected (MISSED); (2) new bytes: every function
whose machine code changed, after relocation addends and NOP alignment
padding are set aside, must be a seed, sit in a TU the header path
broadened, or the plan must have fallen back (NOT_COVERED); (3) external
behavior is not measured here. The conservative set is every TU whose
depfile names a changed file, or every TU when a changed file is a build
input. Branch head c478e77558 for the header seeds and b4f2b17ddd for the
`.c` seeds; host load average 14 to 27 on 32 cores; each seed recompiled
cold.

| seed | edit | conservative / facts TUs | objects changed | MISSED | new-byte functions (not covered) | obligations plain / facts | verdict |
|---|---|---|---|---|---|---|---|
| a_static_body | a static's body in `hotswap_loader.c` | 1 / 1 | 1 | 0 | 3 (0) | 22 / 22 | `address-taken` |
| b_extern_body | an external body in `hotswap_service.c` | 1 / 1 | 1 | 0 | 1 (0) | 49 / 49 | `unknown-effect` |
| c_typedef | a typedef parameter gains `restrict` | 1 / 1 | 0 | 0 | 0 | 14 / 14 | `address-taken` |
| d_macro_used | `ZCL_HOTSWAP_SERVICE_MAX` 16 to 17 | 50 / 1 | 1 | 0 | 10 (0) | 63 / 49 | narrowed |
| d_macro_if | a no-op term in a header `#if` | 9 / 9 | 0 | 0 | 0 | 34 / 75 | narrowed |
| e_shadow | a copy of a header on a search dir ahead of it | 1 / 1 | 0 | 0 | 0 | 14 / 14 | `include-graph-truncated` |
| f_signature | a return type, header and definer | 50 / 28 | 13 | 0 | 29 (0) | 63 / 63 | `address-taken` |
| g_address_taken | a static's address taken in a new file-scope constant | 1 / 1 | 1 | 0 | 2 (0) | 22 / 22 | `declaration-changed` |
| h_layout | a struct gains a field | 50 / 28 | 24 | 0 | 0 | 63 / 63 | `address-taken` |
| i_same_name | a static's body, another TU has a same-name static | 1 / 1 | 1 | 0 | 5 (0) | 22 / 22 | `address-taken` |
| j_flag_drift | a header comment and a `-D` every compile gains | 1 / 66 | 0 | 0 | 0 | 14 / whole catalog | `identity-drift` |
| k_macro_64_65 | `ZCL_HOTSWAP_GEN_MAX_REPLACED` 64 to 65 | 9 / 1 | 1 | 0 | 7 (0) | 34 / 34 | `address-taken` |
| l_new_header | a new header nothing includes | 0 / 0 | 0 | 0 | 0 | 5 / 5 | narrowed |
| m_shadow_src | a changed copy of `hotswap.h` beside the sources | 4 / 4 | 3 | 0 | 7 (0) | 14 / 14 | `include-graph-truncated` |
| n_indirect_callee | a string in a function called through a pointer | 1 / 1 | 1 | 0 | 0 | 22 / 22 | `address-taken` |
| o_tool_drift | the Makefile, and every compile gains `-fstack-protector-all` | 66 / 66 | 65 | 0 | 1,022 (0) | 1 / whole catalog | `identity-drift` |
| p_line_shift | a comment line above a logging function (712e05de35) | 1 / 1 | 1 | 0 | 1 (0) | 14 / 14 | `address-taken` (rule 11 seeds the moved logger, whose address is taken) |

Every seed passes: no missed dependency and no uncovered new-byte function.
The two shadow seeds (`e_shadow`, `m_shadow_src`) were measured before the
include graph kept a quoted include no depfile lists as an edge
(be2e35e22b); they were not rerun after it.
Under the rules before rule 11 (inferred from them, not rerun),
p_line_shift would have narrowed with no seed, leaving its moved logger
an uncovered new-byte function.

Compile executions avoided against the conservative set: 49 (d_macro_used),
22 (f_signature), 22 (h_layout), 8 (k_macro_64_65); 101 over the 16 seeds,
while j_flag_drift predicts 65 more than the depfile set (the flag reaches
every TU; the depfile set is unsound there, and so is the plain plan for a
Makefile edit, which selects one group). Test-group executions avoided
against the plain plan: 14 (d_macro_used); d_macro_if runs 41 more, and the
two drift seeds run the whole catalog.

d_macro_if: the plain plan for a header is the header's own path groups and
its reverse-include closure; it does not include the obligations of the
functions in the TUs that read it. A macro tested in `#if` can change any
code in a reader, so the consumer broadens the nine readers and adds each
one's file-seeded plan. On the repository's own index the plain plan of the
header selects 40 groups, of its nine readers 123, of both 123: the plain
header plan is a strict subset of what a code change in those readers
needs, so it is unsound for a header edit that changes their code, and the
facts plan (the header's plan plus the readers') is not wider than that
conservative union. Here the edit was a no-op and no object changed, so the
41 groups are the cost of not evaluating the `#if`.

Most body edits now fall back (`address-taken`): the hotswap module
registers its handlers by address, and the code-generation closure reaches
them. The precise reductions are at the compile level (d, f, h, k) and in
d_macro_used's obligations.

### Measured narrowing (candidate branch, 2026-09-25 and 2026-09-26)

These numbers were taken with manifests from the in-compile plugin described
above, which were byte-identical to the sensor's except for the producer
digest, so the verdicts and plan sizes carry over to the sensor. They were
taken before the head, seed, producer-source, code-generation and position
rules existed; those rules can only turn a narrowing into a fallback or add
seeds. The compile-overhead figures from that run measured the plugin, not
the sensor, and are not repeated here: the sensor's cost is its second
parse, given above.

**engine/modules/hotswap.** `z23-dev dev change plan` ran on the same edit
without and with `"facts"`; `code impact` gives the file-seeded count. The
copy's code index had no include graph (`no-include-graph`).

| edit | files given | plain: groups selected / execution groups | facts verdict | facts: reached files / groups selected / execution groups | `code impact` files |
|---|---|---|---|---|---|
| comment | `hotswap_loader.c` | 25 / 54 | narrowed, no seeds | 1 / 5 / 18 | 119 |
| body | `hotswap_loader.c` | 25 / 54 | narrowed, seed `hotswap_generation_count` | 3 / 5 / 18 | 119 |
| layout | `hotswap.h` | 5 / 18 | `not-c-source` | fallback, 5 / 18 | 1 |
| layout | the 4 including TUs | 82 / 168 | `layout-changed` | fallback, 82 / 168 | n/a |
| macro | `hotswap.h` | 5 / 18 | `not-c-source` | fallback, 5 / 18 | 1 |
| macro | the 4 including TUs | 82 / 168 | `macro-changed` | fallback, 82 / 168 | n/a |
| flag | all 12 TUs | 140 / 265 | `identity-changed` | fallback, 140 / 265 | 5 to 120 per TU |

"Groups selected" counts path groups plus closure groups. Function-body and
comment edits narrow the feedback plan from 25 to 5 selected groups and from
54 to 18 execution groups; every other edit falls back to the plain plan.
The base manifests of the 12 TUs held 894 symbols, 1,794 references, 361
UNKNOWN records (293 external calls, 60 atomics, 8 indirect calls), 344
namespace probes and no truncation.

**Real commits.** Each commit's parent was extracted with `git archive`, and
the commit's changed production `.c` files got a manifest before and after.
Set A is 12 recent commits picked before any result was seen; set B is 7
commits picked by a diff filter for hunks with no top-level declaration line,
which favours body edits.

| set | commits | narrowed | fallbacks |
|---|---|---|---|
| A | 12 | 2 | `declaration-changed` 7, `include-resolution-changed` 1, `file-scope-changed` 1, `not-c-source` 1 |
| B | 7 | 5 | `declaration-changed` 1, `file-scope-changed` 1 |

Most fallbacks are real declaration changes in the edited file, such as a
new static helper or a changed static signature. These are not position
artifacts: DECLS carry no positions, and the whitespace fixture edit
narrows. Across the 7 narrowed commits, 198 selected groups became 59 and 483
execution groups became 192; across all 19, 1,485 execution groups became
1,194. Each seed was the function the diff edits, and no narrowing was false
on the falsification set. Every narrowed commit also changed a test harness
`.c`, a script, the Makefile or a doc, so planned as a whole commit each falls
back (`not-c-source`, or `no-manifest` for a harness TU without manifests):
the saving applies to the edit loop, where one production `.c` is planned at
a time. A narrowed plan took 0.12 to 0.17 s against 0.09 to 0.11 s plain;
manifests ran from 59,253 to 841,893 bytes per TU.

**A narrowed plan is not proof.** Proof still runs the full closure, so the
saving is feedback work only.

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
