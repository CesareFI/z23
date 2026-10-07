# Bounded public-address recovery batches

`zcl_recovery_address_batch` derives at most 16 consecutive transparent public
addresses without repeating BIP39 seed derivation and EC-context allocation for
each address. It uses the same existing derivation helpers as the single-address
APIs. It introduces no new key derivation scheme or wallet format.

The input is authenticated, caller-owned BIP39 entropy (16, 20, 24, 28 or 32
bytes). The existing empty-passphrase profile is unchanged. The paths remain
`m/44'/147'/0'/chain/index` on mainnet and `m/44'/1'/0'/chain/index` on testnet;
chain 0 receives and chain 1 returns change. Only account 0 is supported. Every
requested index must be below `2^31`. An invalid child fails the entire batch;
the API never skips or reserves an index.

The output contains exactly `count * 35` bytes: adjacent ASCII addresses without
NUL terminators. No caller output changes unless all addresses succeed, and
unused output capacity is unchanged. Inputs and output must be stable,
nonoverlapping borrowed spans. Supply 32 independent OS-random blinding bytes
for each batch invocation. The function retains no pointers or secrets. Its
bounded seed, address staging buffer and owned EC context are cleared before
returning, including partial-failure paths. No global seed cache is used.

The caller remains responsible for authenticated entropy access. Deriving an
address does not authenticate wallet ownership, inspect balances, discover
history, reserve change, authorize spending or update durable wallet state.
This native API does not enable Android networking, change custody policy,
provide a recovery UI or claim shielded-account recovery.

Regression coverage compares all supported entropy sizes, both networks and
both chains with existing derivation, including the final non-hardened indices.
An independent host OpenSSL oracle checks published mnemonic fixtures against
both single and batch results. Failure injection checks context allocation,
partial ownership, seed failure, every child failure and wrong provider lengths.
Output guards and cleanup are checked after failures and a successful retry.
The normal C safety suite applies static analysis, complexity limits, strict
warnings and sanitizer runs to the added implementation and tests.

`bench_recovery_addresses` compares unchanged single-address calls and the batch
API in the same optimized host binary. It verifies identical outputs, warms
both paths and alternates seven paired samples of ten 16-address batches for
16-byte and 32-byte public zero-entropy fixtures. It reports wall and thread-CPU
time without imposing a timing acceptance threshold. This is native derivation
evidence, not phone battery, network synchronization or end-to-end restoration
performance.
