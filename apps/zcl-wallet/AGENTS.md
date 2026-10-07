# Android wallet implementation contract

The user's 2026-09-11 correction overrides the initial Kotlin-core plan and the
parent repository's C23 language preference for this app: use portable C17
(C11-compatible interfaces where practical). No Rust and no Rust toolchain.

The active wallet mission targets the C-based Z23 codebase. New security-critical
wallet and protocol implementation is C, not C++. Preserve the existing C23 node
and portable C17 Android boundaries. Migrate any required C++ implementation in
small independently tested steps; prove equivalent behavior before replacing
it, and retain independent reference implementations used as test oracles.
Do not rewrite Android lifecycle, UI, permission or Keystore adapters merely to
remove their platform language. Inventory remaining non-C runtime components
accurately. No marketplace development, consensus changes, production nodes,
mining, real wallets or funds belong in this development mission.

Wallet, key, protocol, transaction, networking and validation logic belong in
the C core. Kotlin/Java/JNI is only a thin Android UI/platform adapter. Retain
and adapt existing public test fixtures and Android build setup. Do not import
or touch operator wallets or canonical node data. Worldstream remains dev-only;
no production node, mining, force push or merge.

Before each C implementation commit, record an explicit review of buffer
overflow/underflow; out-of-bounds access; integer overflow/underflow; signed and
unsigned conversions; use-after-free; double-free; leaks; NULL dereferences;
uninitialized memory; dangling pointers; pointer arithmetic; format strings;
stack usage; allocation limits; malformed serialization/network input; races;
resource exhaustion; and secret leakage. Tests complement this review.

All buffer APIs carry explicit lengths. Validate every operation's result.
Do not use strcpy, strcat, sprintf, gets or unbounded scanf patterns. Check
allocation arithmetic before allocation and every returned pointer. Prefer
caller-owned fixed-size buffers and no heap allocation. Every heap allocation
needs checked size, a documented owner and one cleanup path. No recursive
parsing, unbounded stack arrays, hidden global mutable state or secret logs.

Compile authored C with strict warnings as errors. Exercise ASan, UBSan,
malformed-input fuzzing and static analysis on host fixtures. Native device
tests remain required for JNI and Keystore claims. Keep functions small and
cyclomatic complexity low; split checks into named bounded operations.
