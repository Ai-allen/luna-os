# LunaOS GA Release Notes

## Status

This document records the first general-availability release position for
LunaOS based on the frozen repository state dated `2026-04-10`.

The GA promise does not widen beyond current validated behavior.

## GA Promise

The current GA promise includes:

- BIOS boot into LunaOS shell
- setup, login, home, and settings entry
- product apps:
  - `Settings`
  - `Files`
  - `Notes`
  - `Guard`
  - `Console`
- shell software path:
  - `install`
  - `run`
  - `remove`
- developer loop:
  - `pack -> install -> run -> remove`
- update surfaces:
  - `update.status`
  - `update.apply` minimal executable closure
- pairing surface:
  - `pairing.status` local `loop peer trust`
- network surfaces:
  - `network adapter inspect`
  - `network local-only loop`
  - `network external outbound-only`
  - external inbound minimal closure
  - minimal external message-stack main path through:
    - `net.status`
    - `net.connect`
    - `net.send`
    - `net.recv`

## GA Not Ready

The following remain outside the GA promise:

- broader update orchestration beyond the current minimal `apply` closure
- generalized multi-peer or multi-channel networking semantics
- protocol expansion beyond the current minimal external message path
- broader hardware or virtualization support beyond the currently validated path
- fallback removal as a success-path requirement

## Known Limits

- `PROGRAM embedded fallback` remains in code as a damage fallback and is not
  part of the success baseline
- `PACKAGE install/index fallback` remains in code as a damage fallback and is
  not part of the success baseline
- current external networking support is limited to the frozen minimal message
  path and minimal inbound closure
- current update support is limited to the frozen minimal apply path
- current validated environment remains the repository-local Windows and QEMU
  test path
- early boot still exposes engineering-facing logs before shell-level product
  surfaces take over

## Supported Environment

The current GA-supported environment is:

- Windows host workflow
- repository-local toolchains under `toolchains/`
- BIOS QEMU validation path
- UEFI QEMU validation path
- automated validation listed in `docs/test-baseline.md`

This release does not claim support for unvalidated hardware or additional
virtualization targets.

## Upgrade Path

The GA upgrade path is:

1. build the current image
2. boot the current image and enter a valid session
3. inspect `update.status`
4. execute `update.apply`
5. reboot through the normal path
6. re-enter the system
7. confirm `current` and `target` converge after reboot
