# LunaOS Hardware And Firmware Matrix

## Scope

This document records the current support-matrix truth for LunaOS `main` as of
`2026-04-24`.

It separates:

- real-machine support cells
- virtualized pre-physical evidence
- current driver-variance and reliability compression targets

Virtualization evidence is useful only when it reduces the next real-machine
delta. It does not establish an `Intel` or `AMD` support cell by itself.

## Current Real-Machine Matrix

| CPU family | Firmware | Status | Minimum proven level | Current blocker |
| --- | --- | --- | --- | --- |
| Intel x86_64 | UEFI | not established | none | missing real-machine evidence |
| Intel x86_64 | BIOS | not established | none | missing real-machine evidence |
| AMD x86_64 | UEFI | not established | none | missing real-machine evidence |
| AMD x86_64 | BIOS | not established | none | missing real-machine evidence |

Current LunaOS `main` therefore does not yet claim any formal real-machine
support cell.

## Current Virtualized Pre-Physical Status

| Lane | Role | Status | Current proven level |
| --- | --- | --- | --- |
| x86_64 + UEFI virtualization | pre-physical lane | established | installer media, installer target write, installed boot, pre-device read handoff, shell-ready, first-setup required, and current minimum desktop surface |
| VMware UEFI | primary host-level desktop gate | established | current automated validation reaches `[BOOT] dawn online`, `[USER] shell ready`, `first-setup required`, and the current desktop cross-check |
| QEMU UEFI | regression environment | established | current automated UEFI shell and stability regression remain aligned with the VMware-led path |
| QEMU BIOS | regression environment only | regression-green, not a support cell | current automated BIOS boot and shell regression pass, but this does not establish Intel BIOS or AMD BIOS real-machine support |

The established lane is the virtualized `x86_64 + UEFI` pre-physical path.
That lane must not be reported as `Intel + UEFI` or `AMD + UEFI` support.

## Pre-Physical Gate Order

Before moving a hardware / firmware cell, opening a new physical bring-up
session, or classifying a driver-family delta as a LunaOS runtime issue, the
current image should first pass:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
- `python .\build\run_qemu_uefi_shellcheck.py`
- `python .\build\run_qemu_uefi_stabilitycheck.py`

Gate role rules:

- `run_vmware_desktopcheck.ps1` is the formal host-level pre-physical desktop
  comparison gate
- `run_qemu_fullregression.py` remains a QEMU-only regression wrapper
- VMware host instability must not be reported as a QEMU full-regression
  failure

## VMware Gate Failure Classification

When `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1` fails, report
exactly one of:

- `LunaOS guest failure`
- `VMware/vmrun host-start failure`
- `log-capture failure`

Only `LunaOS guest failure` is evidence that the current image regressed in the
VMware lane. The other two categories block pre-physical readiness, but they do
not move a support cell by themselves.

## Current Driver Variance Compression Focus

The next real-machine deltas should be compressed in this order:

- storage path
- input path
- GOP / framebuffer
- firmware-selected driver-family consistency
- installer / update / recovery failure paths and idempotency

These are the places where the first meaningful difference between VMware,
QEMU, and a real board is most likely to appear.

## Current Reliability Residual

The current non-blocking reliability residual is:

- intermittent `QEMU BIOS` `piix-ide` write failure:
  - `[DISK] write fail lba=00008810`

Current classification:

- not an installer target-write regression
- not evidence of a real-machine support cell
- most likely an `LDAT` metadata/object-record write residue during PACKAGE
  bootstrap or the adjacent DATA mutation path

This residue remains open until a deterministic failure branch or a stable fix
is proven by runtime evidence.

## Formal Reporting Rule

When reporting support status:

- real-machine cells may only move from `not established` after real-machine
  evidence is collected
- a physical bring-up directory alone is not evidence; the finalized verdict
  must show `evidence_scope=physical-candidate` and
  `physical_evidence_status=present`
- `physical_evidence_status=present` requires a non-virtualized log path,
  target-cell machine metadata, complete structured operator notes for machine,
  firmware, SATA mode, USB port, capture source, final visible line,
  shell-ready, GOP, and keyboard result, and a finalized evidence manifest
  hashing the capture log, operator notes, and machine metadata
- a physical-candidate runtime pass is still review evidence, not an automatic
  support-cell move
- VMware is the primary virtualized comparison path
- QEMU remains the regression path
- a green virtualized path is a pre-physical result, not a commercial-grade
  hardware-support claim
