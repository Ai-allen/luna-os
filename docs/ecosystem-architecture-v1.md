# LunaOS Ecosystem Architecture v1

## Status

This document freezes `LunaOS Ecosystem v1` as of `2026-04-11`.

It records the current architecture boundary for the LunaOS ecosystem without
expanding beyond the contracts already implemented in the tree.

## Layering

- System layer: `BOOT`, `SYSTEM`, `DATA`, `SECURITY`, `LIFECYCLE`, `DEVICE`,
  `TIME`, and `OBSERVABILITY` provide boot, policy, storage, hardware, and
  audit foundations.
- Platform layer: `PROGRAM`, `PACKAGE`, `UPDATE`, `NETWORK`, and `GRAPHICS`
  expose executable loading, distribution, update, transport, and user-surface
  projection.
- Application layer: system and third-party apps are distributed as `.luna v2`
  and executed as `.la` payloads under the frozen runtime ABI.
- Developer layer: host-side packaging, bundle inspection, ABI declaration, and
  audit tooling are provided by `tools/luna_pack.py`,
  `include/luna_la_abi.h`, and `tools/luna_orchestrate_audit.py`.
- User layer: users interact through `Apps`, `Settings`, `Files`, `Notes`,
  `Guard`, `Console`, shell, and desktop projections rather than through raw
  object paths.

## Frozen Ecosystem Objects

- `.luna v2` is the distribution, install, and update-visible unit.
- `.la` is the runtime executable payload loaded by `PROGRAM`.
- package catalog visibility is owned by `PACKAGE`.
- runtime launch, session, and ABI admission are owned by `PROGRAM`.
- data persistence remains owned by `DATA` under `SECURITY` approval.
- cross-space orchestration uses `LunaLink` as the message and authority
  carrier.

## 15-Space Cooperation Boundary

The frozen ecosystem v1 cooperation model includes these spaces:

- `BOOT`
- `SYSTEM`
- `DATA`
- `SECURITY`
- `GRAPHICS`
- `DEVICE`
- `NETWORK`
- `USER`
- `TIME`
- `LIFECYCLE`
- `OBSERVABILITY`
- `AI`
- `PROGRAM`
- `PACKAGE`
- `UPDATE`

Static authority, dynamic execution, and user projection remain separated:

- authoritative storage ownership is not inferred from path names
- runtime loading is not package ownership
- user-facing directories and app lists are projections, not raw storage truth
- install, remove, update, and recovery remain cross-space flows, not
  single-module behavior

## Frozen v1 Boundary

Included in v1:

- `.luna v2` distribution contract
- `.la ABI/SDK v1` runtime contract
- signer/source trust chain enforcement
- audit-chain visibility for install/remove/update
- current developer loop `pack -> install -> run -> remove`
- current system app and third-party app acceptance path

Out of scope for v1:

- richer store or marketplace infrastructure
- broader signing PKI or delegated trust hierarchy
- broader orchestration UX beyond current CLI/log tooling
- generalized protocol or capability expansion beyond current contracts
