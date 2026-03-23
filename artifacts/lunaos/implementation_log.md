# LunaOS v2.0 Implementation Log

## 2026-03-23

### Baseline

- Active prototype converged to `BOOT + SECURITY` only.
- Verified QEMU serial output:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[BOOT] capability request ok`
  - `[BOOT] capability roster ok`
- Active build path:
  - `boot/bootsector.asm`
  - `boot/entry.asm`
  - `boot/main.c`
  - `security/src/main.rs`
  - `build/build.py`

### Design freeze for Phase 1

- Created `design/space_specs.md`.
- Defined original responsibilities, IPC routes, required capabilities, interaction flows, and core internal structures for all 15 spaces:
  - BOOT
  - SYSTEM
  - DATA
  - SECURITY
  - GRAPHICS
  - DEVICE
  - NETWORK
  - USER
  - TIME
  - LIFECYCLE
  - OBSERVABILITY
  - AI
  - PROGRAM
  - PACKAGE
  - UPDATE

### Phase 2 implementation sequence

1. Harden BOOT and SECURITY:
   - move current ad hoc gate structs toward a shared route schema
   - add seal and claim context support
2. Implement DATA:
   - Rust `no_std`
   - `lafs` object substrate
   - gate routes: `seed_object`, `pour_span`, `draw_span`, `shred_object`, `gather_set`, `commit_wave`
3. Implement LIFECYCLE:
   - stage/wake/rest/revive model
   - runtime recovery ledger
4. Add test spaces for DATA and LIFECYCLE validation before moving upward.

### Current status

- Phase 1 spec complete.
- Phase 2 implementation not started in this entry.

## 2026-03-23 Phase 2: DATA + LIFECYCLE

### Scope

- Implemented `DATA` space (ID 2) as the first `lafs` execution shell.
- Implemented `LIFECYCLE` space (ID 9) as a minimal live-space ledger.
- Reintroduced `TEST` as a validation space.
- Extended BOOT, SECURITY, shared protocol records, and image build layout to include the new spaces.

### Protocol changes

- Updated `include/luna_proto.h`:
  - added active space IDs:
    - `BOOT = 0`
    - `DATA = 2`
    - `SECURITY = 3`
    - `LIFECYCLE = 9`
    - `TEST = 15`
  - added DATA capability domains:
    - `/data/object.seed`
    - `/data/object.pour`
    - `/data/object.draw`
    - `/data/object.shred`
    - `/data/object.gather`
  - added LIFECYCLE capability domains:
    - `/lifecycle/unit.wake`
    - `/lifecycle/unit.read`
  - added `luna_data_gate`, `luna_lifecycle_gate`, `luna_unit_record`, and expanded `luna_manifest`.

### SECURITY changes

- Reworked `security/src/main.rs` to mint seven hardcoded CIDs:
  - five DATA CIDs
  - two LIFECYCLE CIDs
- Kept the minimal BOOT smoke path:
  - `request_capability`
  - `list_capabilities`
- `list_capabilities` now reports `7`.

### DATA implementation

- Replaced the previous disk-backed experiment with a simpler phase-2 memory-backed `lafs` core in `data/src/main.rs`.
- Current object model:
  - 32 object slots
  - 2048 bytes content per slot
  - 128-bit object IDs
  - type, flags, created_at, modified_at, content_size metadata
- Implemented gate operations:
  - `LUNA_DATA_SEED_OBJECT`
  - `LUNA_DATA_POUR_SPAN`
  - `LUNA_DATA_DRAW_SPAN`
  - `LUNA_DATA_SHRED_OBJECT`
  - `LUNA_DATA_GATHER_SET`
- CID validation is enforced inside DATA before each operation.
- Boot log:
  - `[DATA] lafs memory online`

### LIFECYCLE implementation

- Added new space sources:
  - `lifecycle/src/main.rs`
  - `lifecycle/lifecycle.ld`
- Implemented a simple unit ledger:
  - capacity: 16 records
  - fields: `space_id`, `state`, `epoch`, `flags`
- Implemented gate operations:
  - `LUNA_LIFE_WAKE_UNIT`
  - `LUNA_LIFE_REST_UNIT`
  - `LUNA_LIFE_READ_UNITS`
- BOOT records `BOOT`, `SECURITY`, `DATA`, `LIFECYCLE`, and `TEST` as live units after LIFECYCLE comes online.
- Boot log:
  - `[LIFECYCLE] ledger online`

### BOOT integration

- `boot/main.c` now performs:
  1. serial init and BOOT banner
  2. launch SECURITY
  3. request DATA and LIFECYCLE CIDs from SECURITY
  4. verify capability roster
  5. launch DATA
  6. launch LIFECYCLE
  7. register live spaces in LIFECYCLE
  8. launch TEST

### TEST implementation

- Reworked `test/main.c` into a phase-2 validation flow.
- TEST now:
  - asks SECURITY for DATA and LIFECYCLE CIDs
  - creates one object in DATA
  - writes a payload into it
  - reads the payload back
  - gathers the object list
  - queries the LIFECYCLE unit list
  - verifies that `BOOT`, `SECURITY`, `DATA`, `LIFECYCLE`, and `TEST` are all recorded as live

### Build and layout changes

- `build/luna.ld` now defines:
  - `SPACE BOOT 0x10000`
  - `SPACE SECURITY 0x11000`
  - `SPACE DATA 0x12000`
  - `SPACE LIFECYCLE 0x15000`
  - `SPACE TEST 0x18000`
  - `MANIFEST 0x1E000`
  - `GATE SECURITY 0x31000`
  - `GATE DATA 0x32000`
  - `GATE LIFECYCLE 0x33000`
  - `BUFFER DATA 0x34000 size 0x2000`
  - `BUFFER LIST 0x36000 size 0x2000`
- `build/build.py` now compiles and embeds:
  - BOOT
  - SECURITY
  - DATA
  - LIFECYCLE
  - TEST

### Debugging notes

- First DATA build failed due to unresolved `memcpy`, `memset`, and implicit bounds-check paths in the Rust object file.
- Fixed by:
  - adding bare-metal `memcpy` and `memset` exports in DATA
  - switching slot access to raw-pointer helpers to avoid hidden bounds-check linkage
- First LIFECYCLE validation failed because the ledger treated `space_id == 0` as an empty slot marker.
- Fixed by using `epoch == 0` as the real empty-slot condition so `BOOT` can be tracked correctly.

### Build results

- Command:
  - `python .\build\build.py`
- Final image:
  - `build/lunaos.img`
- Final image size:
  - `58368` bytes
- Final packed stage sectors:
  - `113`
- Final payload sizes:
  - BOOT `2000`
  - SECURITY `612`
  - DATA `2920`
  - LIFECYCLE `3368`
  - TEST `3312`

### Final QEMU validation

- Command form:
  - `qemu-system-x86_64 -drive format=raw,file=build/lunaos.img -serial stdio -display none -no-reboot -no-shutdown`
- Captured serial output:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs memory online
[LIFECYCLE] ledger online
[TEST] data gates ok
[TEST] lifecycle query ok
```

