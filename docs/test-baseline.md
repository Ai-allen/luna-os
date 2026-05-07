# LunaOS Test Baseline

## Scope

This document describes the frozen LunaOS 1.0 baseline. The `RC3`
`2026-04-10` freeze remains the historical release-candidate basis for this
line, but it is not the current system version string.

The current frozen gates are:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_desktopcheck.py`
- `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
- `python .\build\run_qemu_uefi_shellcheck.py`
- `python .\build\run_qemu_uefi_stabilitycheck.py`
- `python .\build\run_qemu_inboundcheck.py`
- `python .\build\run_qemu_externalstackcheck.py`
- `python .\build\run_qemu_updateapplycheck.py`
- `python .\build\run_qemu_updaterollbackcheck.py`
- `python .\build\run_qemu_installer_failurecheck.py`
- `python .\build\run_qemu_fullregression.py`

Current non-gating bring-up tooling sanity path:

- `python .\build\run_firsthop_suite.py`
- `python .\build\run_firsthop_classifier_selftest.py`
- `python .\build\select_firsthop_baseline.py .\build\qemu_uefi_shellcheck.log`
- `python .\build\render_firsthop_verdict.py .\build\qemu_uefi_shellcheck.log`
- `python .\build\run_bringup_session_smoke.py`

This host-side smoke does not move the support matrix by itself. It only
verifies that the current bring-up session helpers can still generate:

- `firsthop-summary.txt`
- `firsthop-classification.txt`
- `firsthop-reference.txt`
- `firsthop-delta.txt`
- `evidence-manifest.txt`
- `firsthop-verdict.txt`

from the current virtualized reference logs, and that the current firsthop
classifier still splits minimal handoff/storage/input/governance cases and
records runtime consequence the way the bring-up tooling expects.

These baselines are aligned to current passing behavior and define the current
LunaOS 1.0 gate.

Formal 15-space lock assertions:

- formal build/release/runtime profile contains exactly 15 sovereignty spaces
- `build/luna.ld` and generated `build/constellation.map` must agree on the
  same 15 formal spaces
- `DIAG`, `TEST`, and `DEBUG` may exist only as auxiliary debug/test payloads
  and must not appear as formal spaces
- new capabilities must be assigned inside the 15-space model rather than
  widening the formal space table

## Ecosystem v1 Gate

This gate freezes the current LunaOS ecosystem contract as of `2026-04-11`.
It is layered on top of the LunaOS 1.0 system baseline and must remain aligned
to the current `.luna v2`, `.la ABI/SDK v1`, trust-chain, and audit-chain
behavior.

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
  staged `apply-ready`, SECURITY-approved package replacement, COMMITTED
  activation, LSYS activation on reboot, and final `applied` visibility
- acceptance must not depend on legacy `sample -> console.luna` remapping
  or legacy package/install success semantics

## Native Storage / Install v1 Gate

This gate freezes the commercial Native Storage / Install v1 closure and must
remain green independently of broader product baselines.

Required path:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_installer_applycheck.py`
- `python .\build\run_qemu_installer_failurecheck.py`
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

## Installer Failure / Recovery / Idempotency Gate

This gate preserves the installer target failure boundary. It is a virtualized
regression gate, not a hardware support-cell claim.

Required path:

- `python .\build\build.py`
- `python .\build\run_qemu_installer_failurecheck.py`

Frozen current assertions:

- an undersized installer target must enter installer media mode, bind the
  target, prove writer identity, fail at the GPT backup boundary, and emit
  `[SYSTEM] installer gate=blocked`
- the failed installer attempt must stop before `DATA`, `PACKAGE`, `UPDATE`,
  `USER`, first setup, and shell readiness
- host-side installer verification must reject the partial target layout
- retry on a correctly sized target must write ESP, LSYS, LDAT, and LRCV,
  verify LRCV, and emit `[INSTALLER] verify ok`
- an idempotent retry must leave the verified native layout unchanged
- booting the recovered target must report firmware block handoff, LSYS super
  read, native pair validation, normal boot continuation, and first setup

## Targeted Storage Failure / Recovery Checks

This focused regression path preserves current storage-rooted failure and
activation-recovery handling without widening the primary LunaOS 1.0 release gate
list. Run it when changes touch `LSYS` / `LDAT` contract checks, activation
state handling, storage-rooted driver classification, or the stop-before-
`PACKAGE` / `UPDATE` / `USER` boundary.

Required path:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_recoverycheck.py`
- `python .\build\run_qemu_lsysfailurecheck.py`

