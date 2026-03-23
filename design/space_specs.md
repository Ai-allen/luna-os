# LunaOS v2.0 Space Specifications

This document defines the first complete functional specification for the 15 core spaces of LunaOS v2.0. LunaOS is a space-native, capability-driven operating system. No space is privileged by convention alone; authority flows only through capability issuance by `SECURITY` and explicit gate contracts between spaces.

Common terms used below:

- `CID`: 128-bit capability identifier issued by `SECURITY`.
- `gate`: a shared-memory IPC region with request and response lanes.
- `span`: a bounded memory extent shared for a single transaction.
- `ticket`: a short-lived operation handle returned by an asynchronous gate.
- `space seal`: an integrity record proving a loaded space image matches an expected digest and policy.
- `route`: a named IPC endpoint exposed by a space.

All interfaces in this document are original LunaOS routes. Names are deliberately not modeled after POSIX syscalls, Win32 APIs, or existing microkernel message verbs.

## 0. BOOT

### Space responsibilities

- Enter from BIOS, establish the first execution environment, and construct the initial LunaOS machine view.
- Measure the image, build the initial manifest map, and launch the first service constellation.
- Create the first shared gates and reserve memory spans for early coordination.
- Transfer authority to `SECURITY` and `SYSTEM` without retaining general runtime control.

### IPC routes provided

- `boot.handoff(opening_view) -> boot_handoff_record`
  - Parameters:
    - `opening_view`: physical memory ranges, stage layout, serial route, image digest.
  - Returns:
    - `boot_handoff_record`: gate addresses, reserve spans, launch transcript checksum.
- `boot.echo_probe(probe_word) -> probe_word`
  - Debug-only route for early bring-up; removed from release profiles.

### Required capabilities

- None at power-on.
- May later accept `cid.boot.observe` from `OBSERVABILITY` for post-boot reports.

### Typical interaction flows

- `BOOT -> SECURITY`: pass measured image digest and initial launch ledger.
- `BOOT -> SYSTEM`: pass space catalog and machine resource ledger.
- `BOOT -> TIME`: optionally seed monotonic epoch source once timer space exists.

### Internal data structures

- `LunaLaunchLedger`
  - Image digest
  - Space offsets
  - Reserved spans
  - Launch events
- `LunaSpanRecord`
  - Base
  - Size
  - Purpose
  - Share policy
- `LunaBootView`
  - Serial route
  - Gate map
  - Reserve map
  - Machine fingerprint

## 1. SYSTEM

### Space responsibilities

- Coordinate machine-wide resource placement and space lifecycle intent.
- Maintain the live constellation table: which spaces exist, which are paused, degraded, or restarting.
- Allocate address windows, gate slots, and execution budgets to spaces.
- Act as the non-authoritative control fabric: it orchestrates, but cannot overrule `SECURITY`.

### IPC routes provided

- `system.raise_space(space_recipe, launch_policy) -> space_handle`
  - Parameters:
    - `space_recipe`: image reference, memory envelope, route manifest.
    - `launch_policy`: restart mode, exposure mode, scheduling weight.
  - Returns:
    - `space_handle`: stable runtime handle for the new space.
- `system.fold_space(space_handle, reason_code) -> fold_status`
  - Gracefully stop or quarantine a space.
- `system.bind_route(space_handle, route_decl) -> bind_receipt`
  - Registers a new public or restricted route.
- `system.sample_constellation(cursor) -> constellation_slice`
  - Enumerates live spaces and health states.

### Required capabilities

- `cid.system.raise`
- `cid.system.fold`
- `cid.system.bind`
- `cid.system.inspect`

### Typical interaction flows

- `LIFECYCLE -> SYSTEM`: raise an app runtime space after installation.
- `UPDATE -> SYSTEM`: request staged A/B activation.
- `OBSERVABILITY -> SYSTEM`: inspect live topology for health maps.
- `SYSTEM -> SECURITY`: validate that a launch recipe is authorized before materializing it.

