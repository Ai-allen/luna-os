# LunaOS Driver Architecture v1

## Status

This document defines the formal driver architecture for LunaOS as of
`2026-04-11`.

It is an architecture and contract document. It does not introduce a broad new
driver set, and it does not turn LunaOS into a generic kernel-style
"anyone can touch hardware" model.

## Driver Authority

Drivers are part of `DEVICE` space authority.

This means:

- hardware discovery and binding originate from `DEVICE`
- raw register, DMA, MMIO, PIO, IRQ, and link access do not belong to app or
  general runtime callers
- other spaces consume capabilities projected by `DEVICE`, not raw hardware
  ownership

No normal path allows `USER`, `PROGRAM`, `PACKAGE`, `UPDATE`, or third-party
   payloads to bypass `DEVICE` and touch hardware directly.

## Formal Layering

### 1. Boot-Critical Drivers

These are the minimum drivers required for LunaOS to leave firmware/loader
handoff and reach system bring-up.

They include only hardware paths required for:

- boot medium access
- minimal console or framebuffer output
- minimal input if required for recovery or shell
- timer/clock
- interrupt or polling path required for basic bring-up

Boot-critical drivers are loaded on the BOOT -> DEVICE bring-up path and are
part of the startup contract.

Failure semantics:

- fatal when they block loader handoff, system volume access, or minimum user
  recovery surface
- degraded when a redundant path exists and LIFECYCLE can continue in reduced
  mode

### 2. Core Platform Drivers

These drivers are required for a stable LunaOS platform after BOOT hands off
control.

They include:

- storage controller drivers used by system/data volumes
- persistent input/display paths needed for shell or desktop
- platform bus and topology discovery
- baseline network adapter support when network is part of the active system
  promise

Core platform drivers are managed by `DEVICE`, approved by `SECURITY`, and
phased by `LIFECYCLE`.

Failure semantics:

- fatal when they block `LSYS/LDAT` access, boot continuation, or minimum
  recovery shell
- degraded when they only block optional surfaces such as richer graphics or
  non-required networking

### 3. Optional Device Drivers

These drivers extend available hardware coverage without redefining the minimum
bootable system.

Examples:

- extra NIC families outside the current minimum platform
- richer graphics paths beyond the minimum console/framebuffer contract
- non-required sensors, secondary inputs, or peripherals

Failure semantics:

- never fatal by default
- degraded only if an active policy, install profile, or user-selected mode
  declared them required
- otherwise recorded as observation-only failure

### 4. Future Third-Party Driver Model

Third-party drivers are not part of the minimum system trust boundary today,
but the architecture reserves an explicit model for them.

The future model keeps these hard rules:

- third-party drivers remain `DEVICE`-scoped runtime units, not arbitrary
  kernel blobs
- they enter through signed, governed distribution
- they receive narrower capability envelopes than system-supplied drivers
- they can be denied, quarantined, or rollbacked without redefining platform
  authority

Third-party drivers are out of scope for the initial runtime delivery, but
their architecture boundary is frozen here.

## Distribution and Runtime Model

### Distribution Unit

Drivers use `.luna` as the distribution unit.

Reason:

- LunaOS already freezes `.luna v2` as the install/update-visible ecosystem
  unit
- signer/source trust chain, audit chain, and update orchestration already
  exist on that unit
- introducing a separate foreign driver package format would fragment trust,
  update, and recovery behavior

### Runtime Unit

Driver runtime payloads use `.la` as the executable payload format, but they do
not share the same privilege envelope as normal apps.

Common contract with normal apps:

- `.luna` distribution
- `.la` runtime header
- ABI versioning
- source/signer trust chain
- audit visibility

Different contract from normal apps:

- driver `.la` is marked as `DEVICE`-domain runtime, not general app runtime
- load/start requires driver-specific capabilities and policy approval
- writable operations require `SECURITY` approval and `DEVICE` authority
- driver `.la` can be admitted only into allowed system modes
- driver failure feeds into boot/lifecycle state, not just app-session failure

### System-Supplied vs Third-Party

System-supplied drivers:

- ship in `LSYS`
- participate in boot-critical and core platform binding
- are part of the current system image and update activation

Future third-party drivers:

- also distributed as `.luna`
- staged through the same signer/source trust chain
- loaded only when the system policy allows external driver admission
- cannot become boot-authoritative unless a future policy/profile explicitly
  allows it

## Load and Probe Contract

### BOOT Participation

`BOOT` does not own drivers, but it does own early handoff and minimum device
intent.

