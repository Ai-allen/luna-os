# LunaOS Native Storage Spec v1

## Status

Frozen commercial specification for LunaOS Native Storage / Install v1 as of `2026-04-10`.

## Volume Model

LunaOS native storage v1 consists of two native volumes plus one compatibility volume:

- `ESP`
  - compatibility layer
  - UEFI boot files only
- `LSYS`
  - LunaOS native system volume
  - sealed system store
  - carries active system identity and activation marker
- `LDAT`
  - LunaOS native data volume
  - mutable object/set store
  - carries user, package, setup, logs, recovery, and mutable runtime state

`LSYS` and `LDAT` are a single installation unit only when all of the following match:

- `install_uuid`
- native `profile`
- peer volume LBA contract
- activation contract

## Hard Contract

`LSYS` must carry:

- `profile = LSYS`
- authoritative system objects
- activation marker
- peer pointer to `LDAT`

`LDAT` must carry:

- `profile = LDAT`
- authoritative mutable objects and sets
- peer pointer to `LSYS`
- writer-governed mutation state

Boot must refuse normal activation when the pair contract is broken.

## Consistency Rules

Commit success means all of the following are true:

1. payload or staged extents are durable
2. metadata delta is durable
3. transaction record is durable in committed form
4. checkpoint-visible metadata is switched
5. old state is retired or left replayable

Commit order is frozen as:

1. `prepare txn`
2. `write payload`
3. `write metadata delta`
4. `mark committed`
5. `switch checkpoint`
6. `mark applied / retire old state`

Single-object mutation is atomic at checkpoint visibility boundary.

Multi-object mutation is allowed only inside one bounded mutation group on one volume.

Cross-volume mutation is not atomic in v1.

## Replay / Reject

Crash handling is frozen as:

- `prepared` and not committed: reject
- `committed` and not checkpoint-visible: replay if governance approves, otherwise reject
- `applied` and not retired: treat as committed result, then complete cleanup

Replay and reject must go through `SECURITY` governance approval.

## Writer Identity Contract

`DATA` must not accept anonymous mutation.

Every write or commit must carry and pass:

- writer space identity
- authoritative space identity
- current volume state
- current system mode
- `SECURITY` approval

Mutation is rejected when any of the following are false:

- writer owns the target object class or an allowed delegated set
- current mode allows that writer
- current volume state is writable
- `SECURITY` approved the action

## SECURITY Contract

`SECURITY` is the hard arbiter for:

- mount approval
- writable transition
- commit approval
- replay vs reject decision
- cross-space write approval
- volume-state downgrade or escalation

No normal install, setup, package, update, or replay path may bypass this gate in v1.

## Volume-State Machine

The frozen state set is:

- `healthy`
- `degraded`
- `recovery-required`
- `fatal-incompatible`
- `fatal-unrecoverable`

The derived system mode set is:

- `normal`
- `readonly`
- `recovery`
- `fatal`
- `install`

Required behavior:

- `healthy` -> normal writes allowed per governance
- `degraded` -> readonly
- `recovery-required` -> recovery mode
- `fatal-incompatible` -> refuse normal mount and boot
- `fatal-unrecoverable` -> refuse normal mount and boot

## Recovery Boundary

v1 guarantees:

- checkpoint-based recovery
- replay or reject of interrupted transaction
- readonly degradation when the volume remains structurally readable
- refusal when identity or incompatibility contract is broken

v1 does not guarantee rich recovery UX, interactive migration tooling, or multi-step rollback orchestration.
