# LunaOS RC3 Frozen Release Notes Draft

## Release Identity

- release name: `LunaOS RC3 Frozen`
- release tag date: `2026-04-10`
- release class: `release candidate`

## Release Scope

This draft describes the currently frozen RC3 release candidate package.

The RC3 promise includes:

- `setup -> login -> home -> settings`
- `install -> run -> remove`
- developer loop `pack -> install -> run -> remove`
- product apps: `Settings`, `Files`, `Notes`, `Guard`, `Console`
- `update.status`
- `update.apply` minimal executable closure
- `pairing.status` local `loop peer trust`
- `network adapter inspect`
- `network local-only loop`
- `network external outbound-only`
- external inbound minimal closure
- minimal external network stack main path:
  - `net.status`
  - `net.connect`
  - `net.send`
  - `net.recv`

## Not Ready

The following are intentionally outside the RC3 promise:

- broader update orchestration beyond the current minimal `apply` closure
- protocol expansion beyond the current minimal external message path
- generalized multi-peer or multi-channel external networking behavior
- broader release-grade hardware compatibility beyond the current validated QEMU path

## Known Limits

- `PROGRAM embedded fallback` remains in code as a damage fallback and is not part
  of the RC3 success baseline
- `PACKAGE install/index fallback` remains in code as a damage fallback and is not
  part of the RC3 success baseline
- current external networking promise is limited to the frozen minimal message path
- current update promise is limited to the frozen minimal apply path
- default user-surface success paths are product-facing, but early boot still
  exposes engineering-facing boot logs

## Validated Environment

The current RC3 release package is validated in the repository build and QEMU
test environment:

- Windows host workflow
- repository-local toolchains under `toolchains/`
- BIOS and UEFI QEMU validation paths
- frozen validation scripts listed in `docs/test-baseline.md`

## Release Summary

`LunaOS RC3 Frozen` is suitable as the current formal release candidate because
it holds a stable boot path, stable setup/login/product app path, stable update
apply closure, stable external inbound minimal closure, and a stable minimal
external network stack main path.
