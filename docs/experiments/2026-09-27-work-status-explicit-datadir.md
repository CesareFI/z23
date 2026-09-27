<!-- Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. -->

# Work status honors the explicit node datadir

Local time: 2026-09-27T06:41:28-04:00

UTC: 2026-09-27T10:41:28Z

## Question

When a user supplies the global `-datadir` to `zcode work status`, does
the proof stage read that node's admitted-work ledger if the command JSON
omits `datadir`?

## Result

The command now forwards the explicit global datadir to the proof-status
lookup. A JSON `datadir`, when supplied, keeps its existing precedence.
An implicit process default does not acquire an admitted-work ledger.

The focused CLI regression forks the native command against an isolated
fixture. With an explicit global datadir, it requires `Waiting for
independent reproduction`; without one, it requires `Proof status unknown`.
Before the three-line source change, `make t-fast ONLY=zcode_package_dev`
failed at the explicit-stage assertion in 57.8 s (1/1 group failed, zero
skips). After the change, the same cold group passed in 29.7 s (1/1 passed,
zero skips). The RED and GREEN logs are retained locally at
`/tmp/z23-status-datadir-red-20260927.log` and
`/tmp/z23-status-datadir-green-20260927.log`.

Compiler: GCC 16.1.1. CPU: AMD Ryzen 7 PRO 8840U with Radeon 780M Graphics.
Test date: 2026-09-27.

The generated capability inventory source root changed from
`dc63a3b25c17fe953487e227e652f808d25d59bdb10bae4155fe4c12113e1f09`
on the base to
`4adf7830b425bdb7ccba861fa4fbb2979d17522583fd3a3e6e657debf50fe3ec`.
The live-datadir isolation gate passed its planted-violation self-tests and
real tree scan.

## Limit

This fixture verifies local ledger selection and stage reporting. It does
not establish that a remote proof is eligible or that a work item is
accepted. The exact branch proof and full lint still gate publication.