Frozen current assertions:

- corrupted `LDAT` peer contract must emit `[BOOT] ldat contract fail` and
  `[SYSTEM] storage gate=recovery`
- corrupted `LSYS` peer contract must emit `[BOOT] lsys contract fail` and
  `[SYSTEM] storage gate=fatal`
- activation recovery must emit `[BOOT] activation recovery` and
  `[SYSTEM] storage gate=recovery`
- storage-rooted `fatal` or `recovery` gates must stop before `PACKAGE`,
  `UPDATE`, and `USER`
- the current `recovery` path may continue only through the foundational
  pre-product spaces observed in passing logs before the stop gate is published

## shellcheck Baseline

### Purpose

- Verify BIOS boot reaches shell and first-setup/login flow.
- Verify core shell commands return stable user-facing system output.
- Verify the current LunaOS 1.0 user, storage, package, security, update,
  pairing, and network surfaces.

### Required Current Outputs

- Boot and session:
  - `[USER] shell ready`
  - `LunaOS 1.0`
  - `first-setup required: no hostname or user configured`
  - `setup.init ok: host and first user created`
  - `login ok: session active`
- Capability baseline:
  - initial `caps: 023`
  - after `revoke-cap program.load`: `caps: 022`
  - after `revoke-cap device.list`: `caps: 021`
- Store baseline:
  - `lafs.version: 3`
  - `lafs.objects: 34`
  - `lafs.state: clean`
  - `lafs.health: ok`
  - `lafs.invalid: 0`
  - `lafs.nonce: 97`
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
  - the current LunaOS 1.0 shell success path does not rely on
    `sample -> console.luna` remapping
  - the current LunaOS 1.0 shell success path does not rely on `PROGRAM` embedded
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

## VMware desktopcheck Baseline

### Purpose

- Verify the current VMware UEFI comparison lane remains green.
- Verify the current primary virtualized validation environment still reaches
  the shell and minimum desktop cross-check.
- Keep the VMware-led path aligned with the current hardware / firmware matrix
  rather than leaving it as an unreferenced side path.

### Required Current Outputs

- `[BOOT] dawn online`
- `[GRAPHICS] console ready`
- `[USER] shell ready`
- `LunaOS 1.0`
- `[USER] input lane ready`
- `first-setup required: no hostname or user configured`
- `setup.init luna dev secret`
- `setup.init ok: host and first user created`
- `login ok: session active`
- `desktop.boot`
- `[DESKTOP] boot ok`
- `desktop.launch Settings`
- `[DESKTOP] launch Settings ok`
- `desktop.launch Files`
- `[DESKTOP] launch Files ok`
- `desktop.launch Console`
- `[DESKTOP] launch Console ok`

### Failure Classification

- `VMware desktopcheck host-start failure`:
  - `vmrun` / VMware host state prevented the VM from starting
- `VMware desktopcheck log-capture failure`:
  - the VM ran but the serial log required for the gate was not captured
- `VMware desktopcheck guest failure`:
  - the VM booted far enough to collect logs, but LunaOS output missed a
    required frozen line

## Real input Baseline

### Purpose

- Verify keyboard input is accepted only from the real device input path.
- Verify session-script commands are not counted as interactive input.
- Keep keyboard, serial-operator recovery, and degraded no-local-input paths
  distinct in gate evidence.

### Required Current Gates

- `python .\build\run_qemu_userinputcheck.py`
- `python .\build\run_qemu_desktopinputcheck.py`
- `pwsh -NoProfile -File .\build\run_vmware_inputcheck.ps1`

### Required Current Outputs

- `[DEVICE] input event src=virtio-kbd` or `[DEVICE] input event src=i8042-kbd`
- `[DEVICE] input select ... usb-hid=not-bound` when USB input controller
  candidate evidence is present; this is not a bound USB-HID keyboard claim
