# LunaOS Repository Governance

## Scope

This file is the repository entry contract for Codex and human developers.
It defines the current frozen platform surfaces, the required validation
commands, and the change-control rules for `main`.

## Repository Layout

- `boot/`: LunaLoader, boot handoff, boot entry, linker and image entry code
- `build/`: build system, validation scripts, packaging helpers, release helpers
- `data/`: Native Storage / Install implementation and governance enforcement
- `device/`: DEVICE-space hardware paths and driver governance entry
- `security/`: SECURITY-space approval, trust, mount/write/commit policy
- `system/`: SYSTEM-space orchestration and space bring-up
- `lifecycle/`: phase/state progression and recovery/install/update gating
- `package/`: `.luna` install/remove/index behavior
- `program/`: `.la` runtime load/start contract
- `update/`: update status/apply and activation flow
- `user/`: shell, setup, login, desktop/user projection
- `graphics/`: GOP/framebuffer/UI/window projection
- `network/`: local and external networking paths
- `observe/`: audit and observability paths
- `include/`: shared ABI, protocol, layout, and record definitions
- `docs/`: frozen specifications, contract docs, release docs, roadmap docs
- `tools/`: host-side developer tooling such as `luna_pack` and audit helpers
- `release/`: frozen release artifacts, manifests, checksums, archived logs
- `artifacts/`: local implementation notes and non-release working artifacts

## Self-Owned Architecture Rule

LunaOS is a self-owned operating-system architecture. Changes on `main` must
preserve LunaOS-native ownership instead of silently collapsing back to outside
system models.

Required invariants:

- Native Storage / Install stays LunaOS-native; do not replace system/data
  truth with host filesystems or Linux-style layouts.
- LaSQL is LunaOS's native database system. `LaSQL v1 Phase 1` currently lands
  minimal query/index/catalog/audit entrypoints, but the system must not drift
  into "`PACKAGE`/`USER` private query stacks" or external database
  assumptions.
