# Public JNI sync fixtures

These twelve response frames are the existing native `sync_fixture_reply`
transcript for request IDs 1..6 on mainnet and testnet. Header bytes come from
`native/tests/electrum_genesis_fixture.h`, already pinned to original Zclassic
beta6 in `docs/READ_ONLY_SYNC.md`. Version/features/balance fields are synthetic
public fixture data: confirmed 1000, pending delta -7, total 993 zatoshis.

The frames were exported directly from the C fixture helper on 2026-09-13;
the one-off C exporter and build command evidence live under
`.cache/android-wallet/48h-20260912T235829Z`. SHA256SUMS identifies the resulting
exact JSON bytes. Kotlin tests consume them as opaque response input, without
recreating protocol or chain decisions. Android instrumentation reuses this
same test resource directory as test APK assets; application APKs do not bundle
it. No fixture is an authenticated server statement or spendable balance.
