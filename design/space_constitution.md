# LunaOS Space Constitution

This document defines the non-negotiable structural rules of LunaOS. It is stricter than `design/space_specs.md`.

LunaOS is not a central operating system split into modules. LunaOS is a constellation of 15 fixed spaces that are independent, cooperative, and connected only through explicit, security-reviewed connections.

## Core Law

- Every core system function belongs to one of the 15 fixed spaces.
- No additional core space may be introduced.
- A space must not silently absorb the responsibilities of another space.
- Cross-space action must happen through an explicit connection.
- `SECURITY` reviews and authorizes connections.
- `CID` is the permission to establish or hold a connection.
- `SEAL` is a one-shot proof for a sensitive action within a connection.
- `BOOT`, `SECURITY`, and `DEVICE` are the only spaces allowed to directly touch hardware during the current bring-up architecture.
- All other spaces must access machine capability through routes and connections, not direct hardware I/O.

## Fixed Space Set

These names and IDs are frozen:

1. `BOOT` `0`
2. `SYSTEM` `1`
3. `DATA` `2`
4. `SECURITY` `3`
5. `GRAPHICS` `4`
6. `DEVICE` `5`
7. `NETWORK` `6`
8. `USER` `7`
9. `TIME` `8`
10. `LIFECYCLE` `9`
11. `OBSERVABILITY` `10`
12. `AI` `11`
13. `PROGRAM` `12`
14. `PACKAGE` `13`
15. `UPDATE` `14`

`DIAG`, test payloads, sample apps, and bootstrap helpers are not spaces.

## Constitutional Model

LunaOS is built on four primary objects:

- `Space`
  - an independent system node with bounded responsibility
- `Connection`
  - an authorized collaborative relationship between spaces
- `CID`
  - a capability grant that permits a connection or bounded class of connections
- `SEAL`
  - an action proof for a sensitive mutation, launch, or escalation step

The system is not defined by resource ownership alone. It is defined by authorized relationships between spaces.

## Space Sovereignty

Each space has one primary sovereignty. A space may support another space, but it must not absorb another space's sovereignty.

### `BOOT`

Primary sovereignty: initial machine bring-up

May:
- enter the machine
- establish first memory and gate view
- load the initial constellation
- measure and hand off state

Must not:
- remain the permanent runtime coordinator
- issue long-lived runtime policy
- become the general service manager

### `SYSTEM`

Primary sovereignty: constellation coordination

May:
- arrange live topology
- assign resource envelopes
- request that spaces be raised or folded

Must not:
- issue capabilities
- override `SECURITY`
- directly own program execution
- directly own user session state

### `DATA`

Primary sovereignty: persistent object storage

May:
- store objects
- expose object metadata
- manage layout, mutation, and commit waves

Must not:
- decide who is authorized to access an object
- directly arbitrate policy
- directly own UI, session, or network semantics

### `SECURITY`

Primary sovereignty: security judgment

May:
- authenticate
- issue and revoke `CID`
- issue and consume `SEAL`
- validate claims, seals, and connection requests

Must not:
- become the resource scheduler
- become the storage manager
- become the package manager
- become a hidden execution supervisor

### `GRAPHICS`

Primary sovereignty: visible world composition

May:
- own surfaces, windows, scene state, and presentation
- control focus hints and visible layout

Must not:
- directly own session policy
- directly own program lifecycle
- directly persist user data

### `DEVICE`

Primary sovereignty: hardware mediation

May:
- abstract hardware lanes
- read and write physical devices
- translate raw machine interaction into typed routes

Must not:
- decide final authorization policy
- decide application behavior
- become a generic service bus for unrelated policy

### `NETWORK`

Primary sovereignty: external exchange

May:
- manage links, addresses, exchanges, and burst transport
- coordinate secure remote communication with `SECURITY`

Must not:
- become a package manager
- become user identity authority
- become a data policy engine

### `USER`

Primary sovereignty: human session and intent

May:
- own login state
- own workspace/session state
- translate human intent into connection requests

Must not:
- directly own hardware
- directly execute app images
- directly become the compositor
- become the security authority

### `TIME`

Primary sovereignty: time meaning

May:
- define pulse, epoch, timers, and drift interpretation

