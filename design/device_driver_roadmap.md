# LunaOS Device Driver Roadmap

This document defines the minimum self-developed driver path required to move
LunaOS from a bring-up prototype toward a commercial operating system.

It exists to stop a recurring failure mode: treating "boots in QEMU" as if it
were equivalent to "can operate real hardware". It is not.

If LunaOS is expected to become a user-facing and commercially credible system,
hardware enablement must be planned as a first-class product track rather than a
late compatibility task.

## Product Bar

Driver work is not allowed to optimize for:

- one-device demos
- QEMU-only success
- hardcoded device assumptions
- shell-script bring-up paths that bypass real device ownership

Driver work must instead establish these conditions:

- the system can discover hardware without manual source edits
- the system can recover from lane reset or device loss without rebooting the
  whole machine
- desktop interaction does not depend on debug-only input or serial fallbacks
- storage, input, display, and time all have explicit ownership inside the
  runtime
- regression coverage proves driver bring-up, failure handling, and persistence
  on supported hardware classes

## Why Drivers Are A Product Requirement

Without drivers, LunaOS can at best be a controlled demonstration. A commercial
desktop requires the system to own:

- interrupt delivery
- timer calibration
- block IO
- pointer and keyboard input
- display scanout and mode changes
- physical link control for networking
- power, reset, and error handling

The boot chain can start the machine. It cannot substitute for a real driver
stack.

## Current Reality

The repository currently has stronger bring-up and storage verification than
hardware ownership.

What exists today:

- BIOS and UEFI boot paths
- serial-driven validation
- persistent object storage through `lafs`
- visible desktop bring-up in QEMU

What is still missing as product-grade device work:

- a defined driver contract across `DEVICE`, `GRAPHICS`, `DATA`, `NETWORK`, and
  `TIME`
- explicit bus discovery and lane enumeration
- first-class input drivers suitable for a pointer-driven desktop
- first-class storage drivers suitable for durable installs
- display ownership that can grow beyond a fixed framebuffer path
- network drivers suitable for package and update flows

## Constitutional Ownership

### `DEVICE`

Must own:

- hardware discovery
- interrupt routing for device lanes
- DMA staging and reset behavior
- lane lifecycle for input, block, display, and link classes
- normalized event and order surfaces for the rest of the system

Must not own:

- desktop shell policy
- package update policy
- filesystem semantics
- application networking semantics

### `GRAPHICS`

Must own:

- compositor state
- display scene and scanout policy
- pointer presentation

Must not own:

- raw device enumeration
- keyboard or pointer hardware decoding
- bus-specific logic

### `DATA`

Must own:

- storage semantics
- object persistence
- filesystem integrity

Must not own:

- controller-specific block driver logic

### `NETWORK`

Must own:

- transport and secure channel semantics
- routing, presence, and exchange contracts

Must not own:

- NIC-specific register programming

### `TIME`

Must own:

- monotonic and wall-clock meaning
- timer contracts
- drift tracking

Must not own:

- board-specific timer driver logic outside the lane abstraction

## Driver Stack Shape

LunaOS driver work should converge on this stack:

1. discovery
2. lane binding
3. interrupt and DMA handling
4. normalized event/order contract
5. space-specific consumption

The normalized lane model is the key boundary:

- `DEVICE` turns hardware into typed lanes
- other spaces consume lanes without importing bus-specific knowledge

## Bring-Up Order

LunaOS should not attempt "all drivers" at once. Commercial progress comes from
doing the minimum viable hardware set in the right order.

### Phase 1: Platform discovery and timing

Required work:

- establish bus discovery for supported machine classes
- define lane census and stable lane identifiers
- own interrupt controller and timer calibration paths
- make `TIME` consume calibrated timer sources through `DEVICE`

Exit condition:

- the system can enumerate supported hardware lanes and maintain stable timing
  without serial-only assumptions

### Phase 2: Storage ownership

Required work:

- implement `disk0` against real controller families
- prioritize `AHCI`, then `NVMe`
- move block access assumptions behind a lane contract
- prove persistence, recovery, scrub, and repair through supported storage lanes

Exit condition:

- `lafs` and boot persistence run on supported storage drivers rather than only
  a narrow bring-up path

### Phase 3: Human input ownership

Required work:

- implement keyboard and pointer drivers for `input0`
- prioritize `PS/2` for baseline bring-up, then `USB HID`
- normalize click, move, wheel, and key events into a stable event stream
- remove dependence on shell-scripted interaction for desktop validation

Exit condition:

- the desktop can be operated through supported input lanes as a human-facing
  session rather than a scripted display surface

### Phase 4: Display ownership

Required work:

- formalize the current display path as a lane-backed scanout implementation
- keep framebuffer bring-up as the fallback baseline
- add mode-setting and adapter awareness as the next step after baseline scanout
- define the upgrade path toward accelerated rendering without coupling it to
  desktop shell policy

Exit condition:

- `GRAPHICS` consumes a display lane contract instead of relying on ad hoc fixed
  display assumptions

### Phase 5: Network ownership

Required work:

- implement baseline wired link support for `net0`
- prioritize one virtualized NIC family first, then expand
- allow `PACKAGE` and `UPDATE` to use `NETWORK` over a real physical link lane
- add fault reporting and reset behavior for degraded links

Exit condition:

- package fetch and update planning are no longer blocked on a stub networking
  layer

### Phase 6: Commercial device baseline

Required work:

- support lane reset and partial hardware loss without whole-session collapse
- define supported hardware classes explicitly
- add driver-specific regression suites for storage, input, display, and network
- record unsupported hardware clearly rather than failing ambiguously

Exit condition:

- LunaOS can state a support matrix and keep the session usable when one device
  class degrades

## Immediate Repository Priorities

The next concrete work should follow this order:

1. Freeze the lane contract names and discovery vocabulary in the design docs.
2. Define the first supported driver families for `disk0`, `input0`, `display0`,
   `clock0`, and `net0`.
3. Add driver-focused validation targets alongside desktop and persistence
   regressions.
4. Move desktop interaction validation away from scripted shell-only paths and
   toward real input-lane behavior.
5. Only after that, expand hardware support breadth.

## Non-Goals For This Phase

This document does not yet require:

- Wi-Fi
- Bluetooth
- audio
- camera
- touch
- GPU acceleration
- broad laptop power management

Those matter for a commercial product, but they are not the first blockers.
Storage, input, display, timing, and baseline networking come first.