### Status after this entry

- Phase 2 base implementation is working in QEMU.
- DATA is currently memory-backed and validates the intended IPC contract.
- Next step for DATA is replacing the in-memory payload arena with the planned raw-disk `lafs` backend while preserving the same gate schema.
- Next step after that is building `SYSTEM`, then extending LIFECYCLE from a ledger into a real launch/recovery coordinator.

## 2026-03-23 Phase 3: Disk DATA + SYSTEM

### Scope

- Upgraded `DATA` from a volatile memory arena to a disk-backed `lafs` store using the same gate interface.
- Implemented new `SYSTEM` space (ID 1).
- Extended BOOT startup order to:
  - `BOOT -> SECURITY -> DATA -> LIFECYCLE -> SYSTEM -> TEST`
- Extended TEST to validate:
  - DATA persistence across reboot
  - LIFECYCLE live-space enumeration
  - SYSTEM registration/query results

### Shared protocol changes

- Updated `include/luna_proto.h`:
  - added `SYSTEM = 1`
  - added `luna_system_gate` and `luna_system_record`
  - added system capability domains:
    - `/system/space.register`
    - `/system/space.query`
    - `/system/resource.allocate`
    - `/system/event.report`
  - expanded `luna_bootview` with `system_gate_base`, `data_buffer_base`, and `list_buffer_base`
  - expanded `luna_manifest` with SYSTEM boot/gate entries plus DATA store layout fields

### SECURITY changes

- Reworked `security/src/main.rs` again so SECURITY now mints 11 hardcoded capability families:
  - 5 DATA
  - 2 LIFECYCLE
  - 4 SYSTEM
- `list_capabilities` now reports `11`.

### DATA persistence design

- `data/src/main.rs` now uses a simple ATA PIO backend.
- Chosen original `lafs` disk layout:
  - base LBA: `256`
  - sector `256`: superblock
  - sectors `257..278`: object record table (`22` sectors)
  - sectors `279..406`: object payload slots (`128` slots, `1` sector each)
- Object model:
  - fixed capacity: `128`
  - fixed payload size: `512` bytes per object
  - metadata per record:
    - live flag
    - object type
    - flags
    - slot index
    - size
    - created_at
    - modified_at
    - 128-bit object ID
- Superblock fields:
  - magic
  - version
  - object_capacity
  - slot_sectors
  - store_base_lba
  - data_start_lba
  - next_nonce
  - format_count
  - mount_count

### DATA driver implementation

- Implemented ATA PIO routines in `data/src/main.rs`:
  - port selection on `0x1F0..0x1F7`
  - `ata_read_sector`
  - `ata_write_sector`
  - readiness/idle polling
- On boot:
  - DATA reads the superblock
  - if magic/version mismatch, DATA formats the store
  - otherwise DATA mounts and increments `mount_count`
- Boot log now:
  - `[DATA] lafs disk online`

### DATA gate behavior

- Existing gates preserved:
  - `seed`
  - `pour`
  - `draw`
  - `shred`
  - `gather`
- CID validation is still enforced before every operation.
- TEST behavior now demonstrates real persistence:
  - boot 1: object absent -> create/write/read immediate check -> `[TEST] data armed`
  - boot 2: object present -> read existing object -> `[TEST] persistence ok`

### SYSTEM implementation

- Added new files:
  - `system/src/main.rs`
  - `system/system.ld`
- Implemented original SYSTEM routes:
  - `register_space`
  - `query_space`
  - `allocate_resource`
  - `report_event`
- Internal model:
  - fixed `16`-entry `LunaSystemRecord` table
  - fields:
    - `space_id`
    - `state`
    - `resource_memory`
    - `resource_time`
    - `last_event`
    - `name[16]`
- Boot log:
  - `[SYSTEM] atlas online`

### BOOT integration

- `boot/main.c` now:
  1. boots SECURITY
  2. requests LIFECYCLE and SYSTEM CIDs
  3. boots DATA
  4. boots LIFECYCLE
  5. boots SYSTEM
  6. records live units in LIFECYCLE
  7. registers BOOT, SECURITY, DATA, LIFECYCLE, SYSTEM, and TEST in SYSTEM
  8. grants sample resource counters and a sample event to SYSTEM
  9. launches TEST

### TEST updates

- `test/main.c` now requests:
  - DATA CIDs
  - LIFECYCLE read CID
  - SYSTEM query CID
- TEST validates:
  - DATA create/write/read on first boot
  - DATA readback across reboot on second boot
  - LIFECYCLE contains all six live spaces
  - SYSTEM query for `DATA` returns:
    - correct space id
    - active state
    - name `DATA`

