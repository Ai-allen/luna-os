# LunaOS Test Baseline

## Scope

This document describes the frozen RC3 baseline for LunaOS as of `2026-04-10`.

The current frozen gates are:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_desktopcheck.py`
- `python .\build\run_qemu_uefi_shellcheck.py`
- `python .\build\run_qemu_uefi_stabilitycheck.py`
- `python .\build\run_qemu_inboundcheck.py`
- `python .\build\run_qemu_externalstackcheck.py`
- `python .\build\run_qemu_updateapplycheck.py`
- `python .\build\run_qemu_fullregression.py`

These baselines are aligned to current passing behavior and define the current
RC3 freeze.

## Ecosystem v1 Gate

This gate freezes the current LunaOS ecosystem contract as of `2026-04-11`.
It is layered on top of the RC3 system baseline and must remain aligned to the
current `.luna v2`, `.la ABI/SDK v1`, trust-chain, and audit-chain behavior.

Required path:

- `python .\build\build.py`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_updateapplycheck.py`
- host pack with
  `python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna`
- optional audit summarization with
  `python .\tools\luna_orchestrate_audit.py --bundle .\build\devloop_sample.luna --log .\build\qemu_shellcheck.log`

Frozen contract assertions:

- `.luna v2` headers must carry `source_id`, `signer_id`, ABI, SDK, and proto
  bounds
- `.la ABI/SDK v1` must remain the externally documented runtime ABI
- `source_id != 0` must not be accepted without a non-zero `signer_id`
- `PACKAGE` install must emit `audit package.install approved=SECURITY`
  and `audit package.install persisted=DATA authority=PACKAGE`
- `PACKAGE` remove must emit `audit package.remove approved=SECURITY`
  and `audit package.remove persisted=DATA authority=PACKAGE`
- `update.apply` automation must reflect the current real behavior:
  audit start, `check-only`, and noop/result visibility when no target is staged
- acceptance must not depend on legacy `sample -> console.luna` remapping
  or legacy package/install success semantics

## Native Storage / Install v1 Gate

This gate freezes the commercial Native Storage / Install v1 closure and must
remain green independently of broader product baselines.

Required path:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- fresh image first boot reaches `first-setup required`
- fresh image `setup.init luna dev secret` returns
  `setup.init ok: host and first user created`
- second boot `login dev secret` succeeds
- second boot `setup.status` reports `setup state=ready`
- second boot `home.status` reports retained ownership for `dev@luna`
- retained `LDAT` remains `state=healthy mode=normal`

Frozen contract assertions:

- `LSYS / LDAT` pair identity must match on boot
- `SECURITY` remains the mount/write/commit/replay governance gate
- `DATA` must reject unauthorized or readonly mutation
- fresh split-image must not degrade to readonly during first setup

## shellcheck Baseline

### Purpose

- Verify BIOS boot reaches shell and first-setup/login flow.
- Verify core shell commands return stable user-facing system output.
- Verify the current RC user, storage, package, security, update, pairing, and
  network surfaces.

### Required Current Outputs

- Boot and session:
  - `[USER] shell ready`
  - `first-setup required: no hostname or user configured`
  - `setup.init ok: host and first user created`
  - `login ok: session active`
- Capability baseline:
  - initial `caps: 021`
  - after `revoke-cap program.load`: `caps: 020`
  - after `revoke-cap device.list`: `caps: 019`
- Store baseline:
  - `lafs.version: 3`
  - `lafs.objects: 35`
  - `lafs.state: clean`
  - `lafs.health: ok`
  - `lafs.invalid: 0`
  - `lafs.nonce: 90`
- Package baseline:
  - catalog includes `Settings`, `Files`, `Notes`, `Guard`, `Console`
- Developer loop baseline:
  - host packs `sample.luna` with `tools/luna_pack.py`,
    `tools/luna_manifest.minimal.json`, and `build/obj/console_app.bin`
  - the packed bundle must be `.luna v2`
  - `package.install sample` returns `ok`
  - `audit package.install approved=SECURITY`
  - `audit package.install persisted=DATA authority=PACKAGE`
  - `package.install app=sample.luna status=installed`
  - `package.install package=sample.luna`
  - `package.install result=visible in Apps and ready to open`
  - the next `list-apps` output includes `sample.luna`
  - `run sample` succeeds and emits the current console-facing app output
  - `package.remove sample` returns `ok`
  - `audit package.remove approved=SECURITY`
  - `audit package.remove persisted=DATA authority=PACKAGE`
  - `package.remove app=sample status=removed`
  - `package.remove package=sample`
  - `package.remove result=removed from Apps and launch blocked`
  - the next `list-apps` output no longer includes `sample.luna`
  - `run sample` returns `launch failed` after removal
  - the current RC shell success path does not rely on
    `sample -> console.luna` remapping
  - the current RC shell success path does not rely on `PROGRAM` embedded
    fallback