`BOOT` responsibilities:

- expose install identity, profile, and activation context
- identify minimum boot-critical hardware requirements
- hand off boot-visible device intent to `SYSTEM`, `DEVICE`, and `SECURITY`
- refuse continuation when the minimum driver contract for the current profile
  cannot be satisfied

### DEVICE Participation

`DEVICE` is the authoritative driver orchestrator.

`DEVICE` responsibilities:

- probe buses and discover candidate devices
- classify each device against boot-critical, core, optional, or observer-only
  policy
- select a driver candidate set
- request load/start and write privileges through LunaLink
- expose projected capabilities to consuming spaces

### SECURITY Participation

`SECURITY` is the hard gate for:

- driver admission
- privileged hardware write access
- DMA/MMIO/PIO/IRQ capability approval
- writable transition into driver-controlled persistent state
- replay/reject decisions when driver state recovery is needed
- quarantine, deny, readonly, or recovery downgrade decisions

No driver may become active merely because `DEVICE` found hardware.

### SYSTEM and LIFECYCLE Participation

`SYSTEM` consumes the resulting platform capability graph.

`LIFECYCLE` controls:

- boot phase ordering
- when a missing driver is fatal, degraded, or observational
- retry, quarantine, rollback, and recovery transitions
- whether later spaces may start after a driver outcome
- stable publication of `DEVICE` lifecycle flags before later product spaces
  consume the platform

`SYSTEM` must consume the published `DEVICE` lifecycle state to:

- mark device capability as active vs degraded in the platform graph
- distinguish degraded continuation from recovery-required bring-up
- apply storage recovery actions through `SECURITY`-governed mount floors before
  `DATA` replay/repair/write paths begin
- publish degraded downstream space state when display/input/network/storage
  fallback actions are active
- stop later product-space expansion when the published driver state requires
  recovery

### OBSERVABILITY Participation

`OBSERVABILITY` records:

- probe start and result
- chosen driver identity
- selection basis and normalized firmware-handoff / PCI evidence for the
  chosen path
- structured blocker classification and recovery hint for the chosen driver
  family
- execution summary for recovery actions that `SYSTEM` actually applied
- approval or denial by `SECURITY`
- lifecycle consequence for the chosen outcome
- activation success
- degraded continuation
- rollback, quarantine, reject, and recovery events

### Failure Classification

Fatal:

- storage driver failure blocking `LSYS/LDAT`
- display/input failure when the active profile requires minimum shell/recovery
- driver set identity mismatch that breaks boot-critical contract

Degraded:

- non-primary display path unavailable
- non-required NIC unavailable
- optional peripheral unavailable while minimum system path still works
- storage continuing on firmware-only or legacy fallback
- display continuing on text-only or firmware-only framebuffer fallback
- input continuing on legacy-only keyboard path
- network continuing on loop-only path without external stack bring-up

Observation-only:

- device present but outside current support matrix
- optional device denied by policy without affecting current profile

## Driver Sovereignty Matrix

### DEVICE

Authoritative owner of:

- hardware inventory objects
- device binding objects
- driver candidate and active-set records
- runtime device state projections
- low-level access execution

### SECURITY

Formal adjudication boundary:

- whether a driver may load
- whether a driver may write hardware state
- whether a driver may enter writable/persistent mode
- whether driver state may be replayed after crash
- whether a driver failure escalates volume/system state
- whether third-party or optional driver admission is allowed
- whether blocker-class facts force degraded or recovery-required state even
  when a bind is otherwise approved

### DATA

`DATA` stores, at minimum:

- probe snapshots
- binding results
- binding evidence including selection basis, approval source, normalized
  firmware-handoff snapshots, and PCI facts
- blocker flags and recovery hints for each governed driver result
- recovery-action flags and resulting health-floor snapshots for each governed
  driver result
- lifecycle consequence snapshots for each governed driver result
- lifecycle-flag snapshots that explain which family-specific fallback path was
  active at bind time
- selected driver-set identity
- driver health and failure history
- rollback markers and recovery state

`DATA` does not grant hardware authority. It only persists governed driver
state.

When `SYSTEM` applies a `SECURITY`-approved storage recovery floor, `DATA` must
honor it before replay, scrub, format, or metadata writeback and continue only
within the resulting readonly or recovery mode.

While that floor is active, `DATA` must also execute layered storage
verification before continuing normal object semantics:

- metadata/super and object-record contract checks
- txn-log and txn-aux readability checks
- minimal live-slot read verification for persisted objects