### Build/layout changes

- `build/luna.ld` active space layout:
  - `BOOT 0x10000`
  - `SECURITY 0x11000`
  - `DATA 0x12000`
  - `LIFECYCLE 0x16000`
  - `SYSTEM 0x18000`
  - `TEST 0x1A000`
  - `MANIFEST 0x1E000`
- Gate layout:
  - `SECURITY 0x31000`
  - `DATA 0x32000`
  - `LIFECYCLE 0x33000`
  - `SYSTEM 0x34000`
- Shared buffers:
  - `DATA 0x35000 size 0x2000`
  - `LIST 0x37000 size 0x2000`
- `build/build.py` now:
  - builds and embeds SYSTEM
  - writes store layout fields into manifest
  - pads `build/lunaos.img` to cover the whole DATA store area

### Debugging notes

- Initial phase-3 image produced no serial output because the packed stage grew to `145` sectors.
- BIOS stage loading stabilized after compressing the layout back under the single-read threshold.
- Next failure was silent after DATA boot. Root cause:
  - DATA `.bss` virtual size extended past the original `LIFECYCLE` base and overwrote later spaces.
  - Fixed by moving `LIFECYCLE`, `SYSTEM`, `TEST`, and `MANIFEST` upward.
- First DATA write/read test failed because object record storage used only `16` sectors.
  - Real `ObjectRecord` table footprint is `22` sectors.
  - Fixed the metadata layout and shifted the data area accordingly.
- A false persistence failure also appeared during one validation pass because two QEMU instances were launched in parallel against the same raw image.
  - Final persistence verification was re-run sequentially and passed.

### Final build results

- Command:
  - `python .\build\build.py`
- Final image:
  - `build/lunaos.img`
- Final image size:
  - `208384` bytes
- Final packed stage sectors:
  - `113`
- Final payload sizes:
  - BOOT `3056`
  - SECURITY `772`
  - DATA `3924`
  - LIFECYCLE `3368`
  - SYSTEM `4024`
  - TEST `4304`

### Final sequential validation

- Validation method:
  - rebuild image once
  - boot QEMU once
  - boot the same image a second time without rebuilding
- Boot 1 serial output:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[TEST] data armed
[TEST] lifecycle query ok
[TEST] system query ok
```

- Boot 2 serial output:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[TEST] persistence ok
[TEST] lifecycle query ok
[TEST] system query ok
```

### Status after this entry

- DATA persistence is now working against the QEMU raw disk image.
- SYSTEM is present as the first resource/space registry.
- Next logical step is to add `PROGRAM` or `SYSTEM`-driven launch recipes, then make LIFECYCLE and SYSTEM cooperate instead of BOOT performing all registrations directly.

## 2026-03-23 Phase 4: Dynamic Spawn via SYSTEM + LIFECYCLE

### Scope

- Removed static TEST launch from BOOT.
- Added a minimal dynamic spawn path owned by `SYSTEM` and executed through `LIFECYCLE`.
- Added a new dynamic `HELLO` space to prove runtime spawning works.
- Kept the existing DATA persistence and SYSTEM/LIFECYCLE validation by spawning TEST dynamically after HELLO.

### High-level behavior change

- BOOT now statically loads only:
  - `SECURITY`
  - `DATA`
  - `LIFECYCLE`
  - `SYSTEM`
- BOOT no longer directly loads `TEST` or any other non-foundation space.
- SYSTEM now owns the startup recipe and executes it after its own initialization.

### Protocol changes

- Updated `include/luna_proto.h`:
  - added `LUNA_SPACE_HELLO = 15`
  - moved `LUNA_SPACE_TEST = 16`
  - added `LUNA_CAP_LIFE_SPAWN`
  - added `LUNA_LIFE_SPAWN_SPACE`
  - extended `luna_lifecycle_gate` with `entry_addr`
  - expanded `luna_manifest` with:
    - `hello_base`
    - `hello_boot_entry`
    - `test_base`
    - `test_boot_entry`

### SECURITY changes

- SECURITY now mints one additional capability:
  - `/lifecycle/unit.spawn`
- `list_capabilities` now reports `12`.

### LIFECYCLE spawn implementation

- `lifecycle/src/main.rs` now supports:
  - `LUNA_LIFE_SPAWN_SPACE`
- Simplified spawn model used in this phase:
  - SYSTEM passes a space ID, bootview pointer, and embedded entry address
  - LIFECYCLE records the unit as live
  - LIFECYCLE prints a spawn log
  - LIFECYCLE directly calls the target space entry in the current address space
- This keeps the architecture logically space-oriented while deferring true separate address-space construction to a later phase.

### SYSTEM startup recipe

- `system/src/main.rs` now owns an internal startup recipe.
- At boot SYSTEM:
  1. requests its needed CIDs from SECURITY
  2. marks base spaces live in LIFECYCLE
  3. registers base spaces in SYSTEM
  4. applies sample resource/event records
  5. spawns `HELLO`
  6. registers `HELLO`
  7. spawns `TEST`
  8. registers `TEST`

- Current hardcoded dynamic recipe:
  - `HELLO`
  - `TEST`

### HELLO space

- Added:
  - `hello/main.c`
  - `hello/hello.ld`
- HELLO is embedded in the image but not directly launched by BOOT.
- Runtime log:

```text
[HELLO] Hello from dynamic space!
```

### BOOT changes

- `boot/main.c` now only:
  - brings up serial
  - launches SECURITY
  - verifies capability smoke path
  - launches DATA
  - launches LIFECYCLE
  - launches SYSTEM
- All non-foundation startup work moved out of BOOT.

### TEST changes

- TEST is now dynamically spawned by SYSTEM.
- TEST still validates:
  - DATA persistence
  - LIFECYCLE query correctness
  - SYSTEM query correctness
