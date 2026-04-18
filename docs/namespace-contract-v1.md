# LunaOS Namespace Contract v1

## Status

Frozen namespace and logical path contract for LunaOS Native Storage / Install v1 as of `2026-04-10`.

## Product Contract Paths

The following logical namespaces are frozen product contract paths:

- `/System`
- `/Apps`
- `/Users`
- `/Logs`
- `/Recovery`
- `/Devices`

Stable user paths are:

- `~/Desktop`
- `~/Documents`
- `~/Downloads`
- `~/Settings`

These are logical namespaces. They are not promises about inode trees or foreign filesystem semantics.

## Backing Model

Backing storage remains LunaOS object/set storage:

- `LSYS`
  - system objects
  - system app roots
  - sealed system manifests and policy projections
- `LDAT`
  - user objects
  - package records
  - logs
  - recovery objects
  - mutable set membership

## Authority Binding

- `/System`
  - authoritative space: `SYSTEM`
  - security-governed subdomain: `SECURITY`
- `/Apps`
  - authoritative spaces: `SYSTEM` and `PACKAGE`
  - projected launcher view, not one physical directory tree
- `/Users`
  - authoritative space: `USER`
- `/Logs`
  - authoritative space: `OBSERVABILITY`
  - canonical persistent record model: `LSON` under `DATA` persistence with
    `SECURITY` query and visibility governance
- `/Recovery`
  - authoritative spaces: `LIFECYCLE`, `UPDATE`, `SECURITY`
- `/Devices`
  - authoritative space: `DEVICE`

## Stable Semantics

- uninstall removes package ownership and app visibility, not arbitrary user data
- delete removes object membership or object state per authoritative owner
- rename changes namespace membership label, not object identity
- archive is a state move into retained sets, not implicit destruction

## Writable Modes

The frozen writable behavior is:

- `normal`
  - authoritative spaces may write their own mutable domains with `SECURITY` approval
- `readonly`
  - mutable product writes are blocked
- `recovery`
  - only recovery-authorized paths may mutate
- `fatal`
  - normal mutation is blocked
- `install`
  - installer-authorized writers may seed required system and data objects

## Out of Scope

v1 does not freeze:

- POSIX compatibility
- inode/path implementation details
- multi-disk namespace spanning
- archival UX beyond logical state meaning
