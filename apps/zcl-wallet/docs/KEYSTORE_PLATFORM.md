# Android custody workflow — device qualification pending

AndroidKeyStore owns a nonexportable AES-256 key. It is separate from Zclassic
spending keys, which are derived in C from BIP39 entropy. The platform receives
entropy only for authenticated encryption/decryption; C owns the record format,
path/profile checks, receiving derivation and custody acceptance predicate.

`KeystoreWrappingKey` queries actual `KeyInfo` metadata and normalizes it for
`zcl_wrapping_policy_check`. The C predicate requires all of:

- 256-bit AES, only encryption/decryption, GCM and no padding;
- provider-generated origin and reported TEE or StrongBox protection;
- user authentication required and enforced by secure hardware;
- authentication for every operation, with strong biometric or device credential;
- no on-body extension, timed authentication, unknown flags or authentication types.

The adapter requires a secure, unlocked device and available platform
authentication. It checks C storage is missing before generating a wrapping
key. Existing record data cannot trigger replacement or regeneration of its key.
One process-wide lock serializes alias creation across activity instances; the
manifest declares no additional process. C independently prevents wallet-record
overwrite. Missing/invalidated keys or unsupported hardware are explicit failures.
There is no software fallback, export or key-deletion API in the application.

Creation initializes a fresh provider cipher without a caller IV. Unlock uses
the validated record's IV and a 128-bit GCM tag. BiometricPrompt must authenticate
that exact cipher before the UI adapter supplies AAD and calls `doFinal` once.
The UI must never treat a successful prompt alone as successful decryption:
GCM verification and C address re-derivation must both succeed.

Android's [per-use authentication guidance](https://developer.android.com/identity/sign-in/biometric-auth#auth-per-use-keys)
uses `setUserAuthenticationParameters(0, ...)`. Its
[KeyInfo reference](https://developer.android.com/reference/android/security/keystore/KeyInfo#getUserAuthenticationValidityDurationSeconds())
documents -1 for per-use metadata. The
[Android 15 provider source](https://android.googlesource.com/platform/frameworks/base/+/android-15.0.0_r1/keystore/java/android/security/keystore2/AndroidKeyStoreSecretKeyFactorySpi.java)
initializes the metadata duration to 0 when no timeout authorization is present.
The C predicate accepts only 0 or -1, together with the other mandatory
authentication checks. Positive timeout values and all other negative values
are rejected. The reviewed provider source SHA-256 is
`752e8f3f1c41174fc62d17cdac3c8c0be2dfc20a21b1072e80fd0761473213a8`.

`setUnlockedDeviceRequired(true)` is additionally set from API 35. Android
[documents compatibility bugs on versions 12–14](https://developer.android.com/reference/android/security/keystore/KeyGenParameterSpec.Builder#setUnlockedDeviceRequired(boolean));
per-use authentication remains mandatory on every supported version. We do not
claim that API-35 KeyInfo can publicly report this additional flag; its new
getter is a later API. The flag is not a substitute for per-use authentication.

An emulator may report virtual TEE metadata. These checks trust the Android
provider and do not authenticate a compromised OS, prove a physical secure
element, or replace real-device testing. Instrumentation uses fresh isolated
test aliases and public fixture data, deleting only aliases it created. The
emulator has a public test-only screen-lock PIN; it is not a production device.
No development key or address may control real funds.

`OwnedExecutor` bounds the platform queue to four entries and one active worker.
Each submitted input has one cleanup path on completion, rejection or discard.
Closing cancels queued work, permits active native/provider code to complete its
cleanup, then clears worker-owned session state on the same thread. It never
waits for filesystem/keystore work on the UI thread.

The activity now drives create/restore/confirm/receive/lock/unlock screens.
`WalletAuthentication` binds approval to the same provider Cipher, retains no
entropy while the prompt is pending, and delivers only in the foreground.
Credential UI can pause the activity; a bounded 90-second continuation handles
that transition. Setup expires after ten foreground minutes. Every failed
worker action clears retained setup entropy, including before a fatal VM Error
propagates. Leaving the app
clears secret views and closes the worker-owned setup. Backup confirmation uses
the C checksum decoder and full-length entropy comparison before saving.

Recovery views own bounded char arrays and disable saved state, autofill,
content capture and selection. Entry uses a local keyboard without an IME or
clipboard. The activity enables FLAG_SECURE, hides overlays where supported,
and rejects obscured touches. Android rendering/GC copies cannot all be erased;
these measures do not defend against a compromised OS or authorized hostile
accessibility service. The application has no recovery export/logging path.

On 2026-09-12 the API-35 development emulator reported key size 256,
security level 0 (software), hardware-enforced authentication false, per-use
duration 0 and authentication methods 3. The C policy rejects those actual
provider capabilities. Two isolated Keystore tests pass, including refusal to
finalize a per-use operation without authentication. Successful create/restore
and unlock UI acceptance is **not qualified** by this emulator. The positive
interactive cases remain separate from the explicit software-protection refusal
case, and the policy is not changed to accommodate the emulator.

The software-protection refusal UI case passes: the actual newly generated key
is rejected by the C predicate, no recovery view is shown and no wallet record
is created. The fixture preserves preexisting empty storage scaffolding and
removes only its own files/alias. Two secret-view lifecycle tests also pass.