- TEST expectations now include both:
  - `HELLO`
  - `TEST`
  in the live-space list.

### Build and layout changes

- `build/luna.ld` now includes:
  - `SPACE HELLO 0x1A000`
  - `SPACE TEST 0x1B000`
  - `MANIFEST 0x1F000`
- `build/build.py` now compiles and embeds:
  - `HELLO`
  - `TEST`
  as runtime-spawnable image segments
- BOOT still loads the entire stage image into memory, but the decision to execute HELLO/TEST is now deferred to SYSTEM via LIFECYCLE.

### Debugging notes

- First dynamic-spawn attempt failed immediately after `[BOOT] dawn online`.
  - Root cause: BOOT, SYSTEM, and TEST were still using the old manifest address `0x1E000` after the manifest moved to `0x1F000`.
  - Fixed by updating all three to `0x1F000`.
- The dynamic stage still fits within BIOS loader constraints:
  - final packed stage sectors: `121`

### Final build results

- Command:
  - `python .\build\build.py`
- Final image:
  - `build/lunaos.img`
- Final image size:
  - `208384` bytes
- Final payload sizes:
  - BOOT `1168`
  - SECURITY `820`
  - DATA `3924`
  - LIFECYCLE `4184`
  - SYSTEM `6840`
  - HELLO `224`
  - TEST `4336`

### Final validation

- First boot after rebuild:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning HELLO space...
[LIFECYCLE] Spawning space 15
[HELLO] Hello from dynamic space!
[SYSTEM] Space 15 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] data armed
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

- Second boot on the same image:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning HELLO space...
[LIFECYCLE] Spawning space 15
[HELLO] Hello from dynamic space!
[SYSTEM] Space 15 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] persistence ok
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

### Status after this entry

- LunaOS now has a working dynamic startup path controlled by SYSTEM and executed through LIFECYCLE.
- BOOT has been reduced to foundation-space loading only.
- HELLO proves dynamic spawn execution.
- TEST still validates DATA persistence and registry correctness after being dynamically spawned.
- The next meaningful step is to replace the hardcoded SYSTEM recipe with a manifest-driven or DATA-backed startup recipe, then extend spawn from “same-address-space entry call” into real isolated address-space construction.

## 2026-03-23 Phase 5: USER + PROGRAM

### Scope

- Added `PROGRAM` space (ID `12`) as the first Luna application runtime.
- Added `USER` space (ID `7`) as the first serial shell and session bridge.
- Replaced the old dynamic `HELLO` space proof with a packaged `hello.luna` application executed through PROGRAM.
- Kept the existing foundation path intact:
  - `BOOT -> SECURITY -> DATA -> LIFECYCLE -> SYSTEM`
- Updated the dynamic startup recipe so SYSTEM now spawns:
  - `PROGRAM`
  - `USER`
  - `TEST`

### Protocol and manifest changes

- Updated `include/luna_proto.h`:
  - new active spaces:
    - `USER = 7`
    - `PROGRAM = 12`
  - new capability domains:
    - `/program/app.load`
    - `/program/app.start`
    - `/program/app.stop`
    - `/user/shell`
  - new gate schema:
    - `luna_program_gate`
  - expanded `luna_bootview` with:
    - `program_gate_base`
  - expanded `luna_manifest` with:
    - `user_base`
    - `user_boot_entry`
    - `program_base`
    - `program_boot_entry`
    - `program_gate_entry`
    - `program_gate_base`
    - `app_hello_base`
    - `app_hello_size`
- Bumped protocol version to:
  - `0x00000240`

### SECURITY changes

- Extended `security/src/main.rs` to mint four new CID families:
  - PROGRAM load
  - PROGRAM start
  - PROGRAM stop
  - USER shell
- Capability roster count is now:
  - `16`

### PROGRAM implementation

- Added:
  - `program/main.c`
  - `program/program.ld`
- PROGRAM exposes original gates:
  - `load_app`
  - `start_app`
  - `stop_app`
- Runtime model:
  - embedded `.luna` package lookup by application name
  - package validation through an original header:
    - magic
    - version
    - entry offset
    - capability key count
    - app name
    - up to four declared capability keys
  - current loaded app kept as a runtime ticket
  - `start_app` requests declared capabilities from SECURITY before jumping to the embedded entrypoint
- Boot log:
  - `[PROGRAM] runtime ready`

### USER implementation

- Added:
  - `user/main.c`
  - `user/user.ld`
- USER now provides a serial shell with prompt:
  - `luna:~$`
- Implemented built-ins:
  - `help`
  - `list-spaces`
  - `run <app>`
  - `exit`
- `list-spaces` behavior:
  - requests LIFECYCLE read capability
  - reads the current live-unit ledger
  - renders active space names back to serial
- `run hello` behavior:
  - requests PROGRAM load/start capabilities
  - calls PROGRAM gate
  - loads `hello.luna`
  - executes the packaged app
- Boot log:
  - `[USER] shell ready`

### hello.luna application packaging

- Added:
  - `apps/hello_app.c`
  - `apps/hello_app.ld`
- `hello.luna` is now built by `build/build.py`, not hand-authored as a static blob.
- Packaging format is original:
  - 72-byte app header
  - raw embedded code blob
- Current sample app behavior:
  - prints `Hello from LunaOS!`

### SYSTEM startup recipe changes

- `system/src/main.rs` no longer spawns the old dynamic HELLO space.
- New dynamic order:
  1. spawn PROGRAM
  2. register PROGRAM
  3. spawn USER
  4. USER runs until `exit`
  5. register USER
  6. spawn TEST
  7. register TEST

### BOOT and loader changes

- `boot/main.c` now writes `program_gate_base` into the bootview.
- Manifest address moved to:
  - `0x22000`