### Internal data structures

- `ConstellationTable`
  - `SpaceHandle`
  - Space ID
  - Health color
  - Restart epoch
  - Route index pointer
- `LaunchRecipe`
  - Image object ID
  - Memory envelope
  - Route manifest
  - Capability demand list
- `BudgetFrame`
  - CPU weight
  - Memory ceiling
  - IO credits

## 2. DATA

### Space responsibilities

- Provide Luna Attribute File System (`lafs`), the persistent object substrate for all higher spaces.
- Store objects, metadata, policy labels, and history anchors.
- Enforce capability-gated read/write/delete/list semantics.
- Offer transactional write sessions instead of implicit mutable file handles.

### IPC routes provided

- `data.seed_object(cid, object_profile) -> object_ref`
  - Creates a new object shell.
- `data.pour_span(cid, object_ref, write_plan, span_ref) -> write_receipt`
  - Writes bytes from a shared span into an object extent.
- `data.draw_span(cid, object_ref, read_plan) -> span_ticket`
  - Returns a read ticket that maps a response span.
- `data.shred_object(cid, object_ref) -> shred_status`
  - Tombstones or removes an object according to policy.
- `data.gather_set(cid, filter_cursor) -> object_slice`
  - Lists objects by attribute filters.
- `data.commit_wave(cid, session_ticket) -> commit_receipt`
  - Finalizes a staged mutation wave.

### Required capabilities

- `cid.data.seed`
- `cid.data.pour`
- `cid.data.draw`
- `cid.data.shred`
- `cid.data.gather`
- `cid.data.commit`

### Typical interaction flows

- `PROGRAM -> SECURITY`: request object read/write CIDs scoped to an app domain.
- `PROGRAM -> DATA`: seed and mutate app data objects.
- `PACKAGE -> DATA`: store verified package payloads and manifests.
- `UPDATE -> DATA`: materialize alternate system image objects for A/B rollout.

### Internal data structures

- `LafsSuperframe`
  - Format epoch
  - Object table root
  - Free-wave root
  - Journal frontier
- `LafsObjectCell`
  - Object UUID
  - Attribute mask
  - Content length
  - Layout root
  - Policy label
- `LafsWaveRecord`
  - Session ticket
  - Touched extents
  - Previous root
  - Commit checksum
- `LafsExtentLeaf`
  - Logical offset
  - Physical run
  - Span length
  - Flags

## 3. SECURITY

### Space responsibilities

- Operate as the capability root of LunaOS.
- Authenticate spaces, mint CIDs, revoke CIDs, and bind CIDs to policy.
- Verify space seals, package signatures, launch claims, and delegated authority chains.
- Maintain the authority ledger for the entire constellation.

### IPC routes provided

- `security.ask_cap(cap_path, claim_context) -> cid_grant`
  - Mints or denies a capability.
- `security.show_caps(space_handle, cursor) -> cid_slice`
  - Lists live grants for a subject.
- `security.cut_cap(cid, revoke_reason) -> revoke_receipt`
  - Revokes a CID or CID family.
- `security.prove_space(space_handle) -> seal_report`
  - Returns current integrity and identity proof.
- `security.bind_identity(identity_blob) -> identity_receipt`
  - Associates a user, package, or machine identity with a policy domain.

### Required capabilities

- None for the space itself as the root issuer.
- Administrative routes may require internal sealed control tokens that never leave the space.

### Typical interaction flows

- `BOOT -> SECURITY`: initial measured handoff.
- `SYSTEM -> SECURITY`: ask whether a space recipe may be launched.
- `USER -> SECURITY`: request login session delegation.
- `PROGRAM -> SECURITY`: obtain scoped CIDs for storage, network, AI, or graphics use.

### Internal data structures

- `AuthorityLedger`
  - Subject handle
  - Grant family
  - Scope fence
  - Revocation epoch
- `SealRecord`
  - Space digest
  - Launch digest
  - Policy profile
  - Confidence flags
