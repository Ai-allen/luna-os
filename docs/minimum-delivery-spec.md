# LunaOS Minimum Delivery Spec

## Scope

This document defines the frozen release-candidate delivery baseline for
LunaOS.

As of `2026-04-10`, LunaOS is treated as `RC3` rather than a prototype-only
system. Every item below is anchored to the currently accepted RC3 behavior and
the currently accepted RC3 freeze baseline.

## RC Delivery Promise

The current RC3 promise includes:

- BIOS boot into the LunaOS shell
- first setup, login, home, and settings entry
- package-visible app catalog
- stable shell launch of `Settings`, `Files`, `Notes`, `Guard`, and `Console`
- stable developer loop `pack -> install -> run -> remove`
- `update.status` and `update.apply` as reliable update capabilities
- `pairing.status` as a reliable local `loop peer trust` capability
- `network` as a reliable `adapter inspect`, `local-only loop`, `external
  outbound-only`, and external inbound minimal closure capability
- a minimal external network stack default path through `net.status`,
  `net.connect`, `net.send`, and `net.recv`

The current RC3 promise explicitly excludes:

- protocol expansion beyond the current minimal external message path
- generalized multi-peer or multi-channel networking behavior
- any broader update orchestration beyond the current minimal `apply` closure

## RC Freeze Baseline

The current frozen RC3 baseline is:

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
- `python .\build\run_qemu_fullregression.py`

These passing results define the current release-candidate freeze gate.
`run_vmware_desktopcheck.ps1` remains a separate host-level desktop gate.
`run_qemu_fullregression.py` remains the QEMU-only aggregate gate.

## Boot Paths

- BIOS boot must load LunaOS Stage 2 and reach `[BOOT] dawn online`.
- BIOS boot must complete `SECURITY`, `LIFECYCLE`, and `SYSTEM` bring-up.
- BIOS boot must reach `USER` shell bring-up:
  - `[USER] shell ready`
  - `[USER] input lane ready`
- Desktop bring-up is verified through explicit desktop commands, not by
  requiring automatic desktop entry at boot.

## System Main Chain

- The current verified bring-up order is:
  - `BOOT`
  - `SECURITY`
  - `LIFECYCLE`
  - `SYSTEM`
  - `DEVICE`
  - `DATA`
  - `GRAPHICS`
  - `NETWORK`
  - `TIME`
  - `OBSERVE`
  - `PROGRAM`
  - `AI`
  - `PACKAGE`
  - `UPDATE`
  - `USER`
- Verified boot-ready logs include:
  - `[SECURITY] ready`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `Space 2 ready.`
  - `[GRAPHICS] console ready`
  - `[NETWORK] lunalink ready`
  - `Space 13 ready.`
  - `[UPDATE] wave ready`

## User Entry

- Shell must be reachable after boot.
- First boot without host/user state must explain setup requirements.
- After `setup.init <hostname> <username> <password>`, the system must log the
  first user in automatically.
- Desktop session must be reachable through `desktop.boot`.
- Shell commands verified by the frozen RC baseline:
  - `setup.status`
  - `setup.init <hostname> <username> <password>`
  - `whoami`
  - `hostname`
  - `home.status`
  - `cap-count`
  - `cap-list`
  - `seal-list`
  - `space-map`
  - `store-info`
  - `store-check`
  - `list-apps`
  - `run Notes`
  - `run Files`
  - `run Guard`
  - `space-log`
  - `lane-census`
  - `pci-scan`
  - `net.loop`
  - `net.info`
  - `net.external`
  - `revoke-cap program.load`
  - `run Settings`
  - `revoke-cap device.list`
  - `list-devices`

## Software Entry

- The package-visible app catalog must include:
  - `Settings`
  - `Files`
  - `Notes`
  - `Guard`
  - `Console`
- The formal developer loop must succeed from shell using a `.luna` packed on
  the host:
  - pack with `python tools/luna_pack.py tools/luna_manifest.minimal.json build/obj/console_app.bin build/devloop_sample.luna`
  - `package.install sample`
  - `list-apps` includes `sample.luna`
  - `run sample` succeeds by resolving and launching the installed
    `sample.luna` object directly
  - `package.remove sample`
  - `list-apps` no longer includes `sample.luna`
  - `run sample` returns `launch failed`
- The current RC success path no longer depends on:
  - a `sample/sample.luna -> console.luna` remap
  - `PROGRAM` embedded fallback
- `PROGRAM` embedded fallback is still retained as a damage fallback, but it is
  not part of the frozen RC success baseline.
- `PACKAGE` install/index fallback branches still exist in code, but they are
  not part of the current RC success baseline.

## Data Main Path

- `DATA` must come online as `LAFS`.
- Current verified store state:
  - `lafs.version: 2`
  - `lafs.objects: 34`
  - `lafs.state: clean`
  - `lafs.mounts: 1`
  - `lafs.formats: 1`
  - `lafs.nonce: 88`
  - `lafs.health: ok`
  - `lafs.invalid: 0`
- `PACKAGE`, policy state, installed apps, and related runtime objects are
  expected to resolve through `DATA/LAFS`.

## Observability

- The desktop status line must expose:
  - `update`
  - `pair`
  - `policy`
- `settings.status` must print structured status rows that make RC boundaries
  explicit:
  - user/account and host state
  - home/document roots
  - update state as `check-only`, `apply-ready`, `reboot-confirm`, `applied`,
    or `failed`
  - pairing state as local trust state
  - network state as inspect, inbound diagnostics, and external message-stack
    default path
  - policy state

## Residual RC Notes

The following remain intentionally recorded rather than expanded in this freeze:

- `PROGRAM` embedded fallback remains in code but outside the RC success
  baseline
- `PACKAGE` install/index fallback branches remain in code but outside the RC
  success baseline
- pre-shell serial bring-up still exposes engineering-facing boot logs; the
  default shell/runtime surface is the frozen product surface

## Failure Contract

The following are allowed and do not invalidate the current RC freeze:

- `list-devices` returns `failed` after `revoke-cap device.list`
- only the current minimal external message path is promised
- fallback branches remain present in code but outside the success baseline

The following are not allowed:

- boot not reaching `USER`
- shell not becoming ready
- first setup not being explained on an uninitialized system
- `setup.init` not creating a usable first session
- `DATA/LAFS` not coming online
- package catalog not listing the current product apps
- `Settings` failing to produce structured status from shell
- `Notes`, `Guard`, `Files`, `Console`, or `Settings` failing to launch from the
  current RC shell path
- developer loop not succeeding through `pack -> install -> run -> remove`
- `update.apply` not completing the current minimal apply closure
- `net.inbound` not completing the current minimal external inbound closure
- `net.status -> net.connect -> net.send -> net.recv` not completing the
  current minimal external stack path