- `boot/bootsector.asm` was upgraded from a single extended-read call to chunked EDD reads.
- Reason:
  - the packed stage grew to `145` sectors
  - previous single-shot BIOS read design was no longer sufficient

### TEST changes

- `test/main.c` now expects the live-space set to include:
  - `BOOT`
  - `SECURITY`
  - `DATA`
  - `LIFECYCLE`
  - `SYSTEM`
  - `PROGRAM`
  - `USER`
  - `TEST`
- Existing regression checks still pass:
  - DATA first-boot arm
  - DATA second-boot persistence
  - LIFECYCLE enumeration
  - SYSTEM query for `DATA`

### Build and layout changes

- `build/luna.ld` now includes:
  - `SPACE PROGRAM 0x1A000`
  - `SPACE USER 0x1C000`
  - `SPACE TEST 0x1E000`
  - `APP HELLO 0x20000`
  - `MANIFEST 0x22000`
  - `GATE PROGRAM 0x35000`
- `build/build.py` now:
  - parses `APP` layout regions
  - compiles USER
  - compiles PROGRAM
  - compiles the sample app
  - packs `hello.luna`
  - writes app placement into the manifest

### Final build results

- Command:
  - `python .\build\build.py`
- Final image:
  - `build/lunaos.img`
- Final image size:
  - `208384` bytes
- Final packed stage sectors:
  - `145`
- Final payload sizes:
  - BOOT `1200`
  - SECURITY `1012`
  - DATA `3924`
  - LIFECYCLE `4184`
  - SYSTEM `7544`
  - PROGRAM `944`
  - USER `2736`
  - TEST `4432`
  - `hello.luna` `280`

### Validation method

- Rebuilt the image once.
- Performed two sequential QEMU boots against the same raw image.
- Used QEMU serial TCP mode instead of `stdio` so the USER shell could receive real input on Windows.
- Sent this command sequence to USER on each boot:
  - `list-spaces`
  - `run hello`
  - `exit`

### Boot 1 serial output

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning PROGRAM space...
[LIFECYCLE] Spawning space 12
[PROGRAM] runtime ready
[SYSTEM] Space 12 ready.
[SYSTEM] Spawning USER space...
[LIFECYCLE] Spawning space 7
[USER] shell ready
luna:~$ list-spaces
[USER] live spaces: LIFECYCLE BOOT SECURITY DATA SYSTEM PROGRAM USER
luna:~$ run hello
Hello from LunaOS!
luna:~$ exit
[USER] session closed
[SYSTEM] Space 7 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] data armed
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

### Boot 2 serial output

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning PROGRAM space...
[LIFECYCLE] Spawning space 12
[PROGRAM] runtime ready
[SYSTEM] Space 12 ready.
[SYSTEM] Spawning USER space...
[LIFECYCLE] Spawning space 7
[USER] shell ready
luna:~$ list-spaces
[USER] live spaces: LIFECYCLE BOOT SECURITY DATA SYSTEM PROGRAM USER
luna:~$ run hello
Hello from LunaOS!
luna:~$ exit
[USER] session closed
[SYSTEM] Space 7 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] persistence ok
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

### Status after this entry

- LunaOS now has a working shell and a working packaged-application runtime.
- USER can accept serial commands and route them to other spaces.
- PROGRAM can load and execute an embedded `.luna` package.
- Existing DATA, LIFECYCLE, and SYSTEM regressions still pass.
- The next logical step is splitting application instances away from the shared address space and moving app manifests out of the image into DATA or PACKAGE-managed storage.

## 2026-03-23 Phase 6: Remaining Core Spaces

### Scope

- Implemented the remaining eight required core spaces:
  - `TIME (8)`
  - `DEVICE (5)`
  - `OBSERVABILITY (10)`
  - `NETWORK (6)`
  - `GRAPHICS (4)`
  - `PACKAGE (13)`
  - `UPDATE (14)`
  - `AI (11)`
- Extended `SYSTEM` so the runtime startup recipe now raises all core spaces before `USER`, then starts `TEST` after the shell session closes.
- Extended `TEST` into a full constellation regression that verifies every newly added space.

### Shared protocol changes

- Reworked `include/luna_proto.h` into the first full 15-space shared protocol header.
- Added all missing core space IDs:
  - `GRAPHICS = 4`
  - `DEVICE = 5`
  - `NETWORK = 6`
  - `TIME = 8`
  - `OBSERVABILITY = 10`
  - `AI = 11`
  - `PACKAGE = 13`
  - `UPDATE = 14`
- Added new CID domains for:
  - TIME
  - DEVICE
  - OBSERVABILITY
  - NETWORK
  - GRAPHICS
  - PACKAGE
  - UPDATE
  - AI
- Added new gate records:
  - `luna_time_gate`
  - `luna_device_gate`
  - `luna_observe_gate`
  - `luna_network_gate`
  - `luna_graphics_gate`
  - `luna_package_gate`
  - `luna_update_gate`
  - `luna_ai_gate`
- Expanded `luna_bootview` with gate bases for all new spaces.
- Expanded `luna_manifest` with:
  - boot entries
  - gate entries
  - gate bases
  for all newly implemented spaces.
- Bumped protocol version to:
  - `0x00000250`

### TIME

- Added:
  - `time/main.c`
  - `time/time.ld`
- Implementation:
  - uses `RDTSC` as the monotonic pulse source
  - `read_pulse` returns a tick-like monotonic value
  - `set_chime` performs a busy-wait sleep
- Boot log:
  - `[TIME] pulse ready`

### DEVICE

- Added:
  - `device/main.c`
  - `device/device.ld`
- Implementation:
  - establishes a Luna device lane for serial
  - `device_list` returns one `serial0` lane
  - `device_read` reads a pending serial byte when available
  - `device_write` writes bytes back to serial
- Boot log:
  - `[DEVICE] lane ready`

### OBSERVABILITY

