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

Failed submission also clears its transferred input if task construction or
worker creation throws. A failure after queue insertion removes that task before
discarding it; a later healthy worker cannot run it or inherit its occupied
queue slot. Ordinary rejection still returns false, and unexpected/fatal failures
propagate as the original exception after cleanup. Close drains queued tasks
without allocating a temporary list. Cleanup callbacks retain their contract to
clear owned data without throwing or blocking.

Two host regressions fail on the previous implementation and pass on the fix.
They inject thread-factory allocation/start failures directly, including the
enqueue-before-start branch, without exhausting system memory. The separate
Android fixture exercises both executor branches on API35 and API36, observes
zeroed public marker arrays, one cleanup, an empty failed queue and a successful
independent retry. These fixtures inspect the existing private executor only in
test code; no shipped injection hook or custody-policy exception is added.

`RecoveryPhraseDelivery` separately owns the one bounded phrase awaiting the UI
queue. Closing the foreground session clears that array immediately, including
while the worker is submitting its callback. A late callback cannot deliver it.
Previously the callback alone cleared undelivered words, leaving them resident
after closure until the UI queue ran. Rejection, excess input and receiver failure
also clear the owned input. Successful delivery transfers cleanup to the secret
view. Closure and delivery run on the main thread; only submission uses the
worker. This reduces retention of the owned array; it does not erase all VM or
framework copies or establish successful hardware authentication.

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

Recovery input rendering failures now clear the complete owned buffer and reset
its length after either append or delete. Cleanup is attempted even for a fatal
rendering error; a second clearing failure is preserved alongside the original
exception after the buffer's finally block runs. Submission clears the preview
before allocating its outgoing char array and clears the original in finally.
Thus a preview-clear failure cannot strand a newly allocated outgoing copy,
and a copy-allocation failure still clears the source. No phrase String, input
connection, clipboard route or additional retained secret is introduced.

Real Android fixtures inject throwing text listeners on public markers. The
prior implementation retained four characters after failed append and two
after failed delete, and did not attempt the cleanup callback. The same test
APK checks refusal, original/cleanup exception preservation, empty subsequent
input and a successful fresh input transfer after the fix. This observes owned
array handling, not complete erasure of Android rendering/runtime copies.

Both recovery views explicitly omit hierarchy saving and discard all supplied
hierarchy state on restoration, clearing their current owned material instead.
The saving-disabled flag alone did not prevent a saved ordinary TextView record
from populating a fresh recovery display with its 28-character public marker.
Restoring a populated display also left its owned array uncleared; keyboard
restoration retained its three existing input characters. The exact same ten
device tests pass after the explicit guards on API30, API35 and API36. The guards do
not read, reinterpret or migrate a supplied Parcelable, and do not make saved
state an authorized recovery route.

Foreground pause detaches and closes its session before attempting the waiting
screen. Nested finally blocks still clear the setup timer/secret views and call
the framework's pause method if earlier cleanup fails. A rendering failure can
no longer skip worker closure or retain the activity's session reference. Active
native/provider work still reaches its own cleanup; queued input is discarded.

The guarded wallet-flow fixture invokes the actual pause method inside a
controlled instrumentation callback, with a throwing text listener and a bounded
worker gate. It observes an open worker on the prior code and closed ownership,
zeroed queued public marker input, no queued execution and completed worker
cleanup after the fix on API30, API35 and API36. It creates no wrapping key or
wallet record and cleans only its invocation-owned scaffold. This establishes
the injected method-failure contract, not physical-device rendering-failure
behavior or positive hardware custody.

Platform work has a process-wide limit of two admitted executor owners, allowing
a foreground session alongside one finishing operation. Each owner retains its
existing one-worker/four-queued-task bound. Admission is nonblocking and lazy on
first submission; an unused or failed parent constructor cannot reserve capacity.
Closing retains its admission through active work and the session finalizer until
pool termination. Rejected input is cleared immediately. The UI shows a busy
message and explicit retry; repeated retries cannot start more retiring workers.
An owner never admitted to a pool has only empty session state and clears it
directly on close. No timeout, interruption or UI wait is used to claim that
native/provider work has finished.

The host regression previously admitted a third owner while two closed owners
still had active tasks. It now also checks refusal while their finalizers run,
exactly-once rejected-input cleanup and successful replacement after termination.
Guarded API30, API35 and API36 activity fixtures hold public worker tasks across
pause/resume, observe repeated busy retries, then release them and return to the
welcome screen. They create no key or wallet. Thread-start failure and failed
pause-rendering cleanup also pass on all three APIs with the lazy admission.

On 2026-09-12 the API-35 development emulator reported key size 256,
security level 0 (software), hardware-enforced authentication false, per-use
duration 0 and authentication methods 3. The C policy rejects those actual
provider capabilities. Two isolated Keystore tests pass, including refusal to
finalize a per-use operation without authentication. Successful create/restore
and unlock UI acceptance is **not qualified** by this emulator. The positive
interactive cases remain separate from the explicit software-protection refusal
case, and the policy is not changed to accommodate the emulator.

## Authentication continuation timing — 2026-09-13

The pending prompt's 90-second window is now enforced by
`zcl_authentication_window_check`, using Android `elapsedRealtime()` observations
at creation, successful callback, foreground delivery and resume. Age >=90000
milliseconds or backward time refuses. The JNI adapter rejects negative Java
timestamps before conversion; no pointers, allocation or secret data cross this
new boundary. The delay value also comes from C.

The prior implementation relied on a Handler timeout alone. Android documents
that [Handler delays use uptime and are extended by deep sleep](https://developer.android.com/reference/android/os/Handler),
while [elapsedRealtime includes deep sleep](https://developer.android.com/reference/android/os/SystemClock).
Consequently the prior code did not enforce its stated elapsed-time limit when
delivery followed suspension or an overdue callback. This is a code/clock-contract
finding, not an observed theft or bypass of the hardware's per-use requirement.
The Handler still schedules cleanup, but its delivery order is no longer the
authority to accept an authentication continuation.

The same-Cipher, foreground, request-identity and per-use hardware checks remain
mandatory. The new predicate only constrains how long the app retains a pending
prompt/foreground continuation; it cannot authenticate a user or approve a key.
Exact boundary/overflow/clock tests, metamorphic timestamp fuzzing and host/device
JNI tests complement the source review. Physical-device suspend/resume during
an actual successful BiometricPrompt operation remains required acceptance.

The software-protection refusal UI case passes: the actual newly generated key
is rejected by the C predicate, no recovery view is shown and no wallet record
is created. The fixture preserves preexisting empty storage scaffolding and
removes only its own files/alias. Two secret-view lifecycle tests also pass.