- Security System is LunaOS's system security subsystem. `Security System v1
  Phase 1` is landed, but this does not authorize scattering security logic
  into unrelated spaces.
- Crypto is LunaOS's system crypto subsystem. `Crypto v1 Phase 1` is landed,
  but keys, release policy, and trust decisions still belong to `SECURITY`.
- Driver governance remains LunaOS-native. Do not regress into a monolithic
  kernel-driver ownership model that bypasses `DEVICE` governance and
  lifecycle/audit enforcement.
- Ecosystem stays on the `.luna` / `.la` / trust-chain / audit-chain mainline.
  Do not regress into traditional package-manager assumptions as the system
  source of truth.

Every meaningful change must be checked against this rule:

- Does the change still preserve LunaOS self-owned system facts?
- Did any subsystem quietly regress into an external model?
- Did any consumer space grow a shadow system capability it should not own?

If the answer is unstable, fix the ownership model first and only then proceed
with feature or regression work.

## 15 Spaces Enforcement

LunaOS implementation work must keep capability ownership aligned to the 15
spaces. Every change should explicitly answer:

- which space owns the capability
- which spaces are consumers only
- which space authorizes or governs it
- which space persists it
- which space audits it
- which spaces use LunaLink to request it

The canonical space roles are:

- `BOOT`: boot chain, early handoff, early-read contracts
- `SECURITY`: identity, auth, trust, secret/key management, policy, crypto,
  security response
- `LIFECYCLE`: phase progression, degradation, recovery, blocking
- `SYSTEM`: orchestration and system-global state
- `DATA`: native storage, object/set, transactions, indexes, LaSQL data
  ownership
- `DEVICE`: hardware execution, driver binding, low-level access
- `GRAPHICS`: display and desktop graphics consumption
- `NETWORK`: network capability consumption and product-facing networking
- `TIME`: time capability
- `OBSERVABILITY`: logs, audit chain, query audit, event observability
- `PACKAGE`: package/install semantics consumer
- `PROGRAM`: runtime load/start consumer
- `UPDATE`: update/apply/activation semantics
- `USER`: user/session/product-facing consumption
- `AI`: AI capability consumption and collaboration

`DIAG` is not a formal sovereignty space. Diagnostic or regression-only payloads
must stay auxiliary, debug/test-profile only, and out of release manifests,
required-space gating, installer dependencies, and recovery decisions.

The formal runtime profile is permanently locked to these 15 spaces. New
capabilities must be assigned inside the existing 15-space model; do not solve
ownership gaps by creating a 16th formal space. Any new `SPACE` name entering
`build/build.py`, `build/luna.ld`, `include/luna_proto.h`,
`include/luna_proto.rs`, manifest generation, or lifecycle/system dispatch is
an architectural blocker that must be corrected before mainline work continues.

Hard ownership rules:

- LaSQL ownership belongs to `DATA`; `PACKAGE`, `USER`, and `OBSERVABILITY`
  are consumers.
- Security System and Crypto ownership belong to `SECURITY`.
- Driver ownership belongs to `DEVICE`.
- Audit-chain ownership belongs to `OBSERVABILITY`.
- Product-facing spaces must not become shadow owners of system capabilities
  for convenience.

## Core Commands

Build:

```powershell
python .\build\build.py
```

Primary frozen validation set:

```powershell
pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1
python .\build\run_qemu_shellcheck.py
python .\build\run_qemu_desktopcheck.py
pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1
python .\build\run_qemu_uefi_shellcheck.py
python .\build\run_qemu_uefi_stabilitycheck.py
python .\build\run_qemu_inboundcheck.py
python .\build\run_qemu_externalstackcheck.py
python .\build\run_qemu_updateapplycheck.py
python .\build\run_qemu_fullregression.py
```

Targeted storage failure / recovery regression path:

```powershell
python .\build\run_qemu_recoverycheck.py
python .\build\run_qemu_lsysfailurecheck.py
```

These focused checks preserve current storage-rooted failure, activation
recovery, and stop-before-product-space behavior. They do not replace the
primary frozen validation set, but they are required when changes touch those
paths.

Developer ecosystem entry:

```powershell
python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna
python .\tools\luna_pack.py inspect .\build\devloop_sample.luna
python .\tools\luna_orchestrate_audit.py --bundle .\build\devloop_sample.luna --log .\build\qemu_shellcheck.log
```

## Frozen Contract List

The following contracts are frozen on `main`:

- `GA / RC3 Frozen`
- `Native Storage / Install v1 Frozen`
- `Ecosystem v1 Frozen`
- `Driver Architecture v1 Frozen`

Authoritative contract index:

- [`docs/platform-contract-index.md`](/abs/path/C:/Users/AI/lunaos/docs/platform-contract-index.md)

## Frozen Files And Layers

The following layers are treated as frozen-contract surfaces. Changes are
allowed only when they are explicit contract maintenance, regression repair, or
approved post-freeze contract work.

- release-facing docs:
  - `README.md`
  - `docs/minimum-delivery-spec.md`
  - `docs/system-capabilities.md`
  - `docs/test-baseline.md`
- Native Storage / Install:
  - `docs/native-storage-spec-v1.md`
  - `docs/native-install-spec-v1.md`
  - `docs/namespace-contract-v1.md`
  - `data/`
  - `security/`
  - `boot/`
- Ecosystem:
  - `docs/ecosystem-architecture-v1.md`
  - `docs/app-package-contract-v1.md`
  - `docs/developer-model-v1.md`
  - `docs/user-ecosystem-view-v1.md`
  - `docs/ecosystem-trust-chain-v1.md`
  - `tools/luna_pack.py`
  - `tools/luna_orchestrate_audit.py`
  - `package/`
  - `program/`
  - `update/`
- Driver governance:
  - `docs/driver-architecture-v1.md`
  - `device/`
  - `security/`
  - `observe/`
  - `lifecycle/`
  - `program/`
  - `include/luna_proto.h`
  - `include/luna_proto.rs`
  - `include/luna_la_abi.h`

## Gate Mapping

- Boot / storage / lifecycle / setup changes:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
  - `python .\build\run_qemu_shellcheck.py`
- Boot / storage failure / recovery / activation changes:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
  - `python .\build\run_qemu_shellcheck.py`
  - `python .\build\run_qemu_recoverycheck.py`
  - `python .\build\run_qemu_lsysfailurecheck.py`
- Shell-visible product behavior:
  - `python .\build\build.py`
  - `python .\build\run_qemu_shellcheck.py`
- Desktop-visible behavior:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
- UEFI loader / handoff / stability:
  - `python .\build\build.py`
  - `python .\build\run_qemu_uefi_shellcheck.py`
  - `python .\build\run_qemu_uefi_stabilitycheck.py`
- Hardware / firmware matrix / pre-physical driver bring-up:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
  - `python .\build\run_qemu_uefi_shellcheck.py`
  - `python .\build\run_qemu_uefi_stabilitycheck.py`
- External inbound / external stack:
  - `python .\build\build.py`
  - `python .\build\run_qemu_inboundcheck.py`
  - `python .\build\run_qemu_externalstackcheck.py`
  - `python .\build\run_qemu_fullregression.py`
- Update apply / activation:
  - `python .\build\build.py`
  - `python .\build\run_qemu_updateapplycheck.py`
  - `python .\build\run_qemu_fullregression.py`
- `.luna` / `.la` / trust-chain / audit changes:
  - `python .\build\build.py`
  - `python .\build\run_qemu_shellcheck.py`
  - `python .\build\run_qemu_updateapplycheck.py`
  - host pack via `tools/luna_pack.py`
- Driver governance / device bind / failure classification:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
  - `python .\build\run_qemu_shellcheck.py`

## Currently Forbidden Expansion Directions

The following directions are not allowed on `main` unless the user explicitly
re-opens them as new stage work:

- protocol expansion beyond current frozen network contract
- architecture rewrites for storage, ecosystem, or driver governance
- replacing LunaOS native storage with external host filesystems for system/data
- new app/product surfaces beyond current frozen app set
- new ecosystem layers beyond `.luna v2`, `.la ABI/SDK v1`, trust-chain, and
  audit-chain
- new driver classes beyond current frozen governance set unless required for a
  specific bring-up blocker

## Done Definition

A change is done only when all of the following are true:

- code and docs agree with current runtime behavior
- required gates for the touched surface have passed
- no temporary diagnostics or generated runtime residue remain staged
- frozen contracts are not silently widened
- commit message states the real contract or regression being changed
- if a required gate was not run, the omission is explicit and justified

## Working Rule

- Prefer the smallest change that restores or preserves the frozen contract.
- Treat mismatch between docs, gates, and runtime as a regression until proven
  otherwise.
- Do not land speculative cleanup that changes contract-visible behavior.
