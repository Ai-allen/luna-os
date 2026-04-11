# LunaOS Desktop V1 Architecture

This document defines the first serious LunaOS desktop target.

It replaces the current bring-up fiction where a framebuffer demo, a launcher,
and a handful of sample apps are treated as if they were a desktop environment.
They are not. The current implementation is a proving rig.

The goal of Desktop V1 is not "look more like Windows or macOS". The goal is to
establish the minimum real architecture that makes a desktop operating system
possible:

- a persistent desktop session
- a composited visible workspace
- a stable input and focus model
- a real app window contract
- a theme model shared by shell and apps
- a package and data path that can sustain installed applications

## Product Bar

LunaOS is not allowed to optimize for "demo complete", "teaching clarity", or
"toy OS charm".

Desktop V1 exists to move the repository toward a commercial operating system.
That means every desktop decision must be judged against these bars:

- a user must be able to complete a task without learning internal implementation
  quirks
- the shell must feel stable and predictable across boots, apps, and input paths
- window behavior must be consistent enough that muscle memory can form
- apps must look like clients of one system, not five unrelated demos
- regression coverage must prove real user flows, not only boot logs

Any implementation that produces a visible desktop but fails those bars is not a
desktop milestone. It is still bring-up.

## Product Principles

Desktop V1 and all follow-on work must preserve these principles:

### 1. Task Before Ornament

The system exists to help a user launch, switch, edit, save, and recover work.
Wallpaper, effects, and novelty cannot displace task completion.

### 2. Direct Manipulation

Visible controls must behave like controls.

- clickable surfaces must advertise hover, press, focus, and disabled state
- pointer and keyboard actions must produce immediate visible feedback
- the system must not require the user to guess whether an action landed

### 3. Stable Spatial Model

Desktop, dock, launcher, windows, panels, and notifications must each have a
clear role. They are not interchangeable overlays.

### 4. Consistency Over Cleverness

Window close, minimize, restore, focus, drag, resize, menus, and launcher
selection must work the same way every time. Novel interaction is a product
liability unless it is measurably better.

### 5. One System Language

Theme, spacing, chrome, controls, typography, and motion must come from shared
contracts. First-party apps are not allowed to invent their own interaction
language.

### 6. Failure Must Stay Contained

App faults, repaint faults, bad package metadata, and persistence corruption must
degrade gracefully. A commercial desktop cannot behave like one process with one
global failure surface.

## Current Reality

The current system has these hard limits:

- `GRAPHICS` is a renderer with embedded shell policy.
- `USER` is both command shell and desktop interaction loop.
- apps are launched ad hoc and draw through tightly coupled assumptions.
- there is no theme contract, only hardcoded palette values.
- there is no durable workspace model.
- there is no app-facing UI contract beyond primitive window creation and cell writes.

That is why the system still feels far away from a modern desktop.

It also explains why the current system is still not acceptable as a product
surface even when regressions are green: it proves bring-up and storage paths
better than it proves human-operable workflows.

Desktop V1 also depends on hardware ownership. A visible shell without stable
input, storage, display, and timing lanes is still a prototype. Driver
requirements and bring-up order are defined in
[device_driver_roadmap.md](/abs/path/C:/Users/AI/lunaos/design/device_driver_roadmap.md).

## Desktop V1 Target

Desktop V1 is defined by six concrete outcomes:

1. A user can boot into a persistent `Luna Session` instead of a one-shot demo surface.
2. `GRAPHICS` composes a scene graph from shell and app state instead of owning desktop policy itself.
3. `USER` owns workspace policy, focus policy, launcher policy, and theme selection.
4. `PROGRAM` launches apps against a stable view contract rather than direct one-off window assumptions.
5. Apps can declare window role, preferred size, icon, and startup surface through package metadata.
6. Desktop appearance is driven by a named theme record stored in `DATA`, not by hardcoded colors in `GRAPHICS`.

## Constitutional Cut Lines

The following responsibilities are mandatory.

### `GRAPHICS`

Must own:

- compositor state
- window surfaces
- damage tracking
- hit testing
- scene presentation
- pointer shape and visible cursor

Must not own:

- launcher behavior
- app pinning
- start menu contents
- session policy
- theme selection policy
- package lookup logic

### `USER`

Must own:

- desktop shell state
- workspace state
- focused app intent
- launcher and dock policy
- keyboard shortcut policy
- theme selection policy
- session restore decisions

Must not own:

- pixel composition
- direct app image loading
- package storage
- hardware pointer decoding

### `PROGRAM`

Must own:

- launch contract for desktop apps
- app-to-window binding
- app runtime bookkeeping
- app role declarations

Must not own:

- desktop shell layout
- compositor policy
- persistent user data policy

### `PACKAGE`

Must own:

- installed app manifest catalog
- app icons and metadata lookup
- launcher-visible package enumeration

### `DATA`

Must own:

- desktop theme records
- workspace restore records
- recent documents and shell state objects