- App launch baseline:
  - `run Settings` opens the settings surface from shell
- Device baseline:
  - `serial0`, `disk0`, `display0`, `clock0`, `input0`, `net0`, `pointer0` are listed
- Network baseline:
  - `net.info` returns current adapter information
  - `net.loop state=ready scope=local-only`
  - `net.loop bytes=8 data=luna-net`
  - `net.external` verifies the outbound-only path with hardware tx counter
    growth
  - `network state=ready loopback=available external=outbound-only`
  - `network.entry inspect=net.info verify=net.external diagnostics=net.loop`
  - `net.external state=ready scope=outbound-only`
  - `net.external bytes=8 data=luna-ext`
- Surface separation baseline:
  - default success-path shell output must not rely on `[PACKAGE]` or
    `[PROGRAM]`
- Console baseline:
  - unknown commands return `unknown command; run help`
- Settings baseline:
  - `settings.status` prints structured user, home, update, pairing, network,
    and policy rows using product-facing wording

## desktopcheck Baseline

### Purpose

- Verify BIOS boot reaches desktop control flow after setup/login.
- Verify desktop can accept commands and render the current control surface.
- Verify current desktop status fields and the current Files desktop path.

### Required Current Outputs

- Session baseline:
  - `[USER] shell ready`
  - `setup.init ok: host and first user created`
  - `login ok: session active`
- Desktop core:
  - `desktop.boot` -> `[DESKTOP] boot ok`
  - `desktop.focus` -> `[DESKTOP] focus ok`
  - `desktop.control` -> `[DESKTOP] control ok`
  - `desktop.theme` -> `[DESKTOP] theme ok`
- Status baseline:
  - desktop status includes `update=idle pair=unavailable policy=deny`
  - current selected entry remains `Guard`
- Launch baseline:
  - `desktop.launch Settings` -> ok
  - `desktop.launch Guard` -> ok
  - `desktop.launch Notes` -> ok
  - `desktop.launch Files` -> ok
  - `desktop.launch Console` -> ok
  - `desktop.settings` -> ok
  - `desktop.note workspace draft` -> ok
- Files baseline:
  - `desktop.files.next` -> ok
  - `desktop.files.open` -> ok
  - post-open desktop status remains on the current Files selection state
  - current tested hit output includes the current Files window and glyph facts

## RC Residual Baseline Notes

- `PROGRAM` embedded fallback remains in code but outside the current success
  baseline.
- `PACKAGE` install/index fallback branches remain in code but outside the
  current success baseline.
- broader update orchestration beyond the current minimal apply closure is not
  part of the RC3 baseline.
- protocol expansion beyond the current minimal external message path is not
  part of the RC3 baseline.
- Native Storage / Install v1 does not include richer orchestration, broader
  recovery UX, migration tooling beyond current contract, or deeper fallback
  removal.

## updateapplycheck Baseline

- `package.install sample` must succeed under the current ecosystem contract.
- `audit update.apply start` must appear.
- `update.status` must report the current visible state.
- `update action=check-only` must appear when no staged target exists.
- `update.apply noop` and `update.result state=idle current=0 target=0` must
  reflect the current minimal update contract.
- second boot must retain the installed `sample.luna` and allow `run sample`.

## inboundcheck Baseline

- `network.inbound state=ready entry=net.inbound filter=dst-mac|broadcast`
  must appear before injection.
- `net.inbound state=ready scope=external-raw` must appear.
- `net.inbound rx_packets=0->1 filter=dst-mac|broadcast` must appear.
- post-inbound `net.info` must report `tx_packets=1 rx_packets=1`.

## externalstackcheck Baseline

- `net.status` must expose the external-message transport.
- `net.connect` must establish the minimal external peer/session/channel path.
- `net.send` must emit a real outbound message.
- `net.recv` must consume a real external reply.
- final `net.status` must report non-zero tx/rx message counts.

## Baseline Rules

- A frozen RC baseline should only assert behavior observed in current passing
  logs.
- Default user-surface checks should prefer product-facing output rather than
  internal implementation traces.
- Historical names, legacy app identities, obsolete numeric snapshots, and
  stale UEFI assumptions must not remain in the required list once runtime
  behavior changes.
