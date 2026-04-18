# LunaOS Logging System v1

## Status

This document defines LunaOS Logging System v1 as of `2026-04-18`.

LunaOS logging is not a loose string stream and it is not a borrowed syslog
model. It is a governed system service for structured runtime evidence, audit
chains, product-visible events, recovery evidence, and queryable log state.

## System Position

Logging System v1 exists to serve all of these system needs at once:

- system logs
- security and crypto audit logs
- boot and installer evidence
- driver and hardware failure evidence
- failure and recovery evidence
- LaSQL-governed log query
- product-visible state and event projection

Logging System v1 is built around:

- `OBSERVABILITY` sovereign log ownership
- `DATA` persistence and index maintenance
- `SECURITY` policy, visibility, and protected audit governance
- `LaSQL` query projection over structured records

No product-facing or feature-facing space may define an independent parallel log
system.

## Space Sovereignty Model

### Sovereign Roles

- `OBSERVABILITY` owns the LunaOS logging system itself:
  - record taxonomy
  - structured event envelope
  - `/Logs` namespace semantics
  - retention class and fold rules
  - public log read/query service
  - query-audit consumption
- `SECURITY` owns:
  - security audit class semantics
  - crypto event class semantics
  - redaction, deny, readonly, quarantine, and fatal visibility policy
  - protected-field visibility and release rules
  - whether a given log query is allowed, reduced, redacted, or denied
- `DATA` owns:
  - persistent record layout
  - object storage of durable logs
  - index maintenance and consistency
  - audit-chain and digest persistence requested by sovereign spaces

### Execution Roles Across The 15 Spaces

- `BOOT`
  - emits early boot and handoff records before full bring-up
  - does not own a separate boot logging system
- `SYSTEM`
  - emits orchestration, bring-up, and space-graph state
- `DATA`
  - persists and indexes logs
  - emits storage and integrity evidence for its own actions
- `SECURITY`
  - emits security and crypto audit events
  - governs protected visibility and query response
- `GRAPHICS`
  - emits display and desktop surface facts only as a producer
- `DEVICE`
  - emits hardware discovery, driver, lane, storage, input, and framebuffer
    evidence
  - does not own the log system or private driver log storage
- `NETWORK`
  - emits network transport, pairing, and session records
- `USER`
  - emits setup, login, session, and product-surface events
  - consumes user-visible log and incident views
  - does not own log policy, audit rules, or persistence
- `TIME`
  - provides the clock and tick basis used to stamp records
- `LIFECYCLE`
  - emits stage, degrade, recovery, and fatal transitions
- `OBSERVABILITY`
  - owns the logging service, query-facing log projections, and query-audit
    consumption
- `AI`
  - future producer only; it does not own logging
- `PROGRAM`
  - emits runtime load/start/stop/integrity events
- `PACKAGE`
  - emits install/remove/catalog events
  - does not own catalog logging or audit storage
- `UPDATE`
  - emits apply, activation, rollback, and recovery events

### Explicit Ownership Answers

- ordinary runtime logs are `OBSERVABILITY`-owned records
- security audit logs are `SECURITY`-owned audit classes consumed through
  `OBSERVABILITY`
- driver execution evidence is emitted by `DEVICE`, but the logging system and
  persistent query path remain under `OBSERVABILITY` sovereignty
- product-visible logs are consumed by `USER` and related product surfaces, but
  they are not sovereign there
- `PACKAGE`, `USER`, `PROGRAM`, `GRAPHICS`, `NETWORK`, and apps may emit facts,
  but they may not create a separate log system or private query model

## Log Classes

Logging System v1 formally distinguishes these classes:

- boot log
- lifecycle log
- system orchestration log
- driver and device log
- storage and data log
- security audit log
- crypto event log
- package log
- install log
- update log
- recovery log
- user and product-surface log
- query and LaSQL audit log

Each class uses the same record envelope and storage/query path. The class does
not authorize a private schema or private persistence system.

## Log Lifecycle

Logging System v1 defines four lifecycle forms:

- runtime log
  - memory-resident, stream-first, immediately consumable
- persistent log
  - durable `DATA` object under `/Logs`
- audit log
  - structured durable record whose visibility and retention are governed by
    `SECURITY`
- recovery or replay log
  - records required to justify rollback, repair, replay, reject, or fatal
    transitions

Retention rules:

- summary-only retention is allowed for repeated health beats, polling noise,
  and low-value steady-state repetition after an authoritative fold rule exists
- full structured retention is required for:
  - security and crypto decisions
  - installer, update, activation, and recovery decisions
  - boot and firsthop failure evidence
  - driver bind/load/fail evidence
  - storage integrity or write/read failure evidence
  - LaSQL governance audit
- compression or folding is allowed only after the sovereign class has defined
  the reduction rule and after required audit digest state is preserved

## LaSQL Relationship

Logs are not outside the database path. They are one of the first formal LaSQL
consumption domains.

- `OBSERVABILITY` exposes log query consumption
- `DATA` stores and indexes persistent structured log objects
- `SECURITY` governs who may query which log classes, which fields may be
  returned, and when only summaries or redactions may be shown
- `LaSQL` consumes the structured record model instead of parsing ad hoc text

At minimum, the following log fields are intended to be query-visible:

- tick
- band
- class
- kind
- space
- writer
- authority
- scope
- driver family
- device id
- session identity
- install identity
- trace identity

Structured attributes remain available for additional fields such as:

- namespace
- owner
- object reference
- target reference
- LBA
- driver family
- query target
- policy result

This keeps logs queryable as first-class system records rather than opaque text.

## Phase-One Landing Priority

The first three priority landing scenarios for Logging System v1 are:

1. boot, installer, and recovery chain evidence
2. driver, storage, input, and graphics failure evidence
3. security, crypto, and LaSQL governance audit