- Added:
  - `observe/main.c`
  - `observe/observe.ld`
- Implementation:
  - in-memory ring of `32` trace cells
  - `log`
  - `get_logs`
  - `get_stats`
- Boot log:
  - `[OBSERVE] ribbon ready`

### NETWORK

- Added:
  - `network/main.c`
  - `network/network.ld`
- Implementation:
  - loopback-only initial `lanet` stub
  - one in-memory packet slot
  - `send_packet`
  - `receive_packet`
- Boot log:
  - `[NETWORK] loop ready`

### GRAPHICS

- Added:
  - `graphics/main.c`
  - `graphics/graphics.ld`
- Implementation:
  - VGA text-mode sink at `0xB8000`
  - `draw_char(x, y, glyph)`
- Boot log:
  - `[GRAPHICS] console ready`

### PACKAGE

- Added:
  - `package/main.c`
  - `package/package.ld`
- Implementation:
  - minimal package catalog
  - `install("hello")`
  - `list_installed()`
  - uses the embedded `hello.luna` as the initial installable package
- Boot log:
  - `[PACKAGE] catalog ready`

### UPDATE

- Added:
  - `update/main.c`
  - `update/update.ld`
- Implementation:
  - simulated version wave state
  - `check_update()`
  - `apply_update()`
  - tracks a pending-arm flag
- Boot log:
  - `[UPDATE] wave ready`

### AI

- Added:
  - `ai/main.c`
  - `ai/ai.ld`
- Implementation:
  - YueSi rule-engine stub
  - `infer(text)`
    - `list spaces`
    - `create temporary storage`
  - `create_space(recipe)`
    - requests a lifecycle wake CID from SECURITY
    - wakes a named existing space through LIFECYCLE
- Boot log:
  - `[AI] yuesi stub ready`

### SECURITY changes

- Reworked `security/src/main.rs` so SECURITY now mints the additional CID families needed by the new spaces.
- Capability roster count now reports:
  - `33`

### SYSTEM startup recipe changes

- Reworked `system/src/main.rs` so SYSTEM now spawns and registers:
  1. TIME
  2. DEVICE
  3. OBSERVABILITY
  4. NETWORK
  5. GRAPHICS
  6. PACKAGE
  7. UPDATE
  8. AI
  9. PROGRAM
  10. USER
  11. TEST

### TEST changes

- `test/main.c` now validates:
  - DATA first-boot arm / second-boot persistence
  - TIME pulse delta
  - DEVICE enumeration
  - OBSERVABILITY log/read/stats
  - NETWORK loopback send/receive
  - GRAPHICS draw route
  - PACKAGE install/list
  - UPDATE check/apply
  - AI infer/create
  - LIFECYCLE enumeration of all core spaces plus TEST
  - SYSTEM query for DATA

### Image layout changes

- Reworked `build/luna.ld` for the full constellation:
  - added all new spaces
  - added all new gate pages
  - moved manifest to `0x39000`
  - moved bootview to `0x42000`
- Reworked `build/build.py` to:
  - build all newly added spaces
  - write the expanded manifest
  - emit the expanded constellation map

### Important bug fix

- First implementation pass booted correctly once but the second boot failed after `[BOOT] dawn online`.
- Root cause:
  - the packed stage had grown to `330` sectors
  - `lafs` still began at `LBA 256`
  - DATA writes on first boot overwrote the tail of the runtime stage image
- Fix:
  - moved `DATA_STORE_LBA` from `256` to `1024`
  - updated:
    - `include/luna_proto.h`
    - `build/build.py`
    - `data/src/main.rs`

### Final build results

- Command:
  - `python .\build\build.py`
- Final image:
  - `build/lunaos.img`
- Final image size:
  - `601600` bytes
- Final packed stage sectors:
  - `330`
- Final payload sizes:
  - BOOT `1328`
  - SECURITY `1828`
  - DATA `3924`
  - LIFECYCLE `4184`
  - SYSTEM `10360`
  - PROGRAM `944`
  - TIME `480`
  - DEVICE `592`
  - OBSERVE `2528`
  - NETWORK `496`
  - GRAPHICS `336`
  - PACKAGE `544`
  - UPDATE `384`
  - AI `2160`
  - USER `2896`
  - TEST `7568`
  - `hello.luna` `280`

### Final sequential validation

- Validation method:
  - rebuild image once
  - boot the same image twice in sequence
  - send serial commands through QEMU TCP serial:
    - `list-spaces`
    - `run hello`
    - `exit`

### Boot 1 serial output

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning TIME space...
[LIFECYCLE] Spawning space 8
[TIME] pulse ready
[SYSTEM] Space 8 ready.
[SYSTEM] Spawning DEVICE space...
[LIFECYCLE] Spawning space 5
[DEVICE] lane ready
[SYSTEM] Space 5 ready.
[SYSTEM] Spawning OBSERVE space...
[LIFECYCLE] Spawning space 10
[OBSERVE] ribbon ready
[SYSTEM] Space 10 ready.
[SYSTEM] Spawning NETWORK space...
[LIFECYCLE] Spawning space 6
[NETWORK] loop ready
[SYSTEM] Space 6 ready.
[SYSTEM] Spawning GRAPHICS space...
[LIFECYCLE] Spawning space 4
[GRAPHICS] console ready
[SYSTEM] Space 4 ready.
[SYSTEM] Spawning PACKAGE space...
[LIFECYCLE] Spawning space 13
[PACKAGE] catalog ready
[SYSTEM] Space 13 ready.
[SYSTEM] Spawning UPDATE space...
[LIFECYCLE] Spawning space 14
[UPDATE] wave ready
[SYSTEM] Space 14 ready.
[SYSTEM] Spawning AI space...
[LIFECYCLE] Spawning space 11
[AI] yuesi stub ready
[SYSTEM] Space 11 ready.
[SYSTEM] Spawning PROGRAM space...
[LIFECYCLE] Spawning space 12
[PROGRAM] runtime ready
[SYSTEM] Space 12 ready.
[SYSTEM] Spawning USER space...
[LIFECYCLE] Spawning space 7
[USER] shell ready
luna:~$ list-spaces
[USER] live spaces: LIFECYCLE BOOT SECURITY DATA SYSTEM TIME DEVICE OBSERVE NETWORK GRAPHICS PACKAGE UPDATE AI PROGRAM USER
luna:~$ run hello
Hello from LunaOS!
luna:~$ exit
[USER] session closed
[SYSTEM] Space 7 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] data armed
[TEST] time ok
[TEST] device ok
[TEST] observe ok
[TEST] network ok
[TEST] graphics ok
[TEST] package ok
[TEST] update ok
[TEST] ai ok
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