- `ClaimContext`
  - Caller space
  - Declared intent
  - Resource domain
  - Time fence

## 4. GRAPHICS

### Space responsibilities

- Own all visible composition on the machine.
- Maintain scenes, layers, surfaces, input focus hints, and presentation clocks.
- Isolate GPU command generation behind a space boundary so applications never touch display hardware directly.
- Support both 2D composition and future AI-assisted rendering hints.

### IPC routes provided

- `graphics.cast_surface(cid, surface_recipe) -> surface_handle`
  - Creates a drawable surface.
- `graphics.tint_surface(cid, surface_handle, paint_packet) -> paint_receipt`
  - Writes draw commands or pixel spans.
- `graphics.weave_scene(cid, scene_delta) -> scene_receipt`
  - Adds, removes, or reorders visual nodes.
- `graphics.focus_hint(cid, target_surface, hint_kind) -> focus_receipt`
  - Suggests focus transitions.
- `graphics.read_frame(cid, sample_rect) -> frame_span_ticket`
  - Captures a frame region for screenshots or AI analysis.

### Required capabilities

- `cid.graphics.surface.cast`
- `cid.graphics.surface.tint`
- `cid.graphics.scene.weave`
- `cid.graphics.focus.hint`
- `cid.graphics.frame.read`

### Typical interaction flows

- `USER -> GRAPHICS`: construct shell chrome and session scene root.
- `PROGRAM -> GRAPHICS`: request surfaces and submit paint packets.
- `DEVICE -> GRAPHICS`: deliver display hotplug or mode hints.
- `AI -> GRAPHICS`: ask for sampled frame spans when allowed.

### Internal data structures

- `SceneWeave`
  - Node list
  - Z braid
  - Visibility mask
  - Damage regions
- `SurfacePoolEntry`
  - Surface handle
  - Backing span
  - Color model
  - Present fence
- `FramePulse`
  - Display target
  - Vsync epoch
  - Damage slice

## 5. DEVICE

### Space responsibilities

- Abstract physical devices into Luna-native streams and command routes.
- Normalize keyboards, pointers, block devices, network links, displays, cameras, and sensors into route families.
- Own low-level interrupts, DMA staging spans, and device resets.
- Publish capability-scoped device slices instead of a flat device namespace.

### IPC routes provided

- `device.open_lane(cid, lane_recipe) -> lane_handle`
  - Opens a typed device lane such as input, block, link, or display control.
- `device.push_order(cid, lane_handle, order_packet) -> order_receipt`
  - Sends device-specific commands.
- `device.pull_event(cid, lane_handle, cursor) -> event_slice`
  - Reads normalized device events.
- `device.describe_lane(cid, lane_handle) -> lane_profile`
  - Returns immutable and mutable properties.
- `device.reset_lane(cid, lane_handle) -> reset_receipt`
  - Reinitialize a failing device lane.

### Required capabilities

- `cid.device.lane.open`
- `cid.device.order.push`
- `cid.device.event.pull`
- `cid.device.lane.describe`
- `cid.device.lane.reset`

### Typical interaction flows

- `GRAPHICS -> DEVICE`: acquire display scanout and input lanes.
- `NETWORK -> DEVICE`: acquire physical link lanes.
- `DATA -> DEVICE`: acquire block IO lanes.
- `OBSERVABILITY -> DEVICE`: inspect fault statistics on a device lane.

### Internal data structures

- `LaneRegistry`
  - Lane handle
  - Device class
  - Share mode
  - Fault state
- `EventRibbon`
  - Sequence head
  - Event cells
  - Overflow flag
- `DmaSpanTicket`
  - Physical span
  - Direction
  - Completion epoch

## 6. NETWORK

### Space responsibilities

- Provide the Luna-native networking stack: `lanet` for transport and `latls` for secure channels.
- Manage addresses, links, sessions, routing hints, and stream protection policies.
- Export application-friendly exchange routes instead of raw sockets.
- Coordinate with `DEVICE` for physical links and with `SECURITY` for trust material.