- `[USER] input lane src=keyboard`
- `[USER] shell accept src=keyboard`
- `[USER] shell execute src=keyboard`

### Forbidden Current Outputs

- `[USER] session script cmd=`
- `[USER] input lane src=operator`
- `[DEVICE] input event src=serial-operator`
- `[USER] input recovery=operator-shell`

## UEFI shellcheck Baseline

### Purpose

- Verify `UEFI -> LunaLoader -> LunaOS` reaches shell on the current virtualized
  UEFI path.
- Verify the pre-device handoff, UEFI block handoff, and current shell-visible
  product path remain aligned with the automated gate.
- Verify the current UEFI shellcheck closes on user-facing success and current
  capability revoke behavior rather than stale older assertions.

### Required Current Outputs

- UEFI boot baseline:
  - `LunaLoader UEFI Stage 1 handoff`
  - `LunaLoader UEFI Stage 1 block-io ready`
  - `LunaLoader UEFI Stage 1 gop ready`
  - `[BOOT] fwblk handoff ok`
  - `[BOOT] lsys super read ok`
  - `[BOOT] native pair ok`
- Session baseline:
  - `[USER] shell ready`
  - `LunaOS 1.0`
  - `first-setup required: no hostname or user configured`
  - `setup.init ok: host and first user created`
  - `login ok: session active`
- Capability baseline:
  - initial `caps: 023`
  - after `revoke-cap program.load`: `caps: 022`
  - after `revoke-cap device.list`: `caps: 021`
  - `list-devices` must fail after `device.list` is revoked
- Product baseline:
  - `package.install sample` returns `ok`
  - `package.remove sample` returns `ok`
  - `run Settings` opens the settings surface
  - `run Files` opens the files surface
- UEFI network baseline:
  - `net.info driver=13 flags=19 vendor=8086 device=10D3`
  - `net.external state=ready scope=outbound-only`

## Residual LunaOS 1.0 Baseline Notes

- `PROGRAM` embedded fallback remains in code but outside the current success
  baseline.
- `PACKAGE` install/index fallback branches remain in code but outside the
  current success baseline.
- broader update orchestration beyond the current minimal apply closure is not
  part of the LunaOS 1.0 baseline.
- protocol expansion beyond the current minimal external message path is not
  part of the LunaOS 1.0 baseline.
- Native Storage / Install v1 does not include richer orchestration, broader
  recovery UX, migration tooling beyond current contract, or deeper fallback
  removal.

## updateapplycheck Baseline

- `package.install sample` must succeed under the current ecosystem contract.
- `audit update.apply start` must appear.
- first boot must report `update mode=run action=apply-ready` and
  `update state=staged current=1 target=2`.
- apply boot must emit SECURITY-approved remove/install audits,
  `audit update.apply activation=COMMITTED`, and
  `update.result state=committed current=1 target=2`.
- activation reboot must emit `audit update.apply activation=LSYS` and
  `audit update.apply persisted=DATA authority=UPDATE` before product spaces.
- final confirmation must report `update mode=read-only action=applied`,
  `update state=active current=2 target=2`, retain `sample.luna`, and allow
  `run sample`.

## updaterollbackcheck Baseline

- the staged and committed update path must first match the
  `updateapplycheck` activation assertions.
- after activation confirms `current=2 target=2`, `update.rollback` must emit
  `audit recovery.persisted=DATA authority=UPDATE`, return
  `update.rollback ok`, and report
  `update.result state=rolled_back current=1 target=2`.
- the rollback boot must retain `sample.luna`, allow `run sample`, and must
  not emit `audit recovery.denied reason=rollback`, `update.rollback fail`,
  `update.result state=committed`, or
  `update state=active current=2 target=2`.
- a following reboot must persist `update state=rolled_back current=1 target=2`
  and keep `sample.luna` launchable.

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

- A frozen LunaOS 1.0 baseline should only assert behavior observed in current
  passing logs.
- Default user-surface checks should prefer product-facing output rather than
  internal implementation traces.
- Historical names, legacy app identities, obsolete numeric snapshots, and
  stale UEFI assumptions must not remain in the required list once runtime
  behavior changes.
