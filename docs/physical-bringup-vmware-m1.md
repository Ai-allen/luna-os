# LunaOS VMware UEFI Bring-up M1

## Scope

This document defines the VMware Workstation cross-check used before the first
real physical machine run.

It does not replace physical bring-up. It exists only to reduce the gap
between `QEMU UEFI` and real `x86_64 UEFI` machines.

## Minimum VMware Configuration

Use the existing GA boot artifact:

- [`build/lunaos_installer.iso`](C:/Users/AI/lunaos/build/lunaos_installer.iso)

Recommended first VM configuration:

- firmware: `UEFI`
- secure boot: `off`
- vCPU: `1`
- memory: `1024 MB`
- primary boot device: SATA CD-ROM backed by `lunaos_installer.iso`
- optional second device: SATA virtual disk only if later persistence checks are needed
- display: default VMware SVGA, `64 MB` video RAM is sufficient
- NIC: disconnected for first shell-ready run
- USB/audio/printer/parallel/floppy: disabled

The first run should remove unnecessary virtual hardware rather than trying to
match a rich desktop profile.

If a NIC must be attached for observation, prefer:

- `e1000e`

That NIC is not part of the first VMware success gate.

## VMware Success Standard

The VMware cross-check is successful only when the VM:

- reaches `LunaLoader`
- reaches `UEFI -> LunaLoader -> LunaOS stage`
- completes the minimum bring-up chain
- reaches `[USER] shell ready`
- can show `setup/status` or minimum `BOOT state`

The VMware cross-check does not require:

- desktop
- external networking
- `update.apply`
- complete app coverage

Current validated extension beyond the minimum M1 gate:

- the current automated desktop cross-check reaches
  `desktop.boot -> Settings -> Files -> Console`
- the current VMware path therefore provides a stronger pre-physical comparison
  than the minimum shell-ready-only M1 success line

## Boot Path

Create the session directory first:

- `pwsh -NoProfile -File .\build\start_vmware_bringup_session.ps1`

Each session directory now includes:

- `finalize-session.ps1`
- `firsthop-summary.txt`
- `firsthop-classification.txt`
- `firsthop-reference.txt`
- `firsthop-delta.txt`
- `firsthop-verdict.txt`

Then create a VMware Workstation VM with:

- [`build/vmware_lunaos_m1.vmx`](C:/Users/AI/lunaos/build/vmware_lunaos_m1.vmx)
  as the baseline template, or match its settings manually

Boot from:

- [`build/lunaos_installer.iso`](C:/Users/AI/lunaos/build/lunaos_installer.iso)

Do not start by converting the release disk image to a VMware-specific format
for the first shell-ready pass. That adds a storage conversion variable before
the base UEFI path is proven.

## What To Watch On First Boot

The expected early sequence is:

- `LunaLoader UEFI Stage 1 handoff`
- `LunaLoader UEFI Stage 1 boot-services ok`
- `LunaLoader UEFI Stage 1 stage-alloc ok`
- `LunaLoader UEFI Stage 1 scratch-alloc ok`
- `LunaLoader UEFI Stage 1 loaded-image ok`
- `LunaLoader UEFI Stage 1 stage-load ok`
- `LunaLoader UEFI Stage 1 manifest ok`
- `LunaLoader UEFI Stage 1 block-io ready|missing`
- `LunaLoader UEFI Stage 1 gop ready|missing`
- `LunaLoader UEFI Stage 1 jump stage`
- `[BOOT] dawn online`
- `[USER] shell ready`

If the VM stops before `jump stage`, treat the last visible `LunaLoader` line
as the UEFI split point.

If the VM reaches `jump stage` but not `[USER] shell ready`, the split point
has moved into runtime bring-up.

## Minimum Log Collection

For each VMware run, save:

- `vmware.log`
- `vmware_m1.serial.log` or `serial-capture.log` when serial is enabled
- screenshot of the last visible screen if boot stops
- the session checklist and operator notes
- whether `[GRAPHICS] framebuffer ready` or `[GRAPHICS] console ready` appeared
- whether `[USER] shell ready` appeared

If the VM boots into shell, also record:

- result of `setup/status` or minimum boot state query
- whether keyboard input works in shell

After saving the serial capture, run:

- `.\finalize-session.ps1`

This writes:

- `firsthop-summary.txt`
- `firsthop-classification.txt`
- `firsthop-reference.txt`
- `firsthop-delta.txt`
- `firsthop-log.txt`
- `firsthop-verdict.txt`

These artifacts keep VMware and the first future real-machine trace aligned to
the same handoff/storage/display/input/driver-family triage model.
When a raw `DISK` residual exposes only an `lba=...`, the verdict now refines
it into `lsys+offset` or `ldat+offset` when the selected healthy reference
provides the matching disk layout context.

## Current Status

Current repository status is:

- the UEFI loader exposes the handoff checkpoints needed for VMware comparison
- the VMware session directory script exists
- the automated VMware desktop cross-check
  `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
  is now running successfully in this environment
- current verified output reaches:
  - `[BOOT] dawn online`
  - `[USER] shell ready`
  - `first-setup required`
  - `[DESKTOP] boot ok`
  - `[DESKTOP] launch Settings ok`
  - `[DESKTOP] launch Files ok`
  - `[DESKTOP] launch Console ok`

That means VMware UEFI is no longer only a manual comparison path. It is now a
real automated pre-physical gate, but it still does not substitute for a real
Intel or AMD machine run.
