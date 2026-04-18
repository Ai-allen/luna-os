# LunaOS LaSQL Architecture v1

## Status

This document defines the formal LunaOS native query architecture as of
`2026-04-17`.

LaSQL is the system query, index, catalog, and governed data-view layer for
LunaOS. It is not an external SQL database and it does not replace native
storage ownership.

## System Position

LaSQL sits above:

- `LAFS`
- object storage
- set membership
- transaction boundaries
- `LSYS`
- `LDAT`

LaSQL is:

- a native query layer
- a native index layer
- a native catalog/view layer

LaSQL is not:

- a second independent persistence engine
- a replacement for `DATA`
- a reason to adopt `sqlite`, `PostgreSQL`, or `MySQL` as the formal system DB

## Sovereignty And Governance

- `DATA` owns physical persistence, object layout, and index maintenance.
- `SECURITY` owns query authority, field visibility, and sensitive projection.
- `OBSERVABILITY` owns query audit consumption.
- other spaces consume LaSQL through governed service calls.

## Native Data Model

LaSQL maps relational concepts onto native LunaOS storage:

- table: a governed object family and index definition
- relation: a typed object/set projection over one namespace
- view: a governed projection over one or more indexed object families
- row: a projected object plus metadata envelope
- index: a `DATA`-owned searchable structure bound to object metadata/fields

Built-in query-visible fields in v1 are:

- namespace
- owner
- type
- lifecycle state
- source
- signer
- created-at
- updated-at
- install identity when applicable

These built-in fields exist so catalog, content, and log queries do not depend
on ad hoc per-space schemas.

## Log Relation And LSON Backing

The phase-one logs query target is backed by LunaOS structured records rather
than free-form strings.

- `LUNA_QUERY_TARGET_OBSERVE_LOGS` projects `LSON`-backed records
- `OBSERVABILITY` owns the log relation surface
- `DATA` owns persistent log storage and index consistency
- `SECURITY` owns protected log-field visibility and audit query governance

This means logs query is a governed native data view, not a text scrape path.

## Minimum Query Capability

Phase-one LaSQL supports:

- select-like scans over indexed object sets
- filter predicates
- projection
- sort
- limit
- namespace query
- owner query
- type query
- state query

Join policy in v1:

- general-purpose arbitrary joins are out of scope
- cross-object composition is done through governed views or caller-side key
  stitching over indexed result sets

This keeps the first landing native, predictable, and compatible with current
storage contracts.

## Index Model

Priority v1 indexes are:

- package/app catalog indexes
- user content/files view indexes
- logs/observability indexes
- recovery/update state indexes

Consistency rules:

- index mutation is part of the same governed `DATA` transaction boundary as
  the underlying object mutation
- stale index success is not a valid committed state
- security-sensitive fields may be indexed, but projection of those fields
  remains governed by `SECURITY`

## Query Governance

`SECURITY` governs:

- who may query a namespace
- which fields may be returned
- when only projection or redaction is allowed
- which queries must be denied
- which queries require audit emission

Examples:

- package catalogs may be broadly queryable, but signer/trust details can be
  restricted
- user content views may expose metadata while redacting sensitive fields
- security and incident views may be queryable only by governed authorities

## First Landing Priority

The first landing priority is:

1. package and app catalog query
2. logs and observability query
3. user content and files view query

Package/app catalog query lands first because it directly supports product
install/runtime discoverability with the smallest query surface and the clearest
security model.

## Initial Service Surface

LaSQL v1 requires a governed service surface that can express:

- query target
- filter set
- projection set
- sort mode
- limit
- caller authority
- audit requirement

The exact opcode and ABI can remain minimal at first, but this document fixes
the contract boundary so later implementation does not drift into per-space
query silos.

The `OBSERVE_LOGS` target is expected to project the fixed `LSON` envelope plus
selected structured attributes into LaSQL-visible rows.
