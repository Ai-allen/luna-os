# LunaOS Platform Contract Index

## Purpose

This document is the single index for the currently frozen platform contracts on
`main`. It maps each frozen contract to its authoritative specifications,
automated gates, and primary implementation modules.

## GA / RC3 Frozen

Status:

- `LunaOS GA (2026-04-10)`
- `LunaOS RC3 Frozen (2026-04-10)` remains the release-candidate freeze basis
  for the current GA system path

Primary specs:

- [`README.md`](/abs/path/C:/Users/AI/lunaos/README.md)
- [`docs/minimum-delivery-spec.md`](/abs/path/C:/Users/AI/lunaos/docs/minimum-delivery-spec.md)
- [`docs/system-capabilities.md`](/abs/path/C:/Users/AI/lunaos/docs/system-capabilities.md)
- [`docs/test-baseline.md`](/abs/path/C:/Users/AI/lunaos/docs/test-baseline.md)

Primary gates:

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

Primary modules:

- `boot/`
- `system/`
- `lifecycle/`
- `user/`
- `graphics/`
- `network/`
- `update/`

## Native Storage / Install v1 Frozen

Status:

- commercial closure frozen
- fresh split-image -> first-setup -> retained state path is part of contract

Primary specs:

- [`docs/native-storage-spec-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/native-storage-spec-v1.md)
- [`docs/native-install-spec-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/native-install-spec-v1.md)
- [`docs/namespace-contract-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/namespace-contract-v1.md)

Primary gates:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- fresh image first boot reaches `first-setup required`
- `setup.init luna dev secret` succeeds
- second boot `login dev secret` succeeds
- retained `LDAT` remains healthy and writable

Primary modules:

- `data/src/main.rs`
- `security/src/main.rs`
- `boot/main.c`
- `build/build.py`
- `update/main.c`
- `user/main.c`

## Ecosystem v1 Frozen

Status:

- `.luna v2`
- `.la ABI/SDK v1`
- release-grade signer/source trust-chain
- audit chain for install/remove/update

Primary specs:

- [`docs/ecosystem-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/ecosystem-architecture-v1.md)
- [`docs/app-package-contract-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/app-package-contract-v1.md)
- [`docs/developer-model-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/developer-model-v1.md)
- [`docs/user-ecosystem-view-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/user-ecosystem-view-v1.md)
- [`docs/ecosystem-trust-chain-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/ecosystem-trust-chain-v1.md)

Primary gates:

- `python .\build\build.py`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_updateapplycheck.py`
- host pack with `python .\tools\luna_pack.py ...`
- audit summarization with `python .\tools\luna_orchestrate_audit.py ...`

Primary modules:

- `package/`
- `program/`
- `update/`
- `security/`
- `tools/luna_pack.py`
- `tools/luna_orchestrate_audit.py`
- `include/luna_la_abi.h`
- `include/luna_proto.h`
- `include/luna_proto.rs`

## Driver Architecture v1 Frozen

Status:

- driver governance is enforced
- driver `.la` runtime contract is split from app `.la`
- stage-one driver classes are inside the same governance chain

Primary specs:

- [`docs/driver-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/driver-architecture-v1.md)

Primary gates:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`

Primary modules:

- `device/main.c`
- `security/src/main.rs`
- `program/main.c`
- `observe/main.c`
- `lifecycle/src/main.rs`
- `include/luna_proto.h`
- `include/luna_proto.rs`
- `include/luna_la_abi.h`

## 15 Spaces Lock

Status:

- formal runtime ownership is locked to 15 sovereignty spaces
- debug/test/diagnostic payloads are auxiliary only and are not formal spaces
- build and constellation-map generation must fail if a 16th formal space
  enters the release/runtime profile

Primary specs:

- [`AGENTS.md`](/abs/path/C:/Users/AI/lunaos/AGENTS.md)
- [`design/space_constitution.md`](/abs/path/C:/Users/AI/lunaos/design/space_constitution.md)
- [`docs/test-baseline.md`](/abs/path/C:/Users/AI/lunaos/docs/test-baseline.md)

Primary gates:

- `python .\build\build.py`

Primary modules:

- `build/build.py`
- `build/luna.ld`
- `include/luna_proto.h`
- `include/luna_proto.rs`
- `lifecycle/src/main.rs`
- `system/src/main.rs`

## Hardware / Firmware Matrix

Status:

- `x86_64 + UEFI` virtualized pre-physical lane established
- VMware UEFI is the current primary validation environment
- QEMU UEFI and BIOS remain regression environments
- `Intel + UEFI`, `Intel + BIOS`, `AMD + UEFI`, and `AMD + BIOS` are not yet
  established because real-machine evidence is still missing

Primary specs:

- [`docs/hardware-firmware-matrix.md`](/abs/path/C:/Users/AI/lunaos/docs/hardware-firmware-matrix.md)
- [`docs/physical-bringup-m1.md`](/abs/path/C:/Users/AI/lunaos/docs/physical-bringup-m1.md)
- [`docs/physical-bringup-vmware-m1.md`](/abs/path/C:/Users/AI/lunaos/docs/physical-bringup-vmware-m1.md)

Primary gates:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_uefi_shellcheck.py`
- `python .\build\run_qemu_uefi_stabilitycheck.py`

Primary modules:

- `boot/`
- `device/`
- `graphics/`
- `user/`
- `build/`

## Security System v1

Status:

- formal system security ownership defined
- `SECURITY` boundary extended beyond approval-only semantics

Primary specs:

- [`docs/security-system-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/security-system-v1.md)
- [`docs/crypto-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/crypto-architecture-v1.md)

Primary modules:

- `security/`
- `boot/`
- `data/`
- `package/`
- `program/`
- `update/`
- `device/`
- `observe/`

## LaSQL Architecture v1

Status:

- native query/index/catalog/view layer formally defined
- governance boundary split across `DATA`, `SECURITY`, and `OBSERVABILITY`

Primary specs:

- [`docs/lasql-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/lasql-architecture-v1.md)

Primary modules:

- `data/`
- `security/`
- `observe/`
- `package/`
- `user/`

## Logging System v1 / LSON Format v1

Status:

- formal logging sovereignty defined under `OBSERVABILITY`
- LunaOS native structured record format (`LSON`) defined
- relationship across `OBSERVABILITY`, `SECURITY`, `DATA`, and `LaSQL` fixed

Primary specs:

- [`docs/logging-system-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/logging-system-v1.md)
- [`docs/lson-format-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/lson-format-v1.md)
- [`docs/security-system-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/security-system-v1.md)
- [`docs/lasql-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/lasql-architecture-v1.md)

Primary gates:

- `python .\build\build.py`

Primary modules:

- `observe/`
- `security/`
- `data/`
- `boot/`
- `device/`
- `include/luna_proto.h`
- `include/luna_proto.rs`

## Cross-Contract Rule

If a change touches more than one frozen contract, the broader gate set wins.
For example:

- changing `SECURITY` approval logic requires storage, ecosystem, and driver
  gates
- changing `PACKAGE` trust behavior requires shellcheck and updateapplycheck
- changing `BOOT` or volume activation requires bootcheck and retained-state
  validation