### Boot 2 serial output

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
[DATA] lafs disk online
[LIFECYCLE] ledger online
[SYSTEM] atlas online
[SYSTEM] Spawning TIME space...
[LIFECYCLE] Spawning space 8
[TIME] pulse ready
[SYSTEM] Space 8 ready.
[SYSTEM] Spawning DEVICE space...
[LIFECYCLE] Spawning space 5
[DEVICE] lane ready
[SYSTEM] Space 5 ready.
[SYSTEM] Spawning OBSERVE space...
[LIFECYCLE] Spawning space 10
[OBSERVE] ribbon ready
[SYSTEM] Space 10 ready.
[SYSTEM] Spawning NETWORK space...
[LIFECYCLE] Spawning space 6
[NETWORK] loop ready
[SYSTEM] Space 6 ready.
[SYSTEM] Spawning GRAPHICS space...
[LIFECYCLE] Spawning space 4
[GRAPHICS] console ready
[SYSTEM] Space 4 ready.
[SYSTEM] Spawning PACKAGE space...
[LIFECYCLE] Spawning space 13
[PACKAGE] catalog ready
[SYSTEM] Space 13 ready.
[SYSTEM] Spawning UPDATE space...
[LIFECYCLE] Spawning space 14
[UPDATE] wave ready
[SYSTEM] Space 14 ready.
[SYSTEM] Spawning AI space...
[LIFECYCLE] Spawning space 11
[AI] yuesi stub ready
[SYSTEM] Space 11 ready.
[SYSTEM] Spawning PROGRAM space...
[LIFECYCLE] Spawning space 12
[PROGRAM] runtime ready
[SYSTEM] Space 12 ready.
[SYSTEM] Spawning USER space...
[LIFECYCLE] Spawning space 7
[USER] shell ready
luna:~$ list-spaces
[USER] live spaces: LIFECYCLE BOOT SECURITY DATA SYSTEM TIME DEVICE OBSERVE NETWORK GRAPHICS PACKAGE UPDATE AI PROGRAM USER
luna:~$ run hello
Hello from LunaOS!
luna:~$ exit
[USER] session closed
[SYSTEM] Space 7 ready.
[SYSTEM] Spawning TEST space...
[LIFECYCLE] Spawning space 16
[TEST] persistence ok
[TEST] time ok
[TEST] device ok
[TEST] observe ok
[TEST] network ok
[TEST] graphics ok
[TEST] package ok
[TEST] update ok
[TEST] ai ok
[TEST] lifecycle query ok
[TEST] system query ok
[SYSTEM] Space 16 ready.
```

### Status after this entry

- LunaOS now boots and validates all 15 core spaces, plus the TEST validation space.
- SYSTEM controls the full dynamic startup recipe.
- USER and PROGRAM continue to function while the newly added service spaces are live.
- DATA persistence still works after the full constellation expansion.

## Phase 7: hardware-facing bootstrap skeleton

### Intent

- Push the repository beyond a QEMU-only flat image and toward a real-hardware boot skeleton.
- Keep the validated BIOS prototype working while adding a first UEFI artifact and a GPT disk image layout.

### Changes

- Added `boot/uefi_boot.c` as a minimal PE/COFF UEFI application that writes directly through `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL`.
- Added `boot/uefi_boot.ld` so the build can emit a freestanding `BOOTX64.EFI` without depending on external firmware libraries.
- Extended `build/build.py` to emit:
  - `build/lunaos.img`: the existing BIOS/QEMU flat image
  - `build/EFI/BOOT/BOOTX64.EFI`: experimental UEFI loader stub
  - `build/lunaos_esp.img`: a generated FAT16 ESP image containing `EFI/BOOT/BOOTX64.EFI`
  - `build/lunaos_disk.img`: a GPT disk skeleton with an EFI System partition, a lafs data partition, and a recovery partition placeholder

### Notes

- This is not yet a complete BIOS+UEFI hybrid installer. The BIOS path remains the validated execution path.
- The new GPT disk image is intended as a hardware-oriented scaffold so the repo can evolve toward true install media instead of only a monolithic lab image.
- Added `build/run_qemu.ps1` fallback lookup for `C:\Program Files\qemu\qemu-system-x86_64.exe` so local validation no longer depends on PATH.
- Added `build/run_qemu_uefi.ps1` for experimental OVMF-based boot attempts against `build/lunaos_disk.img`.

### Validation

- `python .\build\build.py` succeeded after the build changes.
- Output sizes:
  - `build/lunaos.img` = 601600 bytes
  - `build/lunaos_esp.img` = 8388608 bytes
  - `build/lunaos_disk.img` = 67108864 bytes
  - `build/EFI/BOOT/BOOTX64.EFI` = 4123 bytes
- BIOS validation still works through QEMU and reaches the existing LunaOS shell and service startup path.
- UEFI validation with QEMU + `edk2-x86_64-code.fd` now confirms:
  - GPT is readable
  - the EFI System Partition is discoverable as `FS0:`
  - the firmware attempts to boot the disk automatically
  - current failure point is `BOOTX64.EFI` format compatibility: EDK2 reports `Unsupported`

### Next fix target

- Rework the EFI binary emission so the loader accepts `BOOTX64.EFI` as a valid application image.
- Once the EFI image loads, connect it to the shared LunaOS bootstrap path instead of only printing a stub banner.

### Additional UEFI diagnosis

- Added `startup.nsh` to the generated ESP so the UEFI shell automatically attempts `FS0:\EFI\BOOT\BOOTX64.EFI`.
- This removed ambiguity around the boot manager path: the shell itself reports `Script Error Status: Unsupported`, so the failure is definitely in the EFI application image compatibility, not in GPT discovery or ESP mounting.
- Removed the custom linker script from the EFI build path and forced a relocation anchor in `boot/uefi_boot.c`.
- Result: the rebuilt `BOOTX64.EFI` now contains a non-zero base relocation directory, but OVMF still rejects it as `Unsupported`.

### Current UEFI status

- GPT disk image: working
- ESP discovery: working
- automatic boot attempt: working
- direct shell execution of `BOOTX64.EFI`: still rejected
- next likely requirement: emit a stricter PE/COFF EFI image, potentially via a different linker/runtime path than MinGW PE generation

### Tiny EFI control experiment

- Built a separate minimal EFI application (`build/uefi_tiny.efi`) that only calls `OutputString` and does not contain LunaOS bootstrap logic.
- Injected that tiny image into a temporary GPT disk image and booted it with the same OVMF path.
- Result: OVMF still loads the disk and starts Boot0001, but crashes in `BdsDxe` with the same `#UD Invalid Opcode` signature.