## Desktop V1 Runtime Contracts

Desktop V1 requires four contracts that are missing or only implicit today.

### 1. Scene Contract

Provider: `GRAPHICS`

Purpose:

- represent windows, z-order, visibility, damage, and frame roles without mixing
  in shell policy

Minimum fields:

- window id
- owner app/runtime id
- role
- bounds
- visible/minimized/maximized state
- focus state
- title buffer reference
- backing surface reference

### 2. Shell State Contract

Provider: `USER`

Purpose:

- persist and restore desktop-level state independent of composition

Minimum fields:

- active workspace
- pinned apps
- launcher open/closed state
- dock/task switcher ordering
- focused window id
- selected theme id

### 3. Theme Contract

Providers: `USER`, `DATA`

Consumers: `GRAPHICS`, apps

Purpose:

- make visual appearance data-driven instead of embedded in renderer code

Minimum fields:

- theme id
- wallpaper mode
- desktop background colors
- chrome colors
- text colors
- accent colors
- alert colors
- font metrics id
- corner radius scale
- shadow strength

### 4. View Contract

Providers: `PROGRAM`, `GRAPHICS`

Consumers: apps

Purpose:

- give apps a stable, system-owned way to request and update windows

Minimum operations:

- create top-level view
- create tool/panel/dialog view
- set title/icon
- resize request
- present cell or pixel buffer
- receive focus and pointer events
- close request

## Desktop V1 Package Model

Every desktop-visible app package must describe:

- app id
- display name
- icon object id
- launch target
- primary window role
- startup width and height
- permissions requested
- file/document capabilities requested
- whether the app is pinnable
- whether the app participates in session restore

This metadata belongs in `PACKAGE`, not hardcoded launcher arrays in `USER` or
`GRAPHICS`.

## Implementation Order

Desktop work should proceed in this order.

### Phase 1: Detach shell policy from composition

Required changes:

- move launcher contents and desktop selection state out of `GRAPHICS`
- keep `GRAPHICS` as compositor and window manager only
- define a shell-state structure owned by `USER`
- replace hardcoded desktop labels and icon positions with shell-provided data

Exit condition:

- `GRAPHICS` can render a desktop scene from state passed in, without embedding
  app names or launcher policy

### Phase 2: Introduce theme and workspace records

Required changes:

- add a `Luna Theme Contract`
- store active theme and workspace restore state in `DATA`
- let `USER` load theme state at session start
- make `GRAPHICS` render from theme colors instead of fixed palette constants

Exit condition:

- switching theme changes shell chrome and window chrome without code edits

### Phase 3: Normalize app launch and window binding

Required changes:

- move app catalog lookup fully into `PACKAGE`
- make `PROGRAM` bind launched apps to explicit window/view contracts
- remove hardcoded desktop app arrays from `USER`
- let shell populate launcher and dock from package metadata

Exit condition:

- a packaged app appears in the desktop because of package metadata, not because
  source code contains its name

### Phase 4: Real shell surfaces

Required changes:

- add dock/task rail state
- add launcher surface
- add system panel surface
- add desktop context actions
- add session restore on boot

Exit condition:

- LunaOS boots into a restorable desktop shell with pinned apps and a
  consistent workspace model

### Phase 5: App-facing UI baseline

Required changes:

- define standard input events
- define standard window roles
- define a shared widget baseline for text, lists, buttons, and document views
- add file open/save contract

Exit condition:

- desktop apps no longer feel like direct render demos; they become clients of a
  real UI contract

### Phase 6: Commercial interaction baseline

Required changes:

- validate full pointer flows for launcher, dock, titlebar, and panel controls
- validate keyboard flows for focus, switching, dismissal, and app-local input
- ensure at least one first-party app supports a real user task end to end
- separate shell transcript/debug output from user-facing desktop surfaces
- add regression cases that prove user workflows instead of only boot and store
  transcripts

Exit condition:

- LunaOS can demonstrate a user completing a desktop task through visible,
  repeatable interactions rather than through developer knowledge of hidden keys
  or scripts

## Immediate Repository Actions

The next concrete repository work should follow this checklist:

1. Remove launcher item ownership from `graphics/main.c`.
2. Introduce a shell-owned desktop model in `user/main.c`.
3. Define package metadata structures for desktop-visible apps.
4. Define theme/workspace records in the protocol headers.
5. Rework `list-apps` and launcher population to come from `PACKAGE`, not fixed arrays.
6. Add explicit user-workflow regression coverage for launcher, dock, focus, and
   app interaction.
7. Remove any remaining shell/debug transcript leakage from default desktop boot.

## Non-Goals For Desktop V1

Desktop V1 does not require:

- browser engine integration
- networking stack maturity
- multi-user accounts
- full font engine
- hardware acceleration
- POSIX compatibility

Those can come later. The current blocker is more basic: LunaOS needs a real
desktop architecture before it needs more desktop features.