### IPC routes provided

- `network.open_exchange(cid, exchange_recipe) -> exchange_handle`
  - Creates a communication exchange.
- `network.send_burst(cid, exchange_handle, span_ref, burst_meta) -> burst_receipt`
  - Sends one burst of payload.
- `network.catch_burst(cid, exchange_handle, cursor) -> burst_ticket`
  - Reads inbound bursts into a mapped span.
- `network.bind_presence(cid, bind_recipe) -> presence_receipt`
  - Publishes a local service presence.
- `network.trace_route(cid, trace_recipe) -> trace_report`
  - Returns Luna route diagnostics.

### Required capabilities

- `cid.network.exchange.open`
- `cid.network.burst.send`
- `cid.network.burst.catch`
- `cid.network.presence.bind`
- `cid.network.trace`

### Typical interaction flows

- `PROGRAM -> NETWORK`: open exchanges for app communication.
- `PACKAGE -> NETWORK`: fetch package indexes and payloads.
- `UPDATE -> NETWORK`: retrieve signed update waves.
- `NETWORK -> SECURITY`: validate trust anchors and secure channel claims.

### Internal data structures

- `ExchangeTable`
  - Exchange handle
  - Remote identity
  - Flow policy
  - Security posture
- `BurstEnvelope`
  - Exchange handle
  - Span ref
  - Reliability class
  - Deadline hint
- `PresenceBloom`
  - Service route
  - Scope
  - Lease epoch

## 7. USER

### Space responsibilities

- Hold the current user session, login state, shell state, workspace model, and desktop persona.
- Translate human intent into capability-scoped space actions.
- Manage session-scoped delegation so apps act on behalf of the user only within declared fences.
- Provide both command and graphical interaction models without treating either as secondary.

### IPC routes provided

- `user.open_session(identity_ticket, session_style) -> session_handle`
- `user.cast_intent(cid, intent_packet) -> intent_receipt`
  - For shell verbs, launcher actions, or workspace commands.
- `user.lend_presence(cid, app_handle, lease_policy) -> lease_receipt`
  - Delegates a session-scoped presence lease to a program.
- `user.read_workspace(cid, cursor) -> workspace_slice`
- `user.close_session(session_handle, fold_policy) -> close_receipt`

### Required capabilities

- `cid.user.session.open`
- `cid.user.intent.cast`
- `cid.user.presence.lend`
- `cid.user.workspace.read`

### Typical interaction flows

- `USER -> SECURITY`: authenticate identity and obtain session root CIDs.
- `USER -> GRAPHICS`: build shell surfaces and scene graph.
- `USER -> PROGRAM`: raise apps with delegated session presence.
- `USER -> AI`: request guidance, completion, or workspace reasoning.

### Internal data structures

- `SessionBloom`
  - Session handle
  - Identity binding
  - Workspace root
  - Presence leases
- `IntentPacket`
  - Verb family
  - Target
  - Arguments
  - Desired effect class
- `WorkspaceWeave`
  - Window groupings
  - Focus route
  - Recent actions

## 8. TIME

### Space responsibilities

- Provide all time meaning inside LunaOS: monotonic ticks, wall epochs, timers, leases, deadlines, and drift estimates.
- Abstract hardware timers and calibration sources.
- Offer shared timer wheels and event wake plans to other spaces.

### IPC routes provided

- `time.read_pulse(cid, pulse_kind) -> pulse_sample`
- `time.set_chime(cid, chime_recipe) -> chime_ticket`
  - Registers a timer or repeating schedule.
- `time.fold_chime(cid, chime_ticket) -> fold_status`
- `time.read_drift(cid) -> drift_report`
- `time.bind_epoch(cid, epoch_claim) -> epoch_receipt`

### Required capabilities

- `cid.time.pulse.read`
- `cid.time.chime.set`
- `cid.time.chime.fold`
- `cid.time.drift.read`
- `cid.time.epoch.bind`