If those checks still show storage residue or unreadable regions, `DATA` must
escalate the governed result to degraded or recovery-required instead of
silently continuing under a stale readonly floor.

`SYSTEM` and `UPDATE` must route installer or update storage failures that
touch `LSYS` / `LDAT` / `LRCV` back through the same `SECURITY`-governed
recovery boundary instead of leaving them as local-only deny strings.

When `BOOT` or `DATA` has already established storage-rooted fatal or
`recovery-required` state for `LSYS` / `LDAT` contract or integrity failure,
`SYSTEM` must keep that root cause visible as `storage gate=fatal` or
`storage gate=recovery` and stop before `PACKAGE` / `UPDATE` / `USER`
expansion instead of collapsing it into a generic driver gate.

### OBSERVABILITY

`OBSERVABILITY` owns:

- driver event logs
- boot/load/failure/rollback audit trails
- degraded-mode records
- recovery traces
- blocker-aware audit evidence that explains why a family continued in
  degraded, fallback, or recovery-required state
- short recovery-action summaries emitted when `SYSTEM` applies storage/input/
  display/network recovery actions

### USER / PROGRAM / NETWORK / GRAPHICS

These spaces consume driver-provided service surfaces but do not own the
drivers:

- `USER` consumes device availability and degraded-status projections
- `PROGRAM` consumes abstract device capabilities exposed by policy
- `NETWORK` consumes NIC/link capability from `DEVICE`, not raw NIC ownership
- `GRAPHICS` consumes display/input capability from `DEVICE`, not raw scanout
  ownership
- when `SYSTEM` publishes degraded device state and `LIFECYCLE` carries input
  recovery actions, `USER` must gate pointer-rich desktop interaction, keep
  only the minimal keyboard path that remains valid, and admit operator-shell
  recovery on the approved serial-backed path instead of inventing a new input
  owner
- when `SYSTEM` publishes degraded device state and `LIFECYCLE` carries display
  recovery actions, `GRAPHICS` must collapse firmware-framebuffer paths to a
  safe text-only projection instead of continuing richer desktop composition

## Update and Rollback Model

### Volume Placement

Driver binaries and authoritative driver manifests belong in `LSYS`.

Mutable records belong in `LDAT`, including:

- probe history
- active binding results
- blocker and recovery-hint evidence for each active binding
- health/degraded markers
- rollback and recovery markers

### Update Path

Driver updates are part of `update.apply`.

This means:

- new driver set is staged as part of the candidate system activation
- activation marker refers to a system image containing a specific driver set
- driver changes do not bypass the existing `LSYS` activation contract

### Rollback Path

If a driver update causes boot failure or violates the minimum boot-critical
contract:

- `BOOT` and `SECURITY` must reject normal continuation for the failed driver
  set
- `LIFECYCLE` enters recovery or fallback activation
- the previous driver set tied to the previous `LSYS` activation becomes the
  rollback target

Driver rollback is therefore a system-image rollback first, not an ad hoc
"replace one blob in place" model.

### Installer and Recovery Contract

Installer, recovery, and update activation must all use the same contract:

- install identity
- system mode
- volume-state
- signer/source trust
- `SECURITY` approval
- observable driver-set selection and failure records

### Physical Bring-Up Minimum Contract

For physical-machine bring-up, the minimum driver contract is:

- one boot-capable storage path
- one minimum display path or serial-equivalent recovery path
- one minimum input path for shell/recovery when interactive bring-up is required
- one timer/clock path
- one platform bus discovery path sufficient to identify required controllers

Network is not inherently boot-critical unless the active profile explicitly
requires it.

## First-Phase Priority Drivers

The first implementation phase should prioritize these five driver classes:

1. storage boot drivers:
   `UEFI Block I/O`, `AHCI SATA`, and the system volume path
2. display bring-up drivers:
   `UEFI GOP` and minimum framebuffer/console projection
3. input bring-up drivers:
   current keyboard path plus USB input-controller candidate evidence for the
   future USB-HID bring-up path; `usb-hid=not-bound` remains non-support
   evidence until a real HID keyboard driver is bound and reviewed; current
   USB-HID blocker evidence is `controller-driver-missing`, before USB
   enumeration, HID descriptor parsing, interrupt polling, keycode mapping, or
   `INPUT0` delivery
4. platform discovery drivers:
   PCI / controller inventory and binding discovery
5. baseline network drivers:
   current minimum Intel-compatible bring-up path for observation and later
   platform continuity