Must not:
- directly schedule all work by itself
- absorb `SYSTEM` or `LIFECYCLE`

### `LIFECYCLE`

Primary sovereignty: wake, rest, revive, fold

May:
- stage units
- wake them
- stop them
- revive them

Must not:
- decide security policy
- decide global resource placement
- become a package manager

### `OBSERVABILITY`

Primary sovereignty: evidence and diagnosis

May:
- collect traces
- cluster faults
- expose health views

Must not:
- silently control the system
- replace `SYSTEM`, `SECURITY`, or `LIFECYCLE`

### `AI`

Primary sovereignty: reasoning and planning

May:
- interpret intent
- explain system state
- propose actions
- operate within delegated capability fences

Must not:
- bypass `SECURITY`
- bypass `USER` session ownership
- become a hidden superuser
- directly control unrelated spaces without authorization

### `PROGRAM`

Primary sovereignty: application runtime

May:
- raise cells
- feed cells
- collect output
- route app-visible streams

Must not:
- directly own the display system
- directly own user session state
- directly issue security grants

### `PACKAGE`

Primary sovereignty: software intake and proof

May:
- verify packages
- stage install material
- maintain package catalog

Must not:
- directly launch runtime units on its own
- replace `UPDATE`
- replace `SECURITY`

### `UPDATE`

Primary sovereignty: system version transition

May:
- stage waves
- arm alternate targets
- rewind failed waves

Must not:
- become the package catalog
- become the daily app installer
- bypass `BOOT`, `SYSTEM`, or `SECURITY`

## Required Collaboration Chains

LunaOS features must emerge from chains of collaboration, not single-space shortcuts.

### Secure program launch

`USER -> SECURITY -> SYSTEM -> LIFECYCLE -> PROGRAM`

Optional visible path:

`PROGRAM -> GRAPHICS`

### Persistent document work

`USER -> PROGRAM -> SECURITY -> DATA`

Optional sync path:

`PROGRAM -> NETWORK -> SECURITY -> DATA`

### Desktop session

`USER -> GRAPHICS`

with applications participating through:

`USER -> PROGRAM -> GRAPHICS`

### Package install

`USER -> PACKAGE -> SECURITY -> DATA -> LIFECYCLE`

### Update wave

`UPDATE -> PACKAGE -> SECURITY -> DATA -> SYSTEM -> BOOT`

### AI-assisted action

`USER -> AI -> SECURITY -> target space`

AI may propose or invoke only within delegated fences.

## Forbidden Architectural Regressions

These are explicit failure modes. If implementation drifts into them, LunaOS is regressing.

- `SYSTEM` acting as a hidden security issuer
- `USER` absorbing window manager, program loader, and hardware roles at once
- `PROGRAM` writing directly to hardware or controlling global display state
- `AI` becoming an unchecked super-controller
- any non-foundation space silently performing direct hardware I/O
- long-lived hidden trust paths that bypass `SECURITY`
- adding new core spaces because boundaries were not respected

## Desktop Constitution

The LunaOS desktop is not a single space. It is a cooperation pattern:

- `USER`
  - session, launcher, focus intent, workspace state
- `GRAPHICS`
  - windows, surfaces, scene composition
- `PROGRAM`
  - app cells and app stream routing
- `DEVICE`
  - keyboard, pointer, display lanes
- `OBSERVABILITY`
  - visible failure and health explanation

Therefore:

- `USER` must not become the compositor.
- `GRAPHICS` must not become the session manager.
- `PROGRAM` must not become the desktop owner.

## Security Constitution

Security is not an add-on feature. It is the legality layer of the entire constellation.

- A connection must be reviewed before it is trusted.
- A capability must be explicit before it is used.
- A seal must exist before a sensitive action is accepted.
- Revocation must be possible after issuance.
- Users must be able to inspect meaningful parts of the active relationship graph.

This is the future role of `LunaGuard`:

- show active spaces
- show active connections
- show active capability grants
- show sensitive action proofs
- revoke or constrain relationships

## Product Meaning

LunaOS is not merely "an operating system with 15 components".

LunaOS is:

- 15 independent spaces
- cooperating through reviewed connections
- with security as the judge of legality
- with users able to understand and control meaningful parts of the live system graph

That is the core architectural promise. All implementation must serve it.