### Typical interaction flows

- `SYSTEM -> TIME`: obtain scheduling pulses.
- `UPDATE -> TIME`: bind rollout deadlines and grace windows.
- `SECURITY -> TIME`: stamp capability expirations.
- `OBSERVABILITY -> TIME`: correlate traces against pulse samples.

### Internal data structures

- `PulseFrame`
  - Monotonic ticks
  - Wall estimate
  - Drift score
- `ChimeWheel`
  - Bucket array
  - Wake epochs
  - Lease flags
- `EpochProof`
  - Source
  - Confidence
  - Last sync

## 9. LIFECYCLE

### Space responsibilities

- Own installation intent, activation, crash recovery, app restoration, and rollback at the application/service level.
- Distinguish package presence from runtime presence; a package may exist without being active.
- Maintain recovery recipes so failed spaces can be revived or fenced cleanly.

### IPC routes provided

- `lifecycle.stage_unit(cid, unit_recipe) -> unit_handle`
  - Register an installable or runnable unit.
- `lifecycle.wake_unit(cid, unit_handle, wake_plan) -> wake_receipt`
  - Activate a staged unit.
- `lifecycle.rest_unit(cid, unit_handle, rest_plan) -> rest_receipt`
  - Suspend or stop it.
- `lifecycle.revive_unit(cid, unit_handle, revive_plan) -> revive_receipt`
  - Crash recovery or stateful restart.
- `lifecycle.read_revival(cid, cursor) -> revival_slice`
  - Enumerates failure/recovery history.

### Required capabilities

- `cid.lifecycle.stage`
- `cid.lifecycle.wake`
- `cid.lifecycle.rest`
- `cid.lifecycle.revive`
- `cid.lifecycle.inspect`

### Typical interaction flows

- `PACKAGE -> LIFECYCLE`: stage a verified app unit.
- `USER -> LIFECYCLE`: wake or rest a user-visible unit.
- `OBSERVABILITY -> LIFECYCLE`: recommend revive or quarantine.
- `LIFECYCLE -> SYSTEM`: materialize or fold runtime spaces.

### Internal data structures

- `UnitCatalogEntry`
  - Unit handle
  - Package object
  - Runtime recipe
  - Recovery policy
- `RevivalLedger`
  - Failure epoch
  - Cause cluster
  - Last healthy seal
  - Recovery result
- `WakePlan`
  - Session target
  - Resource envelope
  - Delegation set

## 10. OBSERVABILITY

### Space responsibilities

- Collect logs, traces, metrics, health colors, and structural explanations across spaces.
- Provide a Luna-native event graph instead of line-only logs.
- Surface root-cause clusters and remediation hints to operators and `AI`.

### IPC routes provided

- `observe.emit_trace(cid, trace_packet) -> emit_receipt`
- `observe.emit_state(cid, state_packet) -> emit_receipt`
- `observe.read_stream(cid, stream_query) -> stream_ticket`
- `observe.paint_health(cid, target_handle) -> health_report`
- `observe.form_cluster(cid, cluster_query) -> cluster_report`

### Required capabilities

- `cid.observe.trace.emit`
- `cid.observe.state.emit`
- `cid.observe.stream.read`
- `cid.observe.health.paint`
- `cid.observe.cluster.form`

### Typical interaction flows

- All spaces -> `OBSERVABILITY`: emit traces and state deltas.
- `OBSERVABILITY -> GRAPHICS`: paint dashboards.
- `OBSERVABILITY -> AI`: provide structured diagnostics context.
- `OBSERVABILITY -> LIFECYCLE`: suggest revive, quarantine, or no-op.

### Internal data structures

- `TraceRibbon`
  - Event cells
  - Causal links
  - Time pulse refs
- `HealthColorMap`
  - Target handle
  - Color
  - Confidence
  - Cause pointer
- `ClusterForm`
  - Symptom set
  - Shared edges
  - Candidate root

## 11. AI

### Space responsibilities

