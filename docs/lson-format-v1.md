# LunaOS LSON Format v1

## Status

This document defines `LSON` as of `2026-04-18`.

`LSON` stands for `Luna Structured Object Notation`.

It is LunaOS's native structured record format for logs, audit records,
installer evidence, driver evidence, system events, and query-visible event
state.

## Position

`LSON` is not a direct JSON clone. It is the LunaOS-native record model shared
by:

- `BOOT`
- `DEVICE`
- `SECURITY`
- `OBSERVABILITY`
- `DATA`
- `PACKAGE`
- `UPDATE`
- `USER`
- `LaSQL` log and audit query

`LSON` has one logical field model with two encodings:

- `LSON-T`
  - canonical text form for serial streams, operator-facing logs, and readable
    export
- `LSON-F`
  - canonical framed form for persistent storage, audit-chain retention, and
    future binary-efficient paths

The field model is the same in both forms. This avoids one text-only schema for
developers and another schema for the real system.

## Record Model

The minimum `LSON` record model is:

- `tick`
  - LunaOS time or ordered stamp for the record
- `band`
  - severity and urgency band
- `class`
  - broad record class such as boot, device, security, or query
- `kind`
  - stable event kind within the class
- `space`
  - emitting space
- `writer`
  - writer authority space
- `authority`
  - final governing authority for the event
- `scope`
  - subsystem or execution surface
- `body`
  - concise human-readable summary
- `attrs`
  - typed structured attributes
- `trail`
  - correlation identities such as trace, session, install, or actor identity

Common extension attributes include:

- namespace
- owner
- type
- state
- object
- target
- LBA
- device id
- driver family
- policy result
- query target
- signer
- source

## Encoding Model

### LSON-T

`LSON-T` is a one-record-per-line text form. A canonical readable line looks
like this:

```text
@lson/1 tick=0x0000000000000042 band=info class=boot kind=boot.lsys.super.ok space=BOOT writer=BOOT authority=BOOT scope=pre-device body='lsys super read ok' attrs(lba:u64=0x00004800,pair:tag=ok,handoff:tag=missing) trail(install:id128=840148853ECAC1DE.DB064C5008865D11)
```

Properties of `LSON-T`:

- one record per line
- stream-friendly
- machine-parseable without JSON token rules
- stable field order
- typed attributes
- readable on serial and console paths

### LSON-F

`LSON-F` is a one-record-per-frame structured encoding backed by the same field
model.

Properties of `LSON-F`:

- fixed header for hot query fields
- append-friendly for persistent log objects
- ready for audit hashing, compression, and binary transport
- better suited to `DATA` persistence and `LaSQL` indexing than arbitrary text

## Why LSON Instead Of JSON

`LSON` exists because LunaOS needs more than a generic text object format:

- LunaOS records must carry fixed identity and governance fields without every
  caller inventing its own schema
- native fields such as install identity, session identity, driver family, LBA,
  and governance authority should not degrade into untyped string blobs
- audit and crypto paths need a deterministic canonical order for hashing and
  replay evidence
- serial readability and durable framed storage both matter, so one logical
  model must support both

`LSON` therefore relates to `JSON` and `JSON Lines` like this:

- it serves a similar role for structured records
- it is not a syntax copy
- it is not a "JSON string inside a log line"
- it uses LunaOS-native field names, typed atoms, and governed record classes

## ABI Shape

The first shared ABI landing for `LSON` is:

- `struct luna_lson_record`
- `struct luna_lson_attr`

inside:

- [`include/luna_proto.h`](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h)
- [`include/luna_proto.rs`](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs)

This is the formal system entrance for structured logging. Current text-only
record projection can continue as a compatibility view, but it is no longer the
architectural end state.

## Examples

### BOOT Success Chain

```text
@lson/1 tick=0x0000000000000009 band=info class=boot kind=boot.native.pair.ok space=BOOT writer=BOOT authority=BOOT scope=pre-device body='native pair ok' attrs(lsys_lba:u64=0x00004800,ldat_lba:u64=0x00008800,source:tag=fwblk) trail(install:id128=840148853ECAC1DE.DB064C5008865D11)
```

### DEVICE Driver Failure Evidence

```text
@lson/1 tick=0x0000000000000301 band=error class=device kind=device.disk.write.fail space=DEVICE writer=DEVICE authority=SECURITY scope=piix-ide body='block write failed' attrs(device:u64=0x00000002,driver:tag=piix-ide,lba:u64=0x00008810,stage:tag=flush-complete,status:u64=0x0000E002) trail(trace:id128=0000000000000001.0000000000000009)
```

### SECURITY Audit Event

```text
@lson/1 tick=0x0000000000000440 band=audit class=security kind=security.auth.session.issue space=SECURITY writer=SECURITY authority=SECURITY scope=session body='session issued' attrs(user:tag=dev,result:tag=allow) trail(session:id128=0000000000001000.0000000000000001,install:id128=840148853ECAC1DE.DB064C5008865D11)
```

### Crypto Secret Request

```text
@lson/1 tick=0x0000000000000441 band=audit class=crypto kind=crypto.secret.release space=SECURITY writer=SECURITY authority=SECURITY scope=credential body='secret released' attrs(key_class:tag=user-auth,policy:tag=allow,sealed:tag=install-bound) trail(session:id128=0000000000001000.0000000000000001)
```

### LaSQL Query Audit

```text
@lson/1 tick=0x0000000000000502 band=audit class=query kind=lasql.query.logs space=OBSERVABILITY writer=OBSERVABILITY authority=SECURITY scope=lasql body='logs query allowed' attrs(target:tag=observe.logs,projection:tag=summary,limit:u64=0x00000020,result:u64=0x00000012) trail(session:id128=0000000000001000.0000000000000001,trace:id128=0000000000000001.0000000000000101)
```

### Installer Target Binding / Write / Boot

```text
@lson/1 tick=0x0000000000000100 band=info class=install kind=install.target.bound space=UPDATE writer=UPDATE authority=SECURITY scope=installer body='target bound' attrs(target:tag=disk1,system_lba:u64=0x00004800,data_lba:u64=0x00008800,writer:tag=installer-formal) trail(install:id128=840148853ECAC1DE.DB064C5008865D11)
@lson/1 tick=0x0000000000000109 band=info class=install kind=install.layout.write.ok space=DATA writer=DATA authority=SECURITY scope=installer body='layout written' attrs(esp:tag=ok,lsys:tag=ok,ldat:tag=ok,lrcv:tag=ok) trail(install:id128=840148853ECAC1DE.DB064C5008865D11)
@lson/1 tick=0x0000000000000201 band=info class=boot kind=boot.installed.target.online space=BOOT writer=BOOT authority=BOOT scope=installed-target body='installed target online' attrs(mode:tag=normal,first_setup:tag=required) trail(install:id128=840148853ECAC1DE.DB064C5008865D11)
```

### USER/Desktop Product Event

```text
@lson/1 tick=0x0000000000000600 band=info class=user kind=user.desktop.window.open space=USER writer=USER authority=USER scope=desktop body='window opened' attrs(app:tag=Files,window:u64=0x00000003,focus:tag=active) trail(session:id128=0000000000001000.0000000000000001)
```
