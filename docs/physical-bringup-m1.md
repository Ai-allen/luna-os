# LunaOS Physical Bring-up Milestone 1

## Scope

This document freezes the first real-machine bring-up path for LunaOS after
`GA (2026-04-10)`.

Milestone 1 is not a feature expansion. It is the minimum hardware bring-up
path for a real `x86_64 UEFI` machine.

## Success Standard

Milestone 1 is successful only when a real machine can:

- boot `LunaLoader` from physical USB media
- enter `UEFI -> LunaLoader -> LunaOS stage`
- complete the minimum bring-up chain:
  - `SECURITY`
  - `LIFECYCLE`
  - `SYSTEM`
  - `DEVICE`
  - `DATA`
  - `GRAPHICS`
  - `USER`
- reach `[USER] shell ready`
- output a minimum system state such as `setup/status`

Milestone 1 does not require:

- desktop
- external networking
- `update.apply`
- complete app surface validation

## First-Phase Support Boundary

The first real-machine support boundary is:

- `Intel x86_64`
- `UEFI`
- `Secure Boot = off`
- single-disk `SATA AHCI`
- `UEFI GOP` framebuffer
- USB boot

The first-phase success path includes:

- `UEFI Block I/O` boot handoff when available
- `AHCI SATA` runtime storage path

The first-phase success path does not include:

- `NVMe`
- `RAID`
- `VMD`
- vendor-specific graphics bring-up beyond `UEFI GOP`
- NIC success as a bring-up gate

## Physical USB Boot Path

The Milestone 1 USB boot artifact is:

- [`build/lunaos_esp.img`](C:/Users/AI/lunaos/build/lunaos_esp.img)

Write path:

1. Build the current image:
   - `python .\build\build.py`
2. List removable disks from an elevated PowerShell session:
   - `pwsh -NoProfile -File .\build\write_lunaos_uefi_usb.ps1 -List`
3. Write the USB media:
   - `pwsh -NoProfile -File .\build\write_lunaos_uefi_usb.ps1 -DiskNumber <N>`
4. Keep verification enabled unless there is a known media issue.

The write script performs the minimum required verification:

- writes the exact `lunaos_esp.img` bytes to the selected physical drive
- verifies the written range with `sha256`

## Log Collection Path

Prepare a session directory before each physical run:

- `pwsh -NoProfile -File .\build\start_physical_bringup_session.ps1 -Machine <name>`

This creates:

- a per-run checklist
- a machine metadata file
- a `sha256` file for the USB image and related boot artifacts
- an operator notes file
- a `finalize-session.ps1` helper for post-capture triage
- placeholder `firsthop-summary.txt`
- placeholder `firsthop-classification.txt`
- placeholder `firsthop-reference.txt`
- placeholder `firsthop-delta.txt`
- placeholder `firsthop-verdict.txt`

Primary log path for Milestone 1:

- the last visible `LunaLoader` line on the physical display
- the first visible LunaOS boot lines
- whether `[USER] shell ready` appears

Preferred secondary log path when the board exposes UART:

- capture `COM1` at `38400 8N1`
- save the raw serial transcript into the session directory
- run `.\finalize-session.ps1` after saving `serial-capture.log`

If no serial path exists, the minimum failure record is:

- photo of the last visible screen
- machine model and firmware version
- SATA mode
- whether keyboard input worked
- the last visible `LunaLoader` or `[BOOT]` line

When serial capture is available, keep the new selection-basis lines alongside
the existing path lines:

- `[DEVICE] serial select ...`
- `[DEVICE] disk select ...`
- `[DEVICE] display select ...`
- `[DEVICE] input select ...`
- `[DEVICE] net select ...`

These lines explain why LunaOS selected a given driver family, not only which
family it ended up using.

## Real-Machine Handoff Checkpoints

The real-machine checkpoints to compare against virtualized boot are:

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

If the machine stops before `jump stage`, treat the final visible line as the
current handoff split point.

If the machine reaches `jump stage` but does not reach `[USER] shell ready`,
the problem has moved past the UEFI handoff and into runtime bring-up.

## Firsthop Triage Artifacts

After `.\finalize-session.ps1` runs, the session directory must contain:

- `firsthop-summary.txt`
  - extracted boot, firmware, storage, serial, display, input, network,
    platform, and progress lines
- `firsthop-classification.txt`
  - normalized identity and contract state for the captured run
  - current first split-layer guess across `handoff`, `storage`, `display`,
    `input`, and `driver-governance`
- `firsthop-reference.txt`
  - the auto-selected current healthy virtualized baseline used for comparison
  - whether the selected baseline was healthy and which firmware lane it came from
- `firsthop-delta.txt`
  - difference against the current virtualized baseline chosen by firmware type
- `firsthop-log.txt`
  - the captured log name, selected reference baseline, current split layer,
    current priority blocker, and current `driver_family_delta`
- `firsthop-verdict.txt`
  - the single-file bring-up verdict for the session
  - current progress, selected healthy reference, current priority blocker,
    the resolved `storage_residual_region`, and the next recommended check

Use these files to classify the first real-machine failure before changing any
runtime code. The expected first triage layers remain:

- handoff
- storage
- input
- display
- driver-family variance

## Current Gate

Milestone 1 bring-up success requires:

- physical USB boot into `LunaLoader`
- visible progression through the handoff checkpoints
- `LunaOS stage` entry
- `[USER] shell ready`
- minimum shell-visible system status

Milestone 1 does not yet gate on:

- desktop
- external network reachability
- update flow
- broader storage compatibility