- Host YueSi, the native reasoning engine of LunaOS.
- Provide inference, planning, learning queues, and system explanation services.
- Consume structured context from other spaces instead of scraping opaque process state.
- Never bypass capability policy; all access to data, frames, traces, and network goes through delegated CIDs.

### IPC routes provided

- `ai.invoke_mind(cid, mind_request) -> mind_ticket`
  - Start an inference or planning task.
- `ai.collect_mind(cid, mind_ticket) -> mind_result`
  - Poll or fetch result spans.
- `ai.teach_wave(cid, teach_recipe) -> teach_receipt`
  - Submit a bounded learning wave.
- `ai.explain_choice(cid, choice_handle) -> explanation_span`
- `ai.shape_agent(cid, agent_recipe) -> agent_handle`
  - Create a delegated agent persona with strict fences.

### Required capabilities

- `cid.ai.mind.invoke`
- `cid.ai.mind.collect`
- `cid.ai.teach.wave`
- `cid.ai.explain.choice`
- `cid.ai.agent.shape`
- Plus delegated CIDs for whatever external spaces an AI task touches.

### Typical interaction flows

- `USER -> AI`: request assistance, planning, or workspace reasoning.
- `PROGRAM -> AI`: invoke model-backed app features.
- `AI -> DATA`: fetch context objects if the caller delegated access.
- `AI -> OBSERVABILITY`: correlate system state before producing explanations.

### Internal data structures

- `MindRequest`
  - Task class
  - Prompt span refs
  - Tool fence
  - Budget fence
- `ThoughtWave`
  - Ticket
  - Intermediate spans
  - Final result span
  - Explainability anchor
- `AgentMold`
  - Persona
  - Delegated routes
  - Lifespan

## 12. PROGRAM

### Space responsibilities

- Run application code in Luna-native containers called `program cells`.
- Support native and bytecode-backed programs behind a common route model.
- Manage program memory spans, delegated capabilities, and termination fences.
- Expose app-declared routes to the rest of the constellation once authorized.

### IPC routes provided

- `program.raise_cell(cid, cell_recipe) -> cell_handle`
- `program.feed_cell(cid, cell_handle, feed_packet) -> feed_receipt`
  - Deliver stdin-like input, launch args, or external events.
- `program.read_cell(cid, cell_handle, read_cursor) -> cell_stream_slice`
  - Read stdout-like output or structured event streams.
- `program.fold_cell(cid, cell_handle, fold_reason) -> fold_receipt`
- `program.describe_cell(cid, cell_handle) -> cell_profile`

### Required capabilities

- `cid.program.cell.raise`
- `cid.program.cell.feed`
- `cid.program.cell.read`
- `cid.program.cell.fold`
- `cid.program.cell.describe`

### Typical interaction flows

- `USER -> PROGRAM`: start a program cell with session delegation.
- `LIFECYCLE -> PROGRAM`: restore or restart a cell after failure.
- `PROGRAM -> GRAPHICS/DATA/NETWORK/AI`: use delegated capabilities to access services.
- `PROGRAM -> OBSERVABILITY`: emit app trace ribbons.

### Internal data structures

- `CellRecord`
  - Cell handle
  - Image ref
  - ABI flavor
  - Delegation set
  - Health color
- `FeedPacket`
  - Channel
  - Payload span
  - Delivery epoch
- `CellRouteManifest`
  - Exported routes
  - Required routes
  - Policy hints

## 13. PACKAGE

### Space responsibilities

- Manage `.luna` packages, repository indexes, package signatures, and dependency shapes.
- Materialize packages into `DATA` objects and hand staged units to `LIFECYCLE`.
- Maintain package trust independently from runtime activity.

### IPC routes provided

- `package.intake_wave(cid, source_recipe) -> intake_ticket`
  - Fetch or import package material.
- `package.prove_wave(cid, intake_ticket) -> proof_report`
  - Validate package signature, dependency closure, and policy fit.