### Conclusion from the control

- The current failure is not specific to LunaOS boot logic.
- The current MinGW/PEI emission path itself is producing EFI applications that OVMF begins to process but cannot safely complete.
- The next engineering move is to replace the current EFI image generation path rather than keep tuning the same PE/COFF emitter.

### Manual PE32+ experiment

- Added `build/emit_manual_uefi.py` to generate a minimal PE32+ EFI application without relying on the linker to define section tables or data directories.
- The manual image only contains:
  - `.text`
  - `.data`
  - `.reloc`
- Export/import/exception directories were intentionally zeroed.
- A rebuilt disk image using the manual EFI image (`build/lunaos_disk_manual2.img`) still triggers the same OVMF-side failure in `BdsDxe`.

### Interpretation of the manual experiment

- The failure survives:
  - a tiny UEFI app,
  - a linker-built stripped UEFI app,
  - a hand-authored PE32+ UEFI app,
  - a fully rebuilt ESP/GPT disk image.
- That strongly suggests the immediate blocker is now in the firmware interaction path itself:
  - either OVMF is asserting on some still-invalid PE/COFF property,
  - or the current debug firmware path is using an internal trap/`ud2` style breakpoint during image handoff.

### Debug logging result

- `-debugcon file:... -global isa-debugcon.iobase=0x402` only emitted the early `SecCoreStartupWithStack(...)` line and did not surface a richer assertion message.
- More firmware-facing diagnostics are still needed before the next UEFI change is chosen.

## Phase 8: self-hosted UEFI image path

### Intent

- Replace the default UEFI build output path with LunaOS-owned image generation logic instead of depending on the external linker to shape the EFI application.

### Changes

- Switched `build/build.py` so `build_uefi_stub()` now defaults to `build/emit_manual_uefi.py`.
- The emitted `BOOTX64.EFI` now comes from a hand-authored PE32+ generator controlled in-repo, not from the MinGW linker path.
- The linker-built EFI experiments remain useful for diagnostics, but they are no longer the primary UEFI artifact route.

### Meaning

- BIOS still depends on assembler/compiler tooling as manufacturing tools.
- The UEFI application format itself is now under direct LunaOS control, which is the correct direction for replacing externally imposed boot-image structure with self-defined system machinery.

## Phase 9: self-hosted flat-image export

### Intent

- Remove `objcopy` from the default LunaOS build path.
- Keep compiler/linker usage limited to code generation, while moving final flat-image shaping into repository-owned logic.

### Changes

- Extended `build/build.py` with a native PE section reader:
  - parses the PE header,
  - walks section headers,
  - extracts `.text`, `.rdata`, `.data`, `.bss`,
  - emits the flat stage blob directly in Python.
- Switched `build_c_space()`, `build_rust_space()`, and `build_boot()` away from `objcopy`.
- Removed the now-unused `objcopy` and `mingw_gcc` tool requirements from the active default toolchain path.
- Fixed `build/run_qemu.ps1` and `build/run_qemu_uefi.ps1` so `-drive` arguments expand the generated image paths correctly instead of passing `$image` / `$disk` as literal strings.

### Validation

- `python .\build\build.py` completed successfully after removing `objcopy` from the default image export path.
- BIOS/QEMU boot still works with the self-hosted flat exporter and reaches the full LunaOS startup flow:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[DATA] lafs disk online`
  - `[SYSTEM] atlas online`
  - `[USER] shell ready`
- UEFI/QEMU regression remains unchanged in the important way:
  - firmware still loads GPT and ESP,
  - firmware still starts `Boot0001`,
  - failure remains the same `BdsDxe` `#UD Invalid Opcode` trap.

### Meaning

- LunaOS now owns:
  - GPT disk layout generation,
  - ESP/FAT image generation,
  - EFI application image emission,
  - PE-to-flat stage export.
- The remaining external tools in the active build path are now mostly manufacturing tools:
  - compiler,
  - assembler,
  - linker,
  - symbol dumper.
- The next replacement candidates are:
  - reducing dependence on external symbol extraction,
  - shrinking assembler dependence in early bootstrap where practical,
  - continuing to replace firmware-facing trial paths with LunaOS-authored image/control logic.