- `package.unpack_wave(cid, proof_ticket) -> unpack_receipt`
  - Write verified objects into `DATA`.
- `package.read_catalog(cid, cursor) -> package_slice`
- `package.trim_wave(cid, package_handle) -> trim_receipt`
  - Remove no-longer-needed package material.

### Required capabilities

- `cid.package.intake`
- `cid.package.prove`
- `cid.package.unpack`
- `cid.package.catalog.read`
- `cid.package.trim`

### Typical interaction flows

- `PACKAGE -> NETWORK`: fetch repository data.
- `PACKAGE -> SECURITY`: validate signatures and trust policies.
- `PACKAGE -> DATA`: store verified package objects.
- `PACKAGE -> LIFECYCLE`: register staged units after unpack.

### Internal data structures

- `WaveIntake`
  - Source
  - Package digest
  - Fetch status
- `ProofMatrix`
  - Signature result
  - Dependency closure
  - Policy fit
  - Seal chain
- `PackageCatalogEntry`
  - Name
  - Version braid
  - Object roots
  - Install state

## 14. UPDATE

### Space responsibilities

- Perform system-wide update planning, A/B image staging, activation, health probation, and rollback.
- Treat updates as controlled constellation transitions rather than blind package replacement.
- Coordinate `BOOT`, `PACKAGE`, `DATA`, `SYSTEM`, and `LIFECYCLE` into a reversible rollout wave.

### IPC routes provided

- `update.stage_wave(cid, update_recipe) -> wave_handle`
- `update.prove_wave(cid, wave_handle) -> wave_proof`
- `update.arm_wave(cid, wave_handle, arm_policy) -> arm_receipt`
  - Marks a wave for next boot activation.
- `update.read_wave(cid, wave_handle) -> wave_report`
- `update.rewind_wave(cid, wave_handle, rewind_reason) -> rewind_receipt`

### Required capabilities

- `cid.update.wave.stage`
- `cid.update.wave.prove`
- `cid.update.wave.arm`
- `cid.update.wave.read`
- `cid.update.wave.rewind`

### Typical interaction flows

- `UPDATE -> PACKAGE`: fetch and prove update material.
- `UPDATE -> DATA`: store alternate system image and metadata.
- `UPDATE -> SYSTEM`: prepare next constellation target.
- `BOOT -> UPDATE`: report whether the new wave survived probation after reboot.

### Internal data structures

- `UpdateWave`
  - Wave handle
  - Source version
  - Target version
  - Alternate root object
  - Probation policy
- `ProbationLedger`
  - Boot count
  - Failure count
  - Health checkpoints
- `RewindAnchor`
  - Previous root
  - Reason
  - Rewind readiness

## Cross-space design rules

### Capability philosophy

- Capabilities are never implicit. Every route that mutates or reveals a protected resource requires a CID.
- Capabilities are scope-fenced:
  - by route family
  - by object or namespace selector
  - by time fence
  - by delegation depth

### IPC philosophy

- LunaOS IPC is gate-based, not syscall-based.
- Gates carry structured packets and may refer to shared spans for large data movement.
- Each public route must declare:
  - request packet shape
  - response packet shape
  - capability requirements
  - failure codes
  - lease semantics

### Space lifecycle philosophy

- Spaces are composable authorities, not userland daemons beneath a hidden kernel.
- `BOOT` starts the first constellation.
- `SECURITY` mints authority.
- `SYSTEM` arranges resources.
- All later spaces exist by declared recipe and validated capability graph.

## Implementation order

1. Stabilize `BOOT` and `SECURITY` as the initial authority path.
2. Implement `DATA` and `LIFECYCLE` as the first persistent substrate.
3. Implement `USER` and `PROGRAM` for human interaction and app execution.
4. Implement `GRAPHICS` and `DEVICE` for visible and physical IO.
5. Implement `NETWORK`, `PACKAGE`, and `UPDATE` for distributed software flow.
6. Integrate `AI` as YueSi, with explicit capability fences to all dependent spaces.
