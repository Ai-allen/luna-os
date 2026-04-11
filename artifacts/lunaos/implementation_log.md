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

## Phase 10: lunaloader naming convergence

### Intent

- Stop describing the self-developed loader stack as a generic `boot/` implementation detail.
- Stabilize `lunaloader` as the product-level name for the LunaOS-owned BIOS/UEFI launch system while preserving `BOOT` as space `0`.

### Changes

- Updated repository-facing documentation so:
  - `BOOT` remains the architectural space identifier,
  - `lunaloader` becomes the implementation name for the self-developed launch chain.
- Clarified in `README.md` that `lunaloader` currently spans:
  - BIOS real-mode entry,
  - long-mode handoff,
  - BOOT runtime entry,
  - experimental UEFI image emission,
  - GPT/ESP image generation.

### Meaning

- LunaOS no longer presents its boot path as anonymous startup glue.
- The loader is now treated as a first-class self-developed subsystem with a stable name, which is the right base for future installer, recovery, and dual-path BIOS/UEFI convergence work.

## Phase 11: v1.0 dependency-order convergence

### Intent

- Start aligning the existing prototype with the stricter LunaOS v1.0 boot dependency graph instead of letting BOOT directly pull higher-level services.
- Enforce the first real separation line:
  - `BOOT` brings up only foundation authority,
  - `SYSTEM` owns the ordered startup recipe for the rest of the constellation.

### Changes

- Changed `boot/main.c` so BOOT now directly enters only:
  - `SECURITY`
  - `LIFECYCLE`
  - `SYSTEM`
- Removed BOOT's direct `DATA` launch.
- Updated `system/src/main.rs` so the live startup recipe now begins in this order:
  - `DEVICE`
  - `DATA`
  - `GRAPHICS`
  - `NETWORK`
  - `TIME`
  - `OBSERVE`
  - `PROGRAM`
  - `USER`
  - `AI`
  - `PACKAGE`
  - `UPDATE`
  - `TEST`
- Adjusted SYSTEM's foundation wake/register logic so `DATA` is no longer treated as pre-booted.
- Moved DATA resource allocation to happen only after SYSTEM actually spawns DATA.

### Important runtime constraint exposed

- The current prototype still runs logical spaces in a cooperative single-flow model.
- Under that model, `USER` entering an interactive shell loop during `user_entry_boot()` blocks all later space startup.
- To preserve the v1.0 dependency chain first, `user_entry_boot()` was temporarily converted to:
  - capability initialization,
  - ready banner,
  - prompt print,
  - immediate return.
- The old shell command handlers remain in tree but are marked unused for the moment.

### Validation

- `python .\build\build.py` succeeded after the startup-order changes.
- BIOS/QEMU serial flow now shows the intended foundation sequence:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
- SYSTEM then dynamically brings up the service chain in the new order:
  - `DEVICE`
  - `DATA`
  - `GRAPHICS`
  - `NETWORK`
  - `TIME`
  - `OBSERVE`
  - `PROGRAM`
  - `USER`
  - `AI`
  - `PACKAGE`
  - `UPDATE`
  - `TEST`
- TEST still passes after the change:
  - `[TEST] data armed`
  - `[TEST] time ok`
  - `[TEST] device ok`
  - `[TEST] observe ok`
  - `[TEST] network ok`
  - `[TEST] graphics ok`
  - `[TEST] package ok`
  - `[TEST] update ok`
  - `[TEST] ai ok`
  - `[TEST] lifecycle query ok`
  - `[TEST] system query ok`

### Meaning

- This is the first concrete convergence step from the broad prototype toward the stricter LunaOS v1.0 dependency model.
- BOOT is now closer to being pure `lunaloader + foundation handoff`.
- The next necessary engineering step is to restore USER interactivity without reintroducing a blocking boot path, which means splitting user shell lifetime from the current boot-entry control flow.

## 2026-03-24: DATA via DEVICE and Spawn Debugging

### Landed

- `DATA` no longer talks to ATA PIO directly. Its sector reads and writes now go through the `DEVICE` gate using `device_id = 2` (`disk0`).
- `DEVICE` now exposes two unified hardware records:
  - `serial0`
  - `disk0`
- `include/luna_proto.h` was extended so the disk device is a first-class device kind.
- `DATA` gained explicit capability requests for `/device/disk.read` and `/device/disk.write`.
- `DATA` sector IO was hardened:
  - metadata reads and writes now use a dedicated 512-byte transfer buffer
  - partial structure copies are done explicitly instead of reading raw sectors into undersized structs
- `GRAPHICS` no longer writes directly to VGA memory at `0xB8000` during bring-up; it uses a private shadow text buffer instead.

### What Was Tried And Reverted

- `LIFECYCLE` was temporarily changed to switch each spawned space onto a dedicated stack region from `build/luna.ld`.
- Several versions of the stack bridge were tested:
  - inline asm with register-preserved restore
  - inline asm with explicit ABI clobbers
  - a dedicated assembly helper
- All of those variants regressed the boot path from:
  - `DEVICE -> DATA -> GRAPHICS`
  down to:
  - `DEVICE` only
- QEMU logs showed the same failure pattern during those attempts:
  - timer interrupt `INT=0x08`
  - faulting `RIP=ffffffffffffffff`
  - stack still inside the spawned space stack window
- Because the stack bridge was not yet reliable, the whole stack-switch experiment was reverted to keep the repository on the stronger baseline.

### Current Validation

- `python .\build\build.py` passes.
- BIOS/QEMU boot is currently back to this serial flow:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[BOOT] capability request ok`
  - `[BOOT] capability roster ok`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[SYSTEM] Spawning DEVICE space...`
  - `[LIFECYCLE] Spawning space 5`
  - `[DEVICE] lane ready`
  - `[SYSTEM] Space 5 ready.`
  - `[SYSTEM] Spawning DATA space...`
  - `[LIFECYCLE] Spawning space 2`
  - `[DATA] lafs disk online`
  - `[SYSTEM] Space 2 ready.`
  - `[SYSTEM] Spawning GRAPHICS space...`

### Meaning

- The `DATA -> DEVICE` hardware abstraction step is now real and retained.
- The next blocker is not storage abstraction anymore; it is dynamic-space control flow around `GRAPHICS` bring-up.
- The next serious implementation step should replace the current naive `boot_fn(bootview)` spawn model with a more formal per-space execution handoff, but without reintroducing the unstable stack bridge that was reverted here.
## 2026-03-24 - Graphics Spawn Narrowing

- Confirmed the architecture namespace is back to the fixed 15 spaces only; diagnostic payload is now `DIAG`, not a sixteenth space.
- Reproduced the current BIOS/QEMU boot stop consistently at:
  - `[SYSTEM] Spawning GRAPHICS space...`
- Added targeted bring-up probes in `SYSTEM`, `LIFECYCLE`, and `GRAPHICS` to localize the failure.
- Confirmed the failure occurs after `SYSTEM` has prepared the graphics spawn gate, but before `lifecycle_entry_gate` emits any spawn log for space `4`.
- Confirmed the failure is earlier than `graphics_entry_boot`; `[GRAPHICS] boot 1` never appears.
- Attempted three containment fixes:
  - masked the legacy PIC in `BOOT`
  - forced `cli` in `BOOT`
  - bypassed the shared lifecycle gate page by calling `LIFECYCLE` with a local gate struct from `SYSTEM`
- None of the above changed the stop point yet.
- Found and fixed a real layout bug while debugging:
  - `STACK GRAPHICS` had been placed at `0xB8000`, which overlaps VGA text memory.
  - Moved `GRAPHICS`, `PACKAGE`, `UPDATE`, `AI`, `USER`, and `DIAG` stack tops upward to a safe region in `build/luna.ld`, and mirrored the same addresses in `lifecycle/src/main.rs`.
- Current narrowed blocker:
  - boot reaches `[SYSTEM] graphics spawn 2`
  - control still fails before `lifecycle_entry_gate` logs `[LIFECYCLE] gate 4`
  - this leaves the remaining suspect set concentrated around the `SYSTEM -> LIFECYCLE` call handoff itself, not the `GRAPHICS` space body.
## 2026-03-24 - CID + SEAL Protocol Correction

- Corrected the shared security protocol toward the accepted LunaOS model:
  - `CID` is now treated as a short-lived session capability.
  - `SEAL` is now defined as a one-shot action proof for high-risk calls.
- Expanded `struct luna_gate` in `include/luna_proto.h` with:
  - `target_space`
  - `target_gate`
  - `ttl`
  - `uses`
  - `seal_low`
  - `seal_high`
  - `nonce`
- Updated `SECURITY` to support:
  - `request_capability`
  - `list_capabilities`
  - `issue_seal`
  - `consume_seal`
  - `revoke_capability`
- Fixed the immediate compatibility break introduced by dynamic session issuance:
  - kept session state in `SECURITY`
  - but restored domain-stable `cid_low/cid_high` values so existing spaces that still do local fixed-value checks continue to boot
  - reserved `nonce/ttl/uses` for session tracking during the transition
- Also updated `SYSTEM` and `DATA` to use the expanded `LunaGate` layout so the protocol compiles cleanly again.

### Validation

- `python .\build\build.py` passes again after the correction.
- BIOS/QEMU boot now advances past the former graphics handoff stop:
  - `[LIFECYCLE] gate 4`
  - `[LIFECYCLE] Spawning space 4`
  - `[GRAPHICS] boot 1`
  - `[GRAPHICS] boot 2`
  - `[GRAPHICS] boot 4`
  - `[GRAPHICS] console ready`
  - `[SYSTEM] graphics post-spawn`
  - `[SYSTEM] Space 4 ready.`
- The active blocker has moved again:
  - `DATA` now reports `[DATA] lafs disk fail` under the corrected security semantics
  - boot continues through `NETWORK` and `TIME`
  - the next visible stop is after `[OBSERVE]` spawn begins

### Meaning

- The protocol correction is now structurally in place without breaking the BIOS build.
- The old `SYSTEM -> LIFECYCLE -> GRAPHICS` handoff failure is no longer the main blocker.
- The next work item is to bring `DATA` and the remaining spawned spaces into full `CID + SEAL` compatibility, then continue removing legacy local fixed-cap checks in favor of `SECURITY`-driven validation.

## 2026-03-24 - Stage Layout, Spawn Stack Fix, and Full Foundation Boot Recovery

- Confirmed a real packed-stage overlap in the previous image layout:
  - `DATA` and later spaces had grown enough that the old fixed `SPACE` bases in `build/luna.ld` were no longer safe.
  - Repacked the stage into a denser but non-overlapping layout and moved `MANIFEST` to `0x2C000`.
- Updated all hardcoded manifest readers to the new base.
- Kept the stage within a BIOS-loadable size:
  - current packed stage size is `226` sectors.
- Found the first hard runtime blocker after the layout correction:
  - `PROGRAM` hung during boot before any capability request completed.
  - Root cause was the `LIFECYCLE` spawn bridge leaving the child stack misaligned by 8 bytes.
  - That was harmless for smaller spaces, but `PROGRAM` used a larger prologue with `movaps`, which faulted immediately.
- Fixed the child boot stack handoff in `lifecycle/src/main.rs`:
  - reserve 16 bytes on the child stack
  - store the previous stack pointer explicitly
  - call the child entry with proper SysV alignment
  - restore the parent stack afterward
- Found the second hard runtime blocker after `PROGRAM` was recovered:
  - `UPDATE` still used the obsolete manifest constant `0x39000`.
  - Corrected `update/main.c` to use `0x2C000`.
- Cleaned the temporary probes used during this debugging pass from:
  - `PROGRAM`
  - `UPDATE`
  - `OBSERVE`
  - `SYSTEM`
  - `LIFECYCLE`
  - `SECURITY`
- Left the system in a clean bootable state with the core foundation and service chain running again.

### Validation

- `python .\build\build.py` passes.
- Clean BIOS/QEMU boot now reaches the full spawned service chain:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[BOOT] capability request ok`
  - `[BOOT] capability roster ok`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - `[DATA] lafs disk online`
  - `[GRAPHICS] console ready`
  - `[NETWORK] loop ready`
  - `[TIME] pulse ready`
  - `[OBSERVE] ribbon ready`
  - `[PROGRAM] runtime ready`
  - `[USER] shell ready`
  - `[AI] yuesi stub ready`
  - `[PACKAGE] catalog ready`
  - `[UPDATE] wave ready`

### Meaning

- The boot chain is back to a stable baseline after the `CID + SEAL` protocol correction.
- The main blocker is no longer the space-spawn bridge itself.
- The next major step remains architectural:
  - move more spaces off direct hardware access and through `DEVICE`
  - then tighten `CID + SEAL` from compatibility mode toward true session-and-seal enforcement.

## 2026-03-24 - Security-Centered Capability Validation Cutover (Batch 1)

- Performed the first non-compatibility capability cutover for the core chain:
  - `SECURITY`
  - `DEVICE`
  - `DATA`
  - `LIFECYCLE`
  - `SYSTEM`
  - `PROGRAM`
- Added `LUNA_GATE_VALIDATE_CAP = 6` to the shared protocol.
- Reworked `SECURITY` session issuance so `request_capability` now allocates a real live session slot in `SESSION_TABLE` instead of returning a static low/high pair.
- Session CIDs are now derived from capability domain + nonce + slot/caller mix, then tracked centrally in `SECURITY`.
- Raised default session use budget from `16` to `256` to keep the boot path alive while moving validation out of local fixed-value checks.
- Added `validate_cap()` in `SECURITY`:
  - looks up the live CID
  - checks requested domain
  - checks target space/gate when provided
  - consumes one use on success
- Replaced local fixed `allow_*` capability checks with SECURITY validation in:
  - `device/main.c`
  - `data/src/main.rs`
  - `lifecycle/src/main.rs`
  - `system/src/main.rs`
  - `program/main.c`

### Validation

- `python .\build\build.py` passes after the cutover.
- BIOS/QEMU boot still reaches the full 15-space service chain:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[BOOT] capability request ok`
  - `[BOOT] capability roster ok`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - `[DATA] format store`
  - `[DATA] lafs disk online`
  - `[GRAPHICS] console ready`
  - `[NETWORK] loop ready`
  - `[TIME] pulse ready`
  - `[OBSERVE] ribbon ready`
  - `[PROGRAM] runtime ready`
  - `[USER] shell ready`
  - `[AI] yuesi stub ready`
  - `[PACKAGE] catalog ready`
  - `[UPDATE] wave ready`

### Remaining Work After Batch 1

- Fixed-value capability checks still remain in:
  - `ai/main.c`
  - `graphics/main.c`
  - `network/main.c`
  - `observe/main.c`
  - `package/main.c`
  - `time/main.c`
  - `update/main.c`
- Direct serial I/O still remains outside `BOOT/SECURITY/DEVICE` in:
  - `ai/main.c`
  - `data/src/main.rs`
  - `graphics/main.c`
  - `lifecycle/src/main.rs`
  - `network/main.c`
  - `observe/main.c`
  - `package/main.c`
  - `program/main.c`
  - `system/src/main.rs`
  - `time/main.c`
  - `update/main.c`
  - `user/main.c`
- The subsystem gate structs still do not carry caller-space identity, so this batch centralizes validation in `SECURITY` by CID/domain/target route only. Full holder-space enforcement requires a second protocol pass across the subsystem gate layouts.

## 2026-03-24 - Security-Centered Capability Validation Cutover (Batch 2)

- Completed the second batch of fixed-value capability removal for:
  - `ai/main.c`
  - `graphics/main.c`
  - `network/main.c`
  - `observe/main.c`
  - `package/main.c`
  - `time/main.c`
  - `update/main.c`
- Each of these spaces now validates incoming session CIDs by calling `SECURITY` through `LUNA_GATE_VALIDATE_CAP` with:
  - capability domain
  - presented CID
  - target space
  - target gate/opcode
- This removes the remaining in-source fixed low/high pair authorization checks from the main 15-space service line.

### Validation

- `python .\build\build.py` passes after Batch 2.
- BIOS/QEMU boot still reaches the full service chain with all 15 core spaces online:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - `[DATA] lafs disk online`
  - `[GRAPHICS] console ready`
  - `[NETWORK] loop ready`
  - `[TIME] pulse ready`
  - `[OBSERVE] ribbon ready`
  - `[PROGRAM] runtime ready`
  - `[USER] shell ready`
  - `[AI] yuesi stub ready`
  - `[PACKAGE] catalog ready`
  - `[UPDATE] wave ready`

### Meaning

- The booted service line no longer depends on local hardcoded capability pair checks in its active runtime path.
- `SECURITY` is now the effective validator for the full main service chain.
- Remaining architectural debt has shifted away from capability constants and toward transport/runtime structure:
  - subsystem gate structs still lack caller-space identity
  - direct serial I/O still exists outside `BOOT/SECURITY/DEVICE`
  - several spaces still keep direct serial fallback paths for bootstrap resilience

### Remaining Direct Serial I/O Outside BOOT / SECURITY / DEVICE

- `ai/main.c`
- `data/src/main.rs`
- `graphics/main.c`
- `lifecycle/src/main.rs`
- `network/main.c`
- `observe/main.c`
- `package/main.c`
- `program/main.c`
- `system/src/main.rs`
- `time/main.c`
- `update/main.c`
- `user/main.c`

## 2026-03-24 - Direct Serial Fallback Removal (Service Batch)

- Removed direct serial fallback code from these non-foundation spaces:
  - `ai/main.c`
  - `graphics/main.c`
  - `network/main.c`
  - `observe/main.c`
  - `package/main.c`
  - `program/main.c`
  - `time/main.c`
  - `update/main.c`
  - `user/main.c`
- In this batch:
  - `ready` output in those spaces now goes through `DEVICE` only
  - the old serial fallback branches were deleted
  - the local `outb/inb/serial_write` helpers were removed from those files
- `USER` was also cleaned further:
  - help text now prints through `DEVICE`
  - space-list output no longer uses direct serial single-character writes
  - local serial input helpers were removed
  - shell capability failure no longer falls back to direct serial

### Validation

- `python .\build\build.py` passes after the service batch cleanup.
- BIOS/QEMU boot still reaches the full 15-space service chain:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - `[DATA] lafs disk online`
  - `[GRAPHICS] console ready`
  - `[NETWORK] loop ready`
  - `[TIME] pulse ready`
  - `[OBSERVE] ribbon ready`
  - `[PROGRAM] runtime ready`
  - `[USER] shell ready`
  - `[AI] yuesi stub ready`
  - `[PACKAGE] catalog ready`
  - `[UPDATE] wave ready`

### Remaining Direct Serial I/O Outside BOOT / SECURITY / DEVICE After Service Batch

- `data/src/main.rs`
- `lifecycle/src/main.rs`
- `system/src/main.rs`

## 2026-03-24 - Desktop Foundation Direction

- Rechecked the LunaOS space specification against the current runtime state.
- Current state is still a service-line OS skeleton, not a desktop OS comparable to Windows/macOS/Linux.
- The next meaningful product milestone is no longer just "boot all spaces", but:
  - enter a visible session shell
  - render a managed scene
  - launch a program cell from the session
  - return program output into the session surface
- The minimal Luna desktop closure is therefore defined as:
  - `DEVICE`: input/display lanes
  - `GRAPHICS`: surfaces + scene weaving, not just single-character draw
  - `USER`: session shell and launcher
  - `PROGRAM`: cell raising and output feed
  - `AI`: optional session assistant, not the only interaction path
- Planned desktop-first milestones:
  - add a small surface/window model to `GRAPHICS`
  - let `USER` build a shell scene instead of serial-only prompt semantics
  - let `PROGRAM` expose app output as a routed stream
  - add one visible shell app/launcher path that proves "enter system and run app"

## 2026-03-24 - Desktop Shell and Embedded App Expansion

- `GRAPHICS` now mirrors drawn character cells into VGA text memory instead of keeping only an internal shadow buffer.
- `USER` now draws a minimal LunaOS desktop shell through `GRAPHICS` on boot:
  - top bar
  - launcher strip
  - session footer
  - launcher labels for `Console`, `Files`, `Notes`, and `Hello`
- Expanded the embedded application set from one app to four apps:
  - `hello`
  - `files`
  - `notes`
  - `console`
- Added sources for:
  - `apps/files_app.c`
  - `apps/notes_app.c`
  - `apps/console_app.c`
- Extended image layout and manifest to carry multiple app slots:
  - `APP HELLO`
  - `APP FILES`
  - `APP NOTES`
  - `APP CONSOLE`
- Updated `PROGRAM` so it no longer hardcodes only `hello`; it now scans the embedded app slots in the manifest and resolves app names against all available embedded `.luna` payloads.

### Validation

- `python .\build\build.py` passes with the expanded app set.
- BIOS/QEMU boot still reaches the full 15-space service chain after app expansion.
- BIOS stage grew from `226` sectors to `249` sectors and still loads successfully through the current `lunaloader-bios` path.

### Current Desktop State

- LunaOS now has:
  - a visible desktop shell scaffold
  - multiple embedded applications
  - a runtime that can resolve more than one app name
- LunaOS still does not yet have:
  - routed app output into graphics surfaces
  - app window ownership
  - input focus and window switching
  - a browser or network-backed document view

## 2026-03-24 - Program-Routed App Output and Clean Multi-App Linking

- Added a program-owned app output bridge instead of letting embedded apps hit COM1 directly.
- Extended `struct luna_bootview` with `app_write_entry`.
- Extended `struct luna_manifest` with `program_app_write_entry`.
- `BOOT` now publishes `program_app_write_entry` into the live bootview before foundation handoff.
- `PROGRAM` now exports `program_app_write(const struct luna_bootview *, const char *)`, which writes app text through the existing `DEVICE_WRITE` lane.
- Reworked all embedded apps to use the bootview-provided app write entry:
  - `apps/hello_app.c`
  - `apps/files_app.c`
  - `apps/notes_app.c`
  - `apps/console_app.c`
- Added dedicated linker scripts for each non-hello embedded app:
  - `apps/files_app.ld`
  - `apps/notes_app.ld`
  - `apps/console_app.ld`
- Updated `build/build.py` to:
  - emit the extra manifest field
  - use per-app linker scripts
  - remove the previous "missing hello_app_entry" linker warnings for non-hello apps

### Validation

- `python .\build\build.py` passes after the protocol and build changes.
- The previous per-app linker warnings are gone.
- BIOS/QEMU boot still reaches the full 15-space service chain.
- During `USER` bootstrap launcher flow, all four embedded apps now execute through `PROGRAM` and emit output through the unified device path:
  - `[CONSOLE] Luna Console preview`
  - `[FILES] Luna Files preview`
  - `[NOTES] Luna Notes preview`
  - `Hello from LunaOS!`

### Result

- LunaOS now has a cleaner desktop-app execution chain:
  - `USER` launches app names
  - `PROGRAM` resolves and starts embedded payloads
  - apps emit text through a program-owned write bridge
  - `DEVICE` remains the hardware-facing output lane
- App output is still text-stream based and not yet bound to owned graphics surfaces or windows.

## 2026-03-24 - Desktop Panel Routing (Minimal Window Seed)

- Extended the desktop shell in `USER` with a fixed workspace region under the launcher.
- The workspace is now divided into four named app panels:
  - `Console`
  - `Files`
  - `Notes`
  - `Hello`
- `PROGRAM` now keeps a small panel map for these desktop apps and tracks a text cursor per panel.
- When an embedded app writes through `program_app_write(...)`, `PROGRAM` now:
  - writes the text to the unified device lane
  - also mirrors the text into the mapped graphics panel via `GRAPHICS`
- This is not a full window system yet, but it is the first concrete step from:
  - "all apps dump into one device stream"
  - toward
  - "apps own visible regions in the desktop scene"

### Validation

- `python .\build\build.py` still passes after the panel-routing change.
- BIOS/QEMU still reaches the full 15-space startup chain.
- Serial validation still shows all four launcher apps executing successfully during `USER` bootstrap:
  - `[CONSOLE] Luna Console preview`
  - `[FILES] Luna Files preview`
  - `[NOTES] Luna Notes preview`
  - `Hello from LunaOS!`

### Remaining Gap

- Panels are still fixed-position text regions, not dynamically created windows.
- There is still no focus, resize, z-order, or input dispatch.
- The next real desktop step is to move from fixed panel slots to a `GRAPHICS`-owned surface/window model.

## 2026-03-24 - Graphics Window Registration Seed

- Extended `GRAPHICS` with a minimal window registry:
  - `LUNA_GRAPHICS_CREATE_WINDOW`
  - `LUNA_GRAPHICS_SET_ACTIVE_WINDOW`
  - `LUNA_GRAPHICS_CLOSE_WINDOW`
- `GRAPHICS` now tracks up to 8 live window records and can draw simple framed window borders with titles.
- `USER` now creates four formal desktop windows during boot:
  - `Console`
  - `Files`
  - `Notes`
  - `Hello`
- `PROGRAM` gained a minimal `LUNA_PROGRAM_BIND_VIEW` path so `USER` can bind app names to view rectangles instead of leaving the entire app routing table hardcoded in one place.

### Validation

- `python .\build\build.py` still passes after the window registration changes.
- BIOS/QEMU still reaches the 15-space service line through `UPDATE`.

### Current Regression

- `USER` bootstrap serial output regressed during this step:
  - `[SYSTEM] Spawning USER space...`
  - a short burst of non-text bytes appears on serial
  - startup then continues and `SYSTEM` still marks `USER` ready
- This means the service chain is still alive, but the `USER` bootstrap path is not yet clean enough to call "stable desktop entry".
- The next correction should focus on the `USER` bootstrap write path around early shell/prompt output after window registration.

## 2026-03-24 - Stable Session Entry vs. Desktop Bootstrap

- Continued pushing LunaOS toward a real desktop/session environment instead of only a service-line boot.
- Reintroduced and tested three escalating desktop bootstrap paths in `USER`:
  - shell + desktop scaffold
  - shell + window creation/bind
  - shell + window creation/bind + embedded app autostart
- The result was clear:
  - any graphics/window bootstrap work performed directly inside `user_entry_boot(...)` polluted the return path and corrupted serial/device output as `USER` handed control back to `SYSTEM`
  - the corruption disappeared again when `USER` was reduced back to a pure session entry path
- Kept the desktop/window code in-tree, but removed it from the `USER` hot boot path for now.
- Current deliberate split:
  - `USER` boot path:
    - request capabilities
    - emit `[USER] shell ready`
    - emit `luna:~$`
  - deferred desktop path:
    - desktop shell painting helpers
    - launcher status helpers
    - window creation/bind helpers
    - app autostart helpers
- This is the correct intermediate state for now:
  - the system again has a clean, stable session entry
  - the desktop/window work remains available for reintroduction through a later session-phase initializer instead of boot-time execution

### Validation

- `python .\build\build.py` passes.
- BIOS/QEMU boot again reaches a clean full-chain startup:
  - `[BOOT] dawn online`
  - `[SECURITY] ready`
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - `[DATA] lafs disk online`
  - `[GRAPHICS] console ready`
  - `[NETWORK] loop ready`
  - `[TIME] pulse ready`
  - `[OBSERVE] ribbon ready`
  - `[PROGRAM] runtime ready`
  - `[AI] yuesi stub ready`
  - `[PACKAGE] catalog ready`
  - `[UPDATE] wave ready`
  - `[USER] shell ready`
  - `luna:~$`

### Meaning

- LunaOS is still far from a Windows/macOS-class desktop OS.
- But the direction is now cleaner:
  - stable session entry first
  - deferred desktop bring-up second
  - application/window ownership third
- The next desktop step should not be "draw more during boot".
- It should be:
  - add a post-boot session initializer inside `USER`
  - let that initializer create windows and bind app views
  - only then reintroduce app launch into the visible desktop scene

## 2026-03-24 - Space Constitution Freeze

- Stopped implementation churn long enough to freeze the architectural rules that must govern all later work.
- Added `design/space_constitution.md` as a stricter companion to `design/space_specs.md`.
- The new constitution formalizes LunaOS as:
  - 15 fixed spaces
  - no extra core space
  - independent spaces
  - collaborative behavior only through explicit, security-reviewed connections
- Captured the four primary LunaOS objects:
  - `Space`
  - `Connection`
  - `CID`
  - `SEAL`
- Added sovereignty rules for all 15 spaces:
  - what each space is allowed to own
  - what each space must never absorb
- Added required collaboration chains for:
  - secure program launch
  - persistent document work
  - desktop session
  - package install
  - update wave
  - AI-assisted action
- Added explicit regression traps so the implementation does not slide back into:
  - hidden central-kernel behavior
  - `USER` absorbing desktop + execution + device roles
  - `PROGRAM` directly controlling hardware/display
  - `AI` becoming an unchecked super-controller
  - non-foundation spaces silently doing direct I/O

### Meaning

- This is not a user-facing feature drop.
- It is a design freeze for the system's identity.
- Future work should now be judged against the constitution first, then against the current prototype baseline.

## 2026-03-24 - DEVICE Reclaims Display Hardware

- Applied the first concrete post-constitution hardware correction:
  - display hardware access is now owned by `DEVICE`, not `GRAPHICS`
- Added `LUNA_DEVICE_KIND_DISPLAY = 3` and `struct luna_display_cell` to the shared protocol.
- `DEVICE` now exposes a third lane:
  - `display0`
- `DEVICE` now handles display cell writes by updating VGA text memory itself.
- `GRAPHICS` no longer writes `0xB8000` directly.
- `GRAPHICS` now:
  - preserves visual semantics
  - keeps its shadow cell map
  - forwards text-cell presentation to `DEVICE` over the existing device gate

### Meaning

- This is an important constitutional correction:
  - `GRAPHICS` owns visible semantics
  - `DEVICE` owns display hardware
- LunaOS is now closer to the intended rule:
  - all hardware belongs in `DEVICE`
  - higher spaces consume hardware only through explicit routes

### Validation

- `python .\build\build.py` passes after the device/display split.
- BIOS/QEMU still reaches the full 15-space startup chain:
  - `[GRAPHICS] console ready`
  - `[USER] shell ready`
  - `luna:~$`

## 2026-03-24 - DATA Logs Move Behind DEVICE

- Continued the hardware-sovereignty correction by removing direct serial output from `DATA`.
- `data/src/main.rs` no longer performs raw COM1 writes for startup or store-mount diagnostics.
- Added a small `device_write(...)` helper inside `DATA` that uses:
  - `LUNA_CAP_DEVICE_WRITE`
  - `LUNA_DEVICE_WRITE`
  - `device_id = 1` (`serial0`)
- `DATA` startup and mount messages now travel through `DEVICE`, including:
  - `[DATA] load super fail`
  - `[DATA] format store`
  - `[DATA] load meta fail`
  - `[DATA] lafs disk online`
  - `[DATA] lafs disk fail`

### Meaning

- `DATA` now respects the rule that hardware is owned by `DEVICE`.
- `DATA` keeps storage semantics only:
  - object store
  - metadata
  - persistence
- serial hardware access is no longer embedded in the `DATA` space runtime.

### Validation

- `python .\build\build.py` passes after the change.
- BIOS/QEMU still reaches the full 15-space startup chain.
- `DATA` still mounts successfully and reports:
  - `[DATA] format store`
  - `[DATA] lafs disk online`

## 2026-03-25 - SYSTEM and LIFECYCLE Hand Off Runtime Logs to DEVICE

- Continued the hardware-sovereignty migration in the two foundation coordinators:
  - `lifecycle/src/main.rs`
  - `system/src/main.rs`
- Both spaces now distinguish between:
  - earliest boot diagnostics
  - post-`DEVICE` runtime orchestration logs
- `LIFECYCLE` now:
  - stores the incoming bootview pointer
  - requests `LUNA_CAP_DEVICE_WRITE` once `DEVICE` has been spawned
  - routes later spawn logs through `DEVICE`
- `SYSTEM` now:
  - requests `LUNA_CAP_DEVICE_WRITE` immediately after `DEVICE` is alive
  - routes subsequent spawn/ready announcements through `DEVICE`

### Meaning

- The remaining direct COM1 usage in these spaces is now reduced to earliest foundation boot only.
- Runtime coordination logs are no longer treated as a special privilege of orchestration spaces.
- LunaOS is closer to the intended rule:
  - hardware belongs in `DEVICE`
  - even foundation spaces must converge toward `DEVICE` once the hardware lane exists

### Validation

- `python .\build\build.py` passes after the migration.
- BIOS/QEMU still reaches the full 15-space startup chain:
  - `[LIFECYCLE] ledger online`
  - `[SYSTEM] atlas online`
  - `[DEVICE] lane ready`
  - ...
  - `[USER] shell ready`
  - `luna:~$`

## 2026-03-25 - TIME Moves Its Clock Source Behind DEVICE

- Continued the hardware-sovereignty cleanup by moving `TIME` off direct `rdtsc`.
- Added a fourth device lane in `DEVICE`:
  - `clock0`
- Extended the shared protocol with:
  - `LUNA_DEVICE_KIND_CLOCK = 4`
- `DEVICE` now serves hardware clock reads through `LUNA_DEVICE_READ` on `device_id = 4`.
- `TIME` now:
  - requests `LUNA_CAP_DEVICE_READ`
  - snapshots its boot clock through `DEVICE`
  - computes pulse ticks from the `clock0` lane instead of touching the hardware timer source directly

### Meaning

- `TIME` now owns time semantics only.
- The hardware clock source has been moved where it belongs:
  - `DEVICE`
- This is the same correction pattern already applied to:
  - `GRAPHICS` -> display hardware
  - `DATA` -> disk hardware

### Validation

- `python .\build\build.py` passes after the `clock0` addition.
- BIOS/QEMU still reaches the full 15-space startup chain.
- `TIME` still boots successfully and reports:
  - `[TIME] pulse ready`

## 2026-03-25 - Component Names Frozen

- Added `design/component_registry.md` as the official self-developed naming registry.
- The registry now freezes the product and subsystem names for:
  - the loader family
  - the 15 fixed spaces
  - core runtime objects
  - `lafs`
  - `Perigee`
  - `LunaGuard`
  - `Luna Desktop`
  - `Luna Suite`
  - `Luna SDK`
  - `YueSi`
- It also explicitly forbids drift back into borrowed identities such as:
  - `lakernel`
  - generic `microkernel` naming as subsystem identity
  - treating `TEST` as a space

### Meaning

- LunaOS now has a naming authority document, not just architectural notes.
- Future code, docs, surfaces, and artifacts can be aligned to one owned vocabulary.
- This reduces the risk of the system drifting back into borrowed platform language.

## 2026-03-25 - Component Registry Expanded to Full-System Vocabulary

- Extended `design/component_registry.md` beyond the initial core set.
- The registry now also names:
  - protocol families
  - storage/object vocabulary
  - desktop and workspace surfaces
  - browser and web stack identities
  - package/update/distribution identities
  - observability and recovery identities
  - identity and trust vocabulary
  - reserved technical terms that LunaOS should avoid as product identity

### Meaning

- LunaOS now has a broader owned vocabulary for:
  - system implementation
  - user-facing surfaces
  - SDK/tooling
  - release artifacts
  - future productivity applications
- This helps keep the project self-defined instead of drifting back into borrowed
  names from existing operating systems.

## 2026-03-25 - Component Registry Expanded Again to Full-System Surface Area

- Expanded `design/component_registry.md` a second time to cover the areas still
  missing from the first pass:
  - release rings and install artifacts
  - boot trust and fallback names
  - session, identity, and presence vocabulary
  - object mutation and archival vocabulary
  - app payload/runtime vocabulary
  - browser, office, media, and collaboration naming
  - enterprise deployment naming
  - command-line and developer workflow names
  - more reserved non-Luna terminology to avoid

### Meaning

- The registry now covers much more of the eventual operating-system surface,
  not just the current prototype core.
- LunaOS now has a stronger owned vocabulary for:
  - shipping artifacts
  - first-party programs
  - desktop/workspace surfaces
  - trust, identity, and sharing flows
  - developer and fleet deployment tools

## 2026-03-25 - Component Registry Expanded Toward Full OS Coverage

- Expanded `design/component_registry.md` again, this time using the functional
  surface area expected from mature desktop operating systems as a reference set,
  while keeping LunaOS-owned names.
- Added naming coverage for:
  - installation and first-run setup
  - accounts, access, and session tools
  - search, file, and content tooling
  - settings and control surfaces
  - monitoring and administration tools
  - accessibility and input aids
  - printing, scanning, and external device tools
  - backup, sync, restore, and rescue tools
  - media, office, productivity, development, and automation surfaces
  - virtualization/sandboxing terminology
  - store, repository, and delivery surfaces

### Meaning

- The registry is now much closer to a complete operating-system naming map,
  not just a prototype-core glossary.
- LunaOS can now keep a self-owned vocabulary while still covering the same
  broad functional domains users expect from full desktop operating systems.

## 2026-03-25 - DEVICE Took Over Clock and Input Lanes

- Added `clock0` and `input0` as formal `DEVICE` lanes.
- Extended `include/luna_proto.h` with:
  - `LUNA_DEVICE_KIND_CLOCK = 4`
  - `LUNA_DEVICE_KIND_INPUT = 5`
- Updated `device/main.c` so:
  - `clock0` exposes the current hardware time source to other spaces
  - `input0` exposes the current serial-backed input source as a temporary
    input lane until dedicated keyboard handling lands
- Updated `time/main.c` so `TIME` no longer reads `rdtsc` directly; it now
  requests capability from `SECURITY` and reads time through `DEVICE`
- Updated `user/main.c` so `USER` no longer treats input as private state; it
  requests `DEVICE_READ`, announces the input lane, and now enters a minimal
  interactive shell loop entirely through `DEVICE`

### Meaning

- `TIME` now owns time semantics, not timer hardware.
- `USER` now owns session semantics, not input hardware.
- `DEVICE` continues to consolidate all machine-facing lanes:
  - `serial0`
  - `disk0`
  - `display0`
  - `clock0`
  - `input0`
- This moves LunaOS closer to the rule that all hardware belongs to `DEVICE`,
  while all other spaces must go through `SECURITY -> DEVICE` to access it.

## 2026-03-25 - DEVICE Took Over the Network Lane

- Added `net0` as a formal `DEVICE` lane and extended
  `include/luna_proto.h` with `LUNA_DEVICE_KIND_NETWORK = 6`.
- Updated `device/main.c` so `DEVICE` now owns the low-level packet lane:
  - `device_id = 6`
  - write stores a packet into the lane buffer
  - read drains the current packet from the lane buffer
- Updated `network/main.c` so `NETWORK` no longer keeps its own underlying
  packet transport state as the source of truth.
- `NETWORK` now requests `DEVICE_READ` and `DEVICE_WRITE` from `SECURITY`,
  and routes packet send/receive through `DEVICE(net0)`.

### Meaning

- `NETWORK` now owns network semantics, not the machine-facing lane itself.
- `DEVICE` now owns the underlying packet lane in the same way it owns:
  - `disk0`
  - `display0`
  - `clock0`
  - `input0`
- This makes the architecture more consistent with the LunaOS rule that all
  hardware and hardware-adjacent machine lanes belong to `DEVICE`.

## 2026-03-25 - USER Shell Began Calling Real Spaces

- Upgraded `USER` from a prompt-only placeholder into a minimal interactive
  shell running through `DEVICE(input0/serial0)`.
- `help` remains local, but:
  - `list-spaces` now calls `LIFECYCLE` and prints live space names
  - `run <app>` now calls `PROGRAM load/start` instead of only echoing text
- This makes `USER` a real session coordinator rather than a static prompt.

## 2026-03-25 - LunaGuard Became an Embedded First-Party App

- Added `apps/guard_app.c` and `apps/guard_app.ld`.
- Extended the manifest, app packing path, and layout so `guard.luna` is now a
  first-class embedded app image.
- Updated `PROGRAM` so it can discover and load `guard` alongside:
  - `hello`
  - `files`
  - `notes`
  - `console`

### Meaning

- LunaOS now has the start of a real first-party security surface, not just the
  idea of `LunaGuard`.
- `USER -> PROGRAM` can now be used to launch a minimal security-center app as
  part of the native LunaOS app set.

## 2026-03-25 - USER Session Grew Into a Real Luna Shell Seed

- Extended `USER` so the shell now has real command paths instead of only local
  placeholder output.
- Added session commands:
  - `list-spaces` -> real call into `LIFECYCLE`
  - `cap-count` -> real call into `SECURITY`
  - `run <app>` -> real `PROGRAM load/start`
  - `list-apps` -> first-party embedded app catalog
- The current built-in app set exposed by the shell is now:
  - `guard`
  - `hello`
  - `files`
  - `notes`
  - `console`

### Meaning

- `USER` is no longer only a prompt surface; it is becoming a true session
  coordinator for LunaOS.
- `LunaGuard` now has a real launch path from the user environment.
- The shell is beginning to expose system structure and first-party app ecology
  through actual space connections.

## 2026-03-25 - Boot Regression Now Uses Auto-Settling QEMU Check

- Added `build/run_qemu_bootcheck.ps1`.
- The new bootcheck path launches QEMU, waits until LunaOS reaches:
  - `[USER] shell ready`
  - `luna:~$`
- It then terminates QEMU automatically and prints the captured boot log.

### Meaning

- Regression checks no longer appear "stuck" simply because QEMU remains open.
- LunaOS now has a more practical automated boot gate for continued iteration.

## 2026-03-25 - USER Can Now Inspect DEVICE Lanes

- Added `list-devices` to the Luna shell.
- `USER` now requests `LUNA_CAP_DEVICE_LIST` from `SECURITY`.
- The shell calls `DEVICE_LIST` and prints the live lane names exported by
  `DEVICE`.

### Meaning

- `DEVICE` is no longer only a hidden implementation space.
- The user environment can now directly inspect the machine-facing lane set,
  reinforcing the LunaOS rule that hardware belongs in `DEVICE`.

## 2026-03-25 - Luna Native Program Suffix Frozen as `.la`

- Officially defined `.la` as the Luna native executable suffix:
  - `Luna Application`
- Kept `.luna` as the distribution/package layer above `.la`
- Updated `design/component_registry.md` so Luna app identity is now two-tier:
  - `.la` for the runnable program unit
  - `.luna` for the packaged app payload
- Updated `build/build.py` so app builds now emit both forms into `build/apps/`
  for the first-party embedded set:
  - `guard.la` + `guard.luna`
  - `hello.la` + `hello.luna`
  - `files.la` + `files.luna`
  - `notes.la` + `notes.luna`
  - `console.la` + `console.luna`

### Meaning

- LunaOS now has a formal answer to the executable suffix question, comparable
  in role to `exe` while staying fully Luna-owned.
- The build output now reflects the naming model instead of only documenting it.

## 2026-03-25 - `.la` Now Participates in Runtime, Not Only Naming

- Extended `USER` so the shell now presents first-party programs as:
  - `guard.la`
  - `hello.la`
  - `files.la`
  - `notes.la`
  - `console.la`
- Updated `run <app>` handling so the shell accepts either:
  - bare app names such as `run guard`
  - Luna executable names such as `run guard.la`
- Updated `PROGRAM` app lookup so embedded app resolution accepts the `.la`
  suffix without changing internal canonical app names.
- Upgraded `guard.la` from a static preview banner into a real first-party
  LunaGuard seed that now queries live system state through space connections:
  - active CID count from `SECURITY`
  - live space count from `LIFECYCLE`
  - live device lane count from `DEVICE`

### Meaning

- `.la` is now part of actual LunaOS runtime behavior instead of only a file
  naming convention.
- `LunaGuard` has started becoming a true security center surface, not just a
  placeholder application name.

## 2026-03-25 - Added Command-Driven QEMU Shell Check

- Added `build/run_qemu_shellcheck.ps1`.
- The new shellcheck path boots LunaOS, feeds shell commands, and validates:
  - `list-apps`
  - `run guard.la`
- It now checks that LunaGuard emits live security-center preview data rather
  than only a static banner.

### Meaning

- LunaOS now has the start of an application-level automated regression path,
  not only a boot-level one.
- The native `.la` execution flow is now testable end-to-end through the shell.

## 2026-03-25 - `files.la` Began Using Real DATA Connections

- Upgraded `files.la` from a static preview into a real DATA-facing app seed.
- `files.la` now:
  - requests `LUNA_CAP_DATA_GATHER` from `SECURITY`
  - calls `DATA` to gather the live object set
  - prints the visible object count through the app output bridge

### Meaning

- LunaOS first-party apps are starting to become true space participants instead
  of only UI placeholders.
- `Files` is now the first native app seed that directly demonstrates
  `PROGRAM -> SECURITY -> DATA` cooperation.

## 2026-03-25 - `notes.la` Began Writing Real DATA Objects

- Upgraded `notes.la` from a static preview into a real DATA-writing app seed.
- `notes.la` now:
  - requests `LUNA_CAP_DATA_SEED` and `LUNA_CAP_DATA_POUR` from `SECURITY`
  - creates a fresh note object in `DATA`
  - writes a seed note payload into that object
  - reports the committed object id back through the app output bridge

### Meaning

- LunaOS now has a first-party native app that performs a real object commit
  through `PROGRAM -> SECURITY -> DATA`.
- The app layer has started to participate in persistence, not only read-only
  previews.

## 2026-03-25 - `files.la` Began Previewing Real Object References

- Extended `files.la` beyond object counting.
- `files.la` now previews the first gathered object references returned by
  `DATA`, so the app starts to resemble a true object browser instead of only a
  counter.

### Meaning

- LunaOS now has a native app that can both inspect the size of the live object
  set and preview concrete object identities through real space cooperation.

## 2026-03-25 - `files.la` Began Previewing Object Content

- Extended `files.la` so it now also requests `LUNA_CAP_DATA_DRAW`.
- `files.la` now:
  - gathers live object references
  - reads back the first object's content span from `DATA`
  - prints a preview of that object's content

### Meaning

- `Files` is no longer only an object counter/id browser seed.
- The native file manager has started to show actual stored object content
  through `PROGRAM -> SECURITY -> DATA`.

## 2026-03-25 - `guard.la` Began Showing Real Space And Lane Previews

- Extended `guard.la` again so it no longer stops at numeric counts.
- `guard.la` now previews:
  - the first live LunaOS spaces returned by `LIFECYCLE`
  - the first exported hardware lanes returned by `DEVICE`

### Meaning

- `LunaGuard` is moving toward a real relationship and system-state console.
- The security center surface now exposes both structure and hardware-facing
  topology, not only abstract counters.

## 2026-03-25 - `guard.la` Began Reading OBSERVABILITY Stats

- Extended `guard.la` so it now also queries `OBSERVABILITY`.
- `guard.la` now reads:
  - total observe entries
  - newest observe level

### Meaning

- `LunaGuard` has started to see the system evidence stream in addition to
  topology and capability counts.
- The security center is now beginning to unify security, structure, hardware,
  and observability views in one first-party surface.

## 2026-03-25 - `guard.la` Began Previewing Observe Records

- Extended `guard.la` again so it now requests `LUNA_CAP_OBSERVE_READ`.
- `guard.la` now previews the first live observe records returned by
  `OBSERVABILITY`, including source space id and message text.

### Meaning

- `LunaGuard` is no longer limited to counters; it is starting to display real
  system evidence records.
- The native security center is moving closer to a true relationship and audit
  console.

## 2026-03-25 - `notes.la` Reached App-Level Readback Verification

- Extended `notes.la` so it no longer stops after `seed + pour`.
- `notes.la` now:
  - requests `LUNA_CAP_DATA_DRAW`
  - reads the freshly written note object back from `DATA`
  - verifies the content matches the committed payload

### Meaning

- LunaOS now has a first-party app that exercises a full `DATA` write-then-read
  verification loop through `PROGRAM -> SECURITY -> DATA`.
- The native app layer has moved from one-way persistence into a true storage
  correctness loop.
## 2026-03-25 - SECURITY capability detail export + LunaGuard cap view

- Extended `struct luna_gate` in `include/luna_proto.h` with `buffer_addr` / `buffer_size`, and added `struct luna_cap_record` so SECURITY can export live session capability details instead of only returning a count.
- Updated `security/src/main.rs`:
  - `LunaGate` now matches the expanded shared protocol.
  - `LUNA_GATE_LIST_CAPS` now writes real capability session records into the shared buffer when a caller provides one.
  - Exported fields include holder space, target space, target gate, remaining uses, ttl, domain key, and CID low/high.
- Upgraded `apps/guard_app.c`:
  - `guard.la` now asks SECURITY for capability details through the shared list buffer.
  - LunaGuard now previews live capability records, including domain name, target space, target gate, uses left, and ttl.
- Regression:
  - `python .\build\build.py` passed.
  - `.\build\run_qemu_bootcheck.ps1` passed.
  - Stable BIOS boot still reaches `[USER] shell ready` and `luna:~$`.

## 2026-03-25 - revoke-cap control path

- Added `LUNA_GATE_REVOKE_DOMAIN` to the shared SECURITY gate protocol.
- Updated `security/src/main.rs`:
  - SECURITY can now revoke all live session CIDs for the calling space matching a requested capability domain.
  - The revoke operation returns the number of session CIDs invalidated.
- Updated `user/main.c`:
  - Shell help now includes `revoke-cap <domain>`.
  - Added `revoke-cap` handling for current live capability domains:
    - `data.seed`
    - `data.pour`
    - `data.draw`
    - `data.gather`
    - `life.read`
    - `device.list`
    - `device.read`
    - `device.write`
    - `observe.read`
    - `observe.stats`
    - `program.load`
    - `program.start`
  - The shell now prints `revoked: NNN` after a revoke action.
- Updated `guard.la` copy to advertise the real shell control path instead of a placeholder next-step line.
- Regression:
  - `python .\build\build.py` passed.
  - `.\build\run_qemu_bootcheck.ps1` passed.

## 2026-03-25 - USER Shell Loop Probe + Stable Prompt Rollback

- Re-tested the dormant `USER` shell loop directly inside `user_entry_boot(...)`.
- Added a first-pass boot-noise suppression path so `input0` would ignore echoed
  startup chatter coming back through the current serial-backed input lane.
- Result:
  - The direct non-returning shell loop is still not compatible with the
    current `LIFECYCLE -> USER` boot bridge.
  - `USER` can safely publish a prompt, but keeping the loop inside the hot
    boot path still destabilizes the spawn handoff.
- Action taken:
  - Restored the stable prompt baseline in `user_entry_boot(...)`.
  - Kept the shell loop code in place as a retained path for a later
    session-phase reactivation, outside the current spawn hot path.

### Meaning

- LunaOS still reaches a clean session entry surface without corrupting the
  15-space startup chain.
- The next shell step should happen as a session-stage behavior, not as a
  foundation-stage non-returning boot routine.

## 2026-03-25 - LunaGuard Log View Shifted to Newest Evidence

- Updated `apps/guard_app.c` so `guard.la` no longer previews the oldest
  observe records in the ring.
- `LunaGuard` now previews the newest observe entries instead.
- Also upgraded the observe preview to resolve `space_id` into LunaOS space
  names where possible, instead of showing only numeric ids.

### Meaning

- The native security center now shows fresher evidence and presents it in
  system language rather than raw debug numbering.
- This moves `LunaGuard` closer to a real operator-facing security surface.

## 2026-03-25 - Stable Chain Preserved After SECURITY Observe Experiment

- Prototyped a direct `SECURITY -> OBSERVABILITY` revoke-event write path.
- The experiment disturbed the foundation boot chain and was fully removed.
- Kept the stable parts:
  - `LUNA_GATE_REVOKE_DOMAIN`
  - `revoke-cap <domain>`
  - `guard.la` capability / seal / observe views
- Removed the unstable direct observe-write branch from `SECURITY`.

### Meaning

- The stable 15-space boot chain remains the hard boundary.
- Security event streaming will need to come back through a safer route than
  the first direct hot-path experiment.

## 2026-03-25 - Files and Notes Grew Into Real lafs Object Programs

- Updated `apps/notes_app.c` so `notes.la` now writes a typed Luna note body
  instead of a generic seed string:
  - `LUNA-NOTE`
  - `title: firstlight`
  - `body: Luna note seed`
- `notes.la` still performs the full object loop:
  - `seed`
  - `pour`
  - `draw`
  - readback verification
- Updated `apps/files_app.c` so `files.la` now previews multiple live objects
  instead of only the first one.
- Added preview sanitization in `files.la` so stored note content is rendered
  as a readable one-line object surface rather than raw multi-line text.
- Rebuilt the image and re-ran `build/run_qemu_bootcheck.ps1`; the stable
  15-space boot chain still reaches:
  - `[USER] shell ready`
  - `[USER] input lane ready`
  - `luna:~$`

### Meaning

- `notes.la` is no longer just a storage probe; it now emits a recognizable
  Luna object form that other native apps can identify and preview.
- `files.la` is no longer just a counter; it is becoming a true `lafs`
  object browser surface for LunaOS.

## 2026-03-25 - Files and LunaGuard Became More Readable Operator Surfaces

- Updated `apps/files_app.c` again so `files.la` now:
  - previews multiple objects
  - sanitizes multi-line content into one-line surface summaries
  - detects `LUNA-NOTE` objects and tags them as `[note]`
  - extracts the `title:` field from note bodies for a more useful preview
- Updated `apps/guard_app.c` so `guard.la` no longer shows raw observe levels
  as numbers only; it now labels evidence with readable levels:
  - `trace`
  - `info`
  - `warn`
  - `fault`
- Rebuilt the image and re-ran `build/run_qemu_bootcheck.ps1`; the stable
  15-space boot chain still lands at the Luna prompt.

### Meaning

- `Files` is starting to understand Luna object forms instead of only raw
  bytes.
- `LunaGuard` is moving from developer-facing counters toward an operator-
  readable security surface.

## 2026-03-25 - LunaGuard Became More Actionable, Notes Fixed Real Readback

- Updated `apps/guard_app.c` so `guard.la` now emits a direct revoke summary
  based on live capability domains, for example:
  - `revoke: data.seed, data.draw, ...`
- This makes the security center surface align more closely with the shell
  control path `revoke-cap <domain>`.
- Also upgraded `guard.la` to report the newest observe level with readable
  names in both the evidence stream and the top-level observe summary.
- Fixed a real application bug in `apps/notes_app.c`:
  - the note body had grown beyond the old 32-byte readback buffer
  - `notes.la` now uses a sufficiently large readback buffer so the app-level
    `seed -> pour -> draw -> verify` loop remains correct
- Rebuilt the image and re-ran `build/run_qemu_bootcheck.ps1`; the stable
  15-space boot chain still reaches the Luna prompt.

### Meaning

- `LunaGuard` is no longer just watching; it is starting to expose real
  operator actions in system language.
- `notes.la` now preserves a correct application-level persistence loop after
  the move from raw probe text to real typed Luna note content.

## 2026-03-25 - Files and LunaGuard Moved Closer to Daily Operator Tools

- Updated `apps/files_app.c` so `files.la` now extracts both note `title:` and
  note `body:` excerpts from `LUNA-NOTE` objects.
- `Files` previews are now closer to a daily object browser:
  - object ref
  - typed note marker
  - title summary
  - body summary
- Updated `apps/guard_app.c` so `guard.la` now emits direct shell-ready revoke
  commands for the first live capability domains, for example:
  - `revoke-cap data.seed`
  - `revoke-cap device.write`
- This makes `LunaGuard` line up more closely with the user shell control path
  instead of only describing the system from a distance.
- Rebuilt the image and re-ran `build/run_qemu_bootcheck.ps1`; the stable
  15-space boot chain still lands at the Luna prompt.

### Meaning

- `Files` is becoming a real content browser rather than a raw object probe.
- `LunaGuard` is becoming a real operator console rather than a passive status
  screen.

## 2026-03-25 - USER Session Script Replay + Stable Shellcheck Path

- Added a fixed embedded `SCRIPT SESSION` region to the image layout and
  extended `struct luna_manifest` with:
  - `session_script_base`
  - `session_script_size`
- Reworked `USER` boot behavior:
  - kept the stable prompt-first entry path
  - removed the non-returning shell loop from the hot boot path
  - added one-shot session-script replay through `run_session_script()`
- Repacked the upper image layout to eliminate real overlaps between:
  - `SECURITY` and `DATA`
  - `USER` and `DIAG`
  - first-party embedded apps and `MANIFEST`
- Moved `MANIFEST` to `0x38000` and updated all active spaces/apps that still
  used the old hardcoded manifest address.
- Increased SECURITY live session CID capacity from `48` to `128` so the late
  boot spaces no longer exhaust the runtime CID table.
- Reworked `build/run_qemu_shellcheck.py`:
  - it now clones `build/lunaos.img`
  - patches the embedded session script region in the cloned image
  - replays:
    - `list-apps`
    - `run guard.la`
  - validates the resulting shell transcript from the serial log

### Regression

- `python .\\build\\build.py` passed.
- `build/run_qemu_bootcheck.ps1` passed.
- `build/run_qemu_shellcheck.ps1` passed.

### Meaning

- LunaOS now has a stable automated command-path regression without depending
  on host-side interactive serial injection.
- `USER -> PROGRAM -> guard.la` is covered end-to-end through a real shell
  transcript replay while preserving the stable 15-space startup chain.

## 2026-03-25 - Shellcheck Expanded to a Real Data Loop

- Extended `build/run_qemu_shellcheck.py` so the embedded session script now
  replays:
  - `list-apps`
  - `run notes.la`
  - `run files.la`
  - `run guard.la`
- Tightened transcript validation to require a real application data path:
  - `notes.la` must complete its `seed -> pour -> draw -> verify` loop
  - `files.la` must enumerate visible objects and surface the note preview
    with `title=firstlight` and `body=Luna note seed`
- The shell regression is no longer only a command-dispatch check; it now
  covers a real content roundtrip through `USER`, `PROGRAM`, `DATA`, and two
  first-party apps before ending in `guard.la`.

### Meaning

- LunaOS now has an automated proof that shell-launched apps can create typed
  content and a second app can discover and render that content in the same
  boot session.

## 2026-03-25 - Shellcheck Now Verifies a Real Control Action

- Extended `build/run_qemu_shellcheck.py` again so the replayed shell session
  now includes:
  - `cap-count`
  - `revoke-cap program.load`
  - a second `cap-count`
  - `run hello.la`
- Tightened transcript checks so the regression now requires:
  - the initial USER shell capability count
  - a successful revoke acknowledgement for `program.load`
  - a reduced post-revoke capability count
  - a failed post-revoke app launch when `hello.la` is started after the
    loader capability has been removed

### Meaning

- LunaOS shell regression now covers a real operator control path, not just
  observation and app execution.
- The automated transcript proves that a shell-issued policy action changes
  runtime behavior inside the same boot session.

## 2026-03-25 - Shellcheck Expanded to Multiple Revocation Outcomes

- Extended `build/run_qemu_shellcheck.py` one step further so the scripted USER
  session now revokes two different domains in the same boot:
  - `program.load`
  - `device.list`
- Tightened transcript requirements so the regression now proves two distinct
  post-policy outcomes:
  - app launch fails after `program.load` is revoked
  - device enumeration fails after `device.list` is revoked
- The shell transcript also now requires the capability count to step down
  across both revocations:
  - `008 -> 007 -> 006`

### Meaning

- LunaOS now has a stronger automated proof that USER shell control actions can
  remove different classes of powers and that each revoked domain changes the
  affected subsystem behavior immediately.

## 2026-03-25 - Added Cross-Boot Note Persistence Regression

- Added `build/run_qemu_persistencecheck.py` and
  `build/run_qemu_persistencecheck.ps1`.
- The new regression clones the stage image once and runs two separate boot
  sessions against the same cloned disk:
  - boot 1 replays `run notes.la`
  - boot 2 replays `run files.la`
- The check now requires:
  - boot 1 formats the fresh data store and successfully commits a note
  - boot 2 does not reformat the store
  - boot 2 can still discover and preview the note written in boot 1

### Meaning

- LunaOS now has an automated proof that note content survives a reboot and is
  visible to a second app in a later session, not just within the same boot.

## 2026-03-25 - UEFI Cold Boot Reaches BOOTX64.EFI

- Fixed a real x64 UEFI ABI bug in `build/emit_manual_uefi.py`:
  - `EFI_SYSTEM_TABLE.ConOut` was read from the wrong offset
  - the generated EFI app now uses the correct `0x40` offset on x64
- Added `build/run_qemu_uefi_bootcheck.py` and
  `build/run_qemu_uefi_bootcheck.ps1`.
- Reworked `build/run_qemu_uefi.ps1` so it now boots QEMU with both:
  - x86_64 OVMF code firmware
  - a writable vars flash image copied from a template
- The UEFI cold-boot path now successfully:
  - starts from `build/lunaos_disk.img`
  - enters OVMF
  - loads `\EFI\BOOT\BOOTX64.EFI`
  - executes the LunaOS EFI app and emits `LunaOS UEFI manual` on serial

### Meaning

- LunaOS no longer dies inside firmware before the disk boot path reaches its
  own EFI binary.
- The remaining UEFI gap is now the handoff from the EFI app into the full
  LunaOS runtime, not basic GPT/ESP discovery or BOOTX64 execution.

## 2026-03-25 - LAFS Grew Format-State and Recovery Semantics

- Upgraded `data/src/main.rs` so `lafs` is no longer only a raw object table:
  - added metadata checksum tracking over the object table
  - added explicit clean/dirty store state in the superblock reserve area
  - metadata-changing operations now mark the store dirty before mutation and
    commit it back clean after the write completes
- Added mount-time integrity behavior:
  - DATA now validates the on-disk layout fields against the compiled format
  - mismatched layout metadata triggers a full reformat
  - checksum mismatch or dirty shutdown triggers repair scan / replay behavior
- Added a minimal object-table scrubber:
  - invalid live records are cleared
  - impossible slot indices, sizes, timestamps, and object ids are rejected

### Meaning

- `lafs` is starting to look like a real Luna-native disk format rather than a
  bare persistence experiment.
- LunaOS now has the beginnings of its own crash-recovery and metadata
  integrity semantics, which is the right direction for a self-developed OS
  storage layer.

## 2026-03-25 - USER Shell Can Inspect LAFS State

- Added `LUNA_DATA_STAT_STORE` as a read-only DATA gate opcode for `lafs`
  format introspection.
- Extended `user/main.c` with a new shell command:
  - `store-info`
- `store-info` now surfaces core Luna-native disk-format state directly from
  DATA:
  - format version
  - live object count
  - clean / dirty state
  - mount count
  - format count
  - nonce cursor
- Extended `build/run_qemu_shellcheck.py` so automated shell regression now
  requires the `store-info` transcript before application launches begin.

### Meaning

- LunaOS now exposes its own native disk-format state as a first-class operator
  concept rather than hiding it behind boot logs.
- `lafs` is becoming observable as a real system format, not only an internal
  implementation detail.

## 2026-03-25 - Stage Packing Now Fails Fast on Overlap

- Repacked `build/luna.ld` after `DATA` grew past its previous slot and
  started colliding with downstream spaces during boot.
- Updated the manifest and bootview hardcoded runtime addresses to match the
  new packed image layout.
- Added a stage-region overlap check in `build/build.py` that validates:
  - every space payload
  - every packaged app blob
  - the manifest
  - the session-script region
  against the next reserved address before the final image is emitted
- Added generated shared layout constants:
  - `include/luna_layout.h`
  - `include/luna_layout.rs`
  emitted directly from `build/luna.ld` by `build/build.py`

### Meaning

- Future growth in `DATA`, apps, or shell-script support now fails at build
  time with an explicit layout error instead of silently corrupting the runtime
  image and only surfacing inside QEMU.
- Manifest and bootview address changes now flow from one layout source into
  both the C and Rust runtime without manual source edits across the tree.

## 2026-03-25 - LAFS Recovery Replay Has an Automated Dirty-Boot Check

- Added `build/run_qemu_recoverycheck.py` and
  `build/run_qemu_recoverycheck.ps1`.
- The new recovery regression now:
  - boots a fresh image and writes a note through `notes.la`
  - patches the on-disk `lafs` superblock state to `dirty`
  - reboots the same image
  - requires `[DATA] recovery replay` during mount
  - requires `store-info` to report the store back in `clean` state
  - requires `files.la` to still discover and preview the original note

### Meaning

- LunaOS now has an automated proof that its native disk format can recover
  from a dirty mounted state without reformatting away live objects.

## 2026-03-25 - Metadata Repair and Full VM Regression Are Now Scripted

- Added `build/run_qemu_metarepaircheck.py` and
  `build/run_qemu_metarepaircheck.ps1`.
- The new metadata-repair regression now:
  - writes a note into a fresh `lafs` image
  - corrupts the stored metadata checksum
  - reboots and requires `[DATA] metadata repair`
  - verifies `store-info` returns to `clean`
  - verifies `files.la` still reads the note
- Added `build/run_qemu_fullregression.ps1` to run the current VM baseline in
  one pass:
  - build
  - BIOS bootcheck
  - shellcheck
  - persistencecheck
  - dirty recoverycheck
  - metadata repaircheck
  - UEFI bootcheck

### Meaning

- LunaOS now has automated coverage for both native `lafs` recovery branches:
  dirty state replay and metadata checksum repair.
- The VM bring-up path is no longer a loose pile of scripts; it now has a
  single high-signal regression entry point.

## 2026-03-25 - USER Shell Can Now Run a Read-Only LAFS Health Check

- Added `LUNA_DATA_VERIFY_STORE` as a read-only DATA opcode for store-health
  inspection.
- Added `store-check` to the USER shell.
- `store-check` now surfaces:
  - `lafs.health`
  - `lafs.invalid`
  - `lafs.objects`
- Extended shell/recovery/metadata-repair regressions so they now require a
  healthy `lafs` report after fresh boot and after repair paths finish.

### Meaning

- LunaOS operators no longer have to infer disk-format health only from boot
  logs; they can ask the running system for a direct `lafs` integrity summary.

## 2026-03-25 - Invalid Live Records Now Have a Scrub Regression

- Added `build/run_qemu_scrubcheck.py` and
  `build/run_qemu_scrubcheck.ps1`.
- The new scrub regression now:
  - writes a note into a fresh `lafs` image
  - corrupts the first object record so it remains live but invalid
  - refreshes the metadata checksum and marks the store dirty
  - reboots and requires both:
    - `[DATA] recovery replay`
    - `[DATA] scrub objects`
  - verifies the bad record is removed and `store-check` returns to `ok`
- Extended `build/run_qemu_fullregression.ps1` so the one-shot VM baseline now
  also covers record scrubbing.

### Meaning

- LunaOS now has automated proof that `lafs` can self-clean invalid live
  records during recovery rather than only detecting dirty state or checksum
  drift.

## 2026-03-25 - LAFS Now Persists the Last Recovery Outcome

- `data/src/main.rs` now stores the most recent recovery outcome and scrub
  count in the `lafs` superblock.
- `store-info` now surfaces:
  - `lafs.last-repair`
  - `lafs.last-scrubbed`
- Recovery regressions now verify the persisted outcome matches the path that
  actually ran:
  - `replay`
  - `metadata`
  - `scrub`

### Meaning

- LunaOS operators can now inspect not only whether `lafs` is currently
  healthy, but also what the most recent self-repair action actually was.

## 2026-03-25 - Full VM Regression Now Emits a Machine-Readable Summary

- Added `build/run_qemu_fullregression.py`.
- Reworked `build/run_qemu_fullregression.ps1` into a thin wrapper over the
  Python orchestrator.
- The full VM regression now writes
  `artifacts/lunaos/fullregression_summary.json` with:
  - overall pass/fail status
  - per-step duration
  - per-step exit code
  - execution timestamps
- The full VM regression now also writes:
  - `artifacts/lunaos/fullregression_junit.xml`
  - `artifacts/lunaos/fullregression_steps/<step>.log`
  - `artifacts/lunaos/fullregression_report.md`
- The Markdown report includes:
  - overall pass/fail summary
  - per-step timings
  - step log paths
  - current image artifact sizes and SHA-256 hashes

### Meaning

- LunaOS VM bring-up is no longer only a human-read transcript stream.
- The current baseline can now be consumed by future CI, installers, or
  higher-level validation tooling without scraping console text.

## 2026-03-25 - UEFI Now Hands Off Into LunaOS BOOT

- Extended `build/build.py` so the FAT16 ESP now carries a contiguous
  `LUNAOS.IMG` stage payload alongside `BOOTX64.EFI`.
- Reworked `build/emit_manual_uefi.py` from a print-only EFI app into a
  minimal loader that:
  - resolves `EFI_LOADED_IMAGE_PROTOCOL`
  - resolves `EFI_BLOCK_IO_PROTOCOL`
  - reads the embedded stage payload from the ESP into the fixed LunaOS image
    base
  - directly calls `boot_main`
- Tightened `build/run_qemu_uefi_bootcheck.py` so it now requires both:
  - `LunaOS UEFI handoff`
  - `[BOOT] dawn online`

### Meaning

- LunaOS UEFI boot is no longer only a firmware-level proof that
  `BOOTX64.EFI` can print a string.
- The UEFI path now enters the real LunaOS runtime and reaches the same BOOT
  authority that BIOS startup uses.

## 2026-03-25 - UEFI Runtime Scratch Is Now Allocated Dynamically

- Extended `struct luna_manifest` with `bootview_base` and updated BOOT to use
  the manifest-provided bootview address instead of a hardcoded
  `LUNA_BOOTVIEW_ADDR`.
- Reworked `build/emit_manual_uefi.py` so the EFI loader now:
  - allocates the Luna runtime scratch region with `AllocatePages`
  - patches the in-memory manifest gate/buffer/bootview base fields before
    entering BOOT
  - preserves `ImageHandle` in a non-volatile register across EFI calls
- Synced Rust-side manifest layouts in:
  - `system/src/main.rs`
  - `lifecycle/src/main.rs`
  - `data/src/main.rs`
- Fixed `build/run_qemu_fullregression.ps1` so it resolves its Python
  orchestrator relative to `$PSScriptRoot`.

### Meaning

- The UEFI path no longer depends on BIOS-era assumptions that low memory
  around `0x43000` is unused.
- BIOS and UEFI now consume one consistent manifest layout again, so VM
  startup, shell regressions, persistence checks, repair checks, and UEFI
  cold boot all stay green under the same image layout.
## 2026-03-25 - UEFI Boot Now Stops At A Controlled Runtime Boundary

- Probed the post-`[BOOT] dawn online` UEFI path with a longer QEMU/OVMF run and found that the old handoff still crashed if it continued past BOOT.
- Confirmed that the first unstable step was not `SECURITY` itself but touching `bootview` too early under UEFI. The controlled boundary in [boot/main.c](/abs/path/C:/Users/AI/lunaos/boot/main.c) now fires before any `bootview`/gate/buffer scratch writes: when `bootview_base` differs from the fixed BIOS layout (`LUNA_BOOTVIEW_ADDR`), BOOT reports `[BOOT] UEFI runtime pending` and returns instead of dropping into the BIOS-oriented multi-space runtime path.
- Tightened [run_qemu_uefi_bootcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_bootcheck.py) so the official UEFI regression now requires `LunaOS UEFI handoff`, `[BOOT] dawn online`, and `[BOOT] UEFI runtime pending`. It also holds the VM for a short grace window after the match and fails immediately if a firmware exception dump appears in the serial log.
- Added [run_qemu_uefi_stabilitycheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_stabilitycheck.py) and [run_qemu_uefi_stabilitycheck.ps1](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_stabilitycheck.ps1). This longer regression keeps the VM alive after the boundary match and proves the UEFI handoff remains stable for an extended window without a firmware exception.
- Wired the new `uefi-stabilitycheck` step into [run_qemu_fullregression.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_fullregression.py), so the VM baseline now checks both fast UEFI reachability and short-horizon UEFI stability.
- Result: the UEFI path is now honest and stable in VM bring-up. It reaches BOOT, declares the current runtime boundary, and no longer hides a latent post-BOOT crash behind a short timeout.

## 2026-03-25 - Rust Runtime Layouts Now Share One Manifest Definition

- Added [include/luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) as a shared Rust-side source of truth for `LunaBootView` and the full `LunaManifest` layout, including the newer app/session tail fields.
- Updated [data/src/main.rs](/abs/path/C:/Users/AI/lunaos/data/src/main.rs), [lifecycle/src/main.rs](/abs/path/C:/Users/AI/lunaos/lifecycle/src/main.rs), and [system/src/main.rs](/abs/path/C:/Users/AI/lunaos/system/src/main.rs) to consume the shared definition instead of carrying their own drift-prone local copies.
- Revalidated the VM baseline sequentially after the refactor:
  - `python .\build\build.py`
  - `.\build\run_qemu_uefi_stabilitycheck.ps1`
  - `.\build\run_qemu_fullregression.ps1`
- Result: BIOS boot, shell/control, persistence, replay/scrub/metadata repair, UEFI bootcheck, and UEFI stability all remain green while the cross-language runtime layout risk is reduced.

## 2026-03-25 - Shell Can Now Query The Live Space Topology

- Extended the SYSTEM gate protocol with `LUNA_SYSTEM_QUERY_SPACES` in [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h), giving LunaOS a first-class runtime snapshot operation for its own live space table.
- Implemented the new topology query in [main.rs](/abs/path/C:/Users/AI/lunaos/system/src/main.rs). SYSTEM now serializes all active `LunaSystemRecord` entries into the shared list buffer instead of forcing callers to query spaces one at a time.
- Wired USER into that API in [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c):
  - USER now requests a `system.query` capability at boot
  - the shell help surface includes `space-map`
  - `space-map` prints each live space with `state`, `mem`, `time`, and `event`
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so VM shell regression now proves the new control-plane output is real by requiring a live topology transcript before the existing storage/app/capability checks.
- Result: LunaOS now exposes its own multi-space runtime map as queryable system state, not just boot-time serial narration.

## 2026-03-25 - Space Topology Now Includes Booting States

- Extended the SYSTEM state model in [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) with `LUNA_SYSTEM_STATE_BOOTING`.
- Updated [main.rs](/abs/path/C:/Users/AI/lunaos/system/src/main.rs) so `spawn_and_register()` stages a space record as `booting` before entering the target space, then upgrades it to `active` after the normal registration path completes. This means slow or long-running self-initialization is now visible in the control plane instead of looking like a missing space.
- Kept the stage freestanding by adding a local `memcpy` shim after Rust started emitting that symbol during the richer record handling.
- Updated [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so `space-map` renders the new `booting` state.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so VM regression now requires the real transitional transcript line `USER state=booting ...` before the USER space fully returns control to SYSTEM.
- Revalidated with:
  - `python .\build\build.py`
  - `.\build\run_qemu_shellcheck.ps1`
  - `.\build\run_qemu_fullregression.ps1`
- Result: LunaOS no longer treats runtime topology as a list of only finished spaces; it now exposes in-flight startup as a first-class observable state.

## 2026-03-25 - Space Topology Now Carries Readable Milestone Events

- Expanded the SYSTEM runtime map in [main.rs](/abs/path/C:/Users/AI/lunaos/system/src/main.rs) so each space record now carries a short 8-byte milestone token, for example:
  - `BOOTLIVE`
  - `SECREADY`
  - `LAFSDISK`
  - `USRBOOTI`
- SYSTEM now writes those tokens both for stable ready states and for transitional booting states, so a shell snapshot can explain not just whether a space exists, but what milestone it most recently crossed.
- Updated [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so `space-map` decodes printable event words directly instead of dumping raw decimal values.
- Because SYSTEM grew again, [luna.ld](/abs/path/C:/Users/AI/lunaos/build/luna.ld) was repacked to move `PROGRAM` and all subsequent runtime/app/manifest regions forward one page, preserving clean build-time non-overlap.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so VM shell regression now proves the readable milestone tokens are present in the topology transcript.
- Revalidated with:
  - `python .\build\build.py`
  - `.\build\run_qemu_shellcheck.ps1`
  - `.\build\run_qemu_fullregression.ps1`
- Result: LunaOS runtime topology is no longer just a flat list of spaces and counters; it now exposes a compact, system-native event vocabulary that describes what each live space is doing.

## 2026-03-29 - Full VM Regression Restored To Green On The Current App/Desktop Build

- Re-ran the full VM baseline on `2026-03-29` after the recent desktop/app and UEFI loader changes accumulated in the worktree.
- Confirmed the generated report at [fullregression_report.md](/abs/path/C:/Users/AI/lunaos/artifacts/lunaos/fullregression_report.md) is now green again with all 9 steps passing:
  - `build`
  - `bootcheck`
  - `shellcheck`
  - `persistencecheck`
  - `recoverycheck`
  - `scrubcheck`
  - `metarepaircheck`
  - `uefi-bootcheck`
  - `uefi-stabilitycheck`
- Removed the unused `boot_desktop()` helper in [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so the baseline no longer carries the current `-Wunused-function` warning in USER.

### Meaning

- The active LunaOS worktree is back to a trustworthy regression baseline under both BIOS and UEFI bring-up, instead of relying on the older failed `2026-03-29` summary.
- The current desktop/app staging changes are not just buildable; they survive the full storage, shell, recovery, scrub, metadata-repair, and UEFI validation chain.

## 2026-03-29 - Desktop Work Is Now Reframed As Architecture, Not Demo Surfaces

- Added [desktop_v1_architecture.md](/abs/path/C:/Users/AI/lunaos/design/desktop_v1_architecture.md) to define the first real desktop target for LunaOS.
- The new design explicitly calls out the current limitation: the existing framebuffer launcher/window demo is a proving rig, not a desktop environment.
- It freezes the Desktop V1 cut lines:
  - `GRAPHICS` owns composition, surfaces, hit testing, and presentation
  - `USER` owns shell policy, workspace state, theme policy, and focus policy
  - `PROGRAM` owns app launch and view binding
  - `PACKAGE` owns launcher-visible app metadata
  - `DATA` owns theme/workspace persistence
- It also fixes the implementation order for desktop work:
  - detach shell policy from `GRAPHICS`
  - add theme/workspace records
  - normalize package metadata and app/window binding
  - build real shell surfaces
  - then add app-facing UI baseline contracts
- Updated [component_registry.md](/abs/path/C:/Users/AI/lunaos/design/component_registry.md) so the desktop architecture now has named first-class contracts and surfaces:
  - `Luna Scene Contract`
  - `Luna Theme Contract`
  - `Luna Workspace Contract`
  - `Luna View Contract`
  - `orbit launcher`
  - `dock rail`

### Meaning

- LunaOS desktop work now has a concrete target and cut lines instead of continuing as loosely coupled shell/renderer experiments.
- Future implementation can now be judged against a real architecture document: if work keeps shell policy inside `GRAPHICS` or keeps launcher/app state hardcoded in source, it is the wrong direction.

## 2026-03-29 - USER Now Pushes Desktop Shell Metadata Into GRAPHICS

- Added `LUNA_DESKTOP_ENTRY_CAPACITY`, `luna_desktop_entry`, and `luna_desktop_shell_state` in [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) as the first explicit shell-to-compositor desktop metadata contract.
- Updated [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so USER now owns the launcher-visible desktop entries and passes that shell state to GRAPHICS on desktop render requests instead of assuming the renderer owns those labels.
- Updated [main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so GRAPHICS now consumes shell-supplied desktop entry labels and icon positions for icon rendering, launcher rendering, and icon hit testing.
- The immediate cut is still small on purpose:
  - app launch ownership remains in USER
  - package metadata is not yet the source of truth
  - theme/workspace persistence is not yet implemented
- But one important boundary has changed:
  - launcher-visible desktop metadata is no longer baked only into GRAPHICS

### Meaning

- Desktop policy has started moving out of the compositor.
- This is the first concrete implementation step toward the architecture in [desktop_v1_architecture.md](/abs/path/C:/Users/AI/lunaos/design/desktop_v1_architecture.md): `GRAPHICS` is becoming a renderer for shell-owned state instead of the place where shell state is invented.

## 2026-03-29 - Desktop Architecture Is Now Explicitly Held To A Commercial Product Bar

- Updated [desktop_v1_architecture.md](/abs/path/C:/Users/AI/lunaos/design/desktop_v1_architecture.md) to state a hard rule: LunaOS desktop work is not allowed to optimize for demo completeness, teaching clarity, or toy-OS aesthetics.
- Added a new `Product Bar` section that defines Desktop V1 as a step toward a commercial operating system rather than a visible bring-up surface.
- Added `Product Principles` that now govern desktop implementation:
  - task before ornament
  - direct manipulation
  - stable spatial model
  - consistency over cleverness
  - one system language
  - failure must stay contained
- Added a new `Phase 6: Commercial interaction baseline` so the roadmap now explicitly requires:
  - real pointer and keyboard workflow validation
  - at least one first-party app that supports a real end-to-end task
  - removal of shell/debug leakage from default desktop boot
  - user-workflow regressions rather than only boot/storage transcript checks
- Extended the immediate repository checklist so future work must add workflow-level regressions and continue removing shell/developer affordances from the default desktop session.

### Meaning

- LunaOS desktop work now has a written anti-demo rule inside the primary architecture document.
- Future desktop changes can be rejected on a clear basis: if they improve visible output without improving user-operable workflows, they are not progress toward the product target.

## 2026-03-29 - Session-Script Prompt Duplication Removed

- Fixed duplicated `luna@operator:~$ luna@operator:~$` prompt emission in [main.c](/abs/path/C:/Users/AI/lunaos/user/main.c).
- Root cause: `run_session_script()` owned prompt emission even though prompt ownership should belong to the steady-state interactive shell.
- The scripted path no longer emits shell prompts at all; it only replays command text and output, and `shell_loop()` remains the single owner of the interactive prompt.

### Meaning

- Script-driven regression sessions no longer leave the duplicated `luna@operator:~$ luna@operator:~$` transcript noise.
- Prompt ownership is now explicit: scripted replay is transcript-only, interactive shell startup prints the steady-state prompt.

## 2026-03-29 - Driver Roadmap Is Now A First-Class Product Track

- Added [device_driver_roadmap.md](/abs/path/C:/Users/AI/lunaos/design/device_driver_roadmap.md) to define the minimum self-developed driver path required for LunaOS to move beyond a QEMU-bound bring-up prototype.
- The new design makes one hard rule explicit: "boots in QEMU" is not allowed to stand in for real hardware ownership.
- The roadmap freezes the first commercial hardware order of operations:
  - platform discovery and timing
  - storage ownership through `AHCI` and then `NVMe`
  - human input ownership through `PS/2` and then `USB HID`
  - display ownership through lane-backed scanout
  - baseline network ownership for `net0`
- Updated [desktop_v1_architecture.md](/abs/path/C:/Users/AI/lunaos/design/desktop_v1_architecture.md) so the desktop target now explicitly depends on driver maturity rather than pretending shell work can advance independently of hardware ownership.
- Updated [component_registry.md](/abs/path/C:/Users/AI/lunaos/design/component_registry.md) with the official names:
  - `Luna Lane Contract`
  - `Luna Lane Stack`
  - `lane census`

### Meaning

- Driver work is now written into the same product bar as desktop work.
- Future LunaOS progress can no longer claim "commercial direction" while leaving storage, input, display, timing, and network ownership undefined.

## 2026-03-29 - Lane Census Protocol Added For DEVICE Driver Bring-Up

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) with:
  - `LUNA_DEVICE_CENSUS`
  - `luna_lane_record`
  - lane class, driver family, and lane state flag enums
- Mirrored the public constants and record in [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs).
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now exposes a structured lane census instead of only a flat device-name list.
- The first census formalizes the current bring-up reality:
  - `serial0` -> `uart16550`
  - `disk0` -> dynamic `uefi-block` or `ata-pio`
  - `display0` -> `vga-text`
  - `clock0` -> `rdtsc`
  - `input0` -> `ps2-kbd`
  - `net0` -> `soft-loop`
  - `pointer0` -> `ps2-mouse`
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) with a new `lane-census` shell command so the runtime can print lane kind, class, driver family, and state flags through the same gate contract.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so shell regression now checks for structured lane census output instead of only relying on the old coarse `device lanes: 7` signal.

### Meaning

- LunaOS now has a concrete bridge between the driver roadmap and the running system.
- Future driver work can extend a real lane/discovery protocol instead of inventing device metadata ad hoc in each space.

## 2026-03-29 - PCI Bus Discovery Added To DEVICE

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) with:
  - `LUNA_DEVICE_PCI_SCAN`
  - `luna_pci_record`
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now performs PCI configuration-space discovery through the standard `0xCF8/0xCFC` path and exposes paged scan results through the device gate.
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) with a new `pci-scan` command that prints detected PCI functions as `bus:slot.function`, class family, vendor ID, device ID, and class tuple.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so shell regression now validates both lane census and PCI bus discovery.

### Meaning

- LunaOS driver work now includes real bus discovery rather than only hardcoded lane declarations.
- This is the first concrete step toward controller-specific drivers such as AHCI and PCI-backed network/display paths.

## 2026-03-29 - Desktop Launch Metadata Moved Into PACKAGE

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) so desktop/package records now carry:
  - startup flags
  - preferred window origin
  - preferred window size
  - window role
- Updated [package/main.c](/abs/path/C:/Users/AI/lunaos/package/main.c) so the built-in first-party catalog is now the source of truth for desktop launch metadata instead of only icon names and labels.
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so desktop shell state mirrors the richer package metadata and desktop startup now launches apps marked with `LUNA_PACKAGE_FLAG_STARTUP` instead of hardcoding `files.la` and `console.la`.
- Updated [program/main.c](/abs/path/C:/Users/AI/lunaos/program/main.c) so `PROGRAM` now pulls package metadata before binding a window and uses it to apply app label, preferred window position, and preferred window size.
- Adjusted [luna.ld](/abs/path/C:/Users/AI/lunaos/build/luna.ld) so the larger USER payload still fits cleanly in the packed image.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) to reflect the new live capability count seen by LunaGuard after the desktop metadata path came online.

### Meaning

- LunaOS desktop behavior is now materially closer to the Desktop V1 rule that apps declare shell-visible launch properties through package metadata rather than through scattered hardcoded tables.
- `PACKAGE -> USER -> PROGRAM` is now a real metadata path for startup policy and window preferences, which reduces desktop policy drift between the launcher and the runtime.

## 2026-03-29 - CLOCK0 Moved From Raw RDTSC To PIT-Calibrated Time

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_PIT_TSC`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now calibrates `clock0` against the PIT during boot and exports elapsed microseconds instead of exposing raw unscaled `rdtsc` counts.
- Updated the lane census path so `clock0` now reports `driver=pit-tsc` rather than `driver=rdtsc`.
- Updated [time/main.c](/abs/path/C:/Users/AI/lunaos/time/main.c) so `TIME` now treats the `clock0` lane value as a normalized pulse source instead of dividing a guessed TSC value by a hardcoded constant.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) to validate the calibrated clock driver name.
- Revalidated with:
  - `python .\build\build.py`
  - `pwsh -NoProfile -ExecutionPolicy Bypass -File .\build\run_qemu_bootcheck.ps1`
  - `pwsh -NoProfile -ExecutionPolicy Bypass -File .\build\run_qemu_shellcheck.ps1`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- LunaOS timekeeping is now less dependent on a guessed CPU frequency and closer to a real platform-timing driver path.
- `TIME` now consumes a calibrated device-owned clock lane, which is the right direction for later interrupt-controller and timer-driver work.

## 2026-03-30 - DISPLAY0 Metadata Moved Behind DEVICE

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `struct luna_display_info` and `LUNA_LANE_DRIVER_BOOT_FRAMEBUFFER`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` snapshots boot framebuffer state, reports `display0` as `boot-fb` when present, and serves scanout metadata through `LUNA_DEVICE_READ`.
- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so `GRAPHICS` requests `LUNA_CAP_DEVICE_READ` and initializes framebuffer rendering from the `display0` lane before falling back to bootview data.

### Meaning

- LunaOS display bring-up is now less ad hoc because `GRAPHICS` no longer depends primarily on bootloader-private framebuffer fields.
- `DEVICE -> GRAPHICS` is now the primary metadata path for scanout shape, which is the right boundary for later mode-setting and adapter-specific display drivers.

## 2026-03-30 - NET0 Controller Census Moved Beyond Pure Stub

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_E1000`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now class-matches the QEMU PCI NIC and reports `net0` as `e1000-stub` when an Intel `8086:100E` controller is present, while still marking the lane fallback because payload traffic still uses the software loop path.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so shell validation expects the detected NIC family in lane census output.

### Meaning

- LunaOS networking no longer claims to be a generic software-only lane when a concrete virtual NIC is actually present on the PCI bus.
- This tightens the discovery contract ahead of real MMIO/register work for an actual `e1000` data path.

## 2026-03-30 - DISK0 Controller Census Moved Beyond Generic ATA Label

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_PCI_IDE`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now distinguishes `uefi-block`, `piix-ide`, and plain `ata-pio` for `disk0`, instead of flattening every non-UEFI path to a single generic label.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) so shell validation expects the QEMU IDE controller to surface as `piix-ide`.

### Meaning

- LunaOS storage discovery now reports the actual controller family sitting under `disk0`, which is necessary groundwork for later AHCI and NVMe ownership.
- This keeps the lane contract honest: transport remains ATA-sector based today, but controller census is no longer pretending every path is the same device class.

## 2026-03-30 - POINTER0 Event Decoding Moved Into DEVICE

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `struct luna_pointer_event`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now assembles PS/2 mouse packets internally and exposes structured `dx`, `dy`, and `buttons` on `pointer0`.
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so `USER` consumes normalized pointer events instead of reassembling raw 3-byte PS/2 packets in the desktop shell.

### Meaning

- LunaOS input ownership is slightly cleaner because packet framing now lives in the driver lane rather than in desktop policy code.
- This is the right boundary for later replacing PS/2 with USB HID without forcing another rewrite in the shell/runtime layer.

## 2026-03-30 - Added UEFI Driver Census Validation

- Added [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) and [run_qemu_uefi_shellcheck.ps1](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.ps1).
- Updated [run_qemu_fullregression.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_fullregression.py) so full regression now includes a UEFI shell lane-census pass.
- The new UEFI validation explicitly checks:
  - `disk0 -> uefi-block`
  - `display0 -> boot-fb`
  - `net0 -> e1000-stub`

### Meaning

- Driver assertions are now less BIOS-only; the UEFI path has a direct lane-census check rather than relying on boot banners alone.
- This closes an obvious verification gap in the display and storage driver discovery path under firmware handoff.

## 2026-03-30 - NET0 Detection Expanded For q35 UEFI

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_E1000E`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now distinguishes legacy `e1000` from q35/UEFI `e1000e` instead of collapsing the latter back to `soft-loop`.
- Updated [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so the UEFI lane-census expectation matches the q35 machine profile actually used by firmware tests.

### Meaning

- LunaOS NIC discovery now stays truthful across both main virtual machine classes used in regression.
- This removes a real blind spot where UEFI had a concrete PCI NIC present but `net0` still advertised itself as a pure software fallback.

## 2026-03-30 - q35 UEFI Storage Census Now Surfaces AHCI

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_AHCI`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `disk0` now reports `ahci-stub` when the q35 `8086:2922` SATA controller is present, instead of flattening that path back to generic `uefi-block`.
- Kept the current UEFI block handoff as the live IO transport, so the AHCI lane still advertises `fallback` until native register ownership lands.
- Updated [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so firmware regression validates the controller-aware storage census.

### Meaning

- LunaOS storage discovery is now more truthful on the UEFI/q35 path: the lane reports the actual controller family instead of only the firmware transport that happens to be servicing reads today.
- This tightens the storage driver roadmap toward native AHCI ownership without pretending that controller discovery and firmware fallback are the same thing.

## 2026-03-30 - DISPLAY0 Census Now Surfaces The QEMU VGA Adapter

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_QEMU_STD_VGA`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `display0` now class-matches the QEMU `1234:1111` VGA adapter and reports controller-aware lane names:
  - BIOS path: `std-vga-text`
  - UEFI framebuffer path: `std-vga-fb`
- Kept the current rendering path unchanged: `GRAPHICS` still uses the same text or framebuffer transport, but the lane census no longer flattens the adapter identity into only `vga-text` or `boot-fb`.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) and [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so both machine profiles validate the controller-aware display lane names.

### Meaning

- LunaOS display discovery is now closer to the driver roadmap because the lane reports which adapter family is actually present, not just which output mode happened to be active at boot.
- This makes the future mode-setting and scanout-driver path clearer: the controller identity is now visible before native adapter ownership exists.

## 2026-03-30 - INPUT Census Now Surfaces The i8042 Controller

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_I8042_KEYBOARD` and `LUNA_LANE_DRIVER_I8042_MOUSE`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `input0` and `pointer0` now report controller-aware lane families and names:
  - `input0` -> `i8042-kbd`
  - `pointer0` -> `i8042-mouse`
- Kept the existing PS/2 byte-stream and packet decoding path unchanged; this step only corrects lane census so it names the controller stack rather than only the protocol endpoint.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) and [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so both firmware profiles validate the new input lane names.

### Meaning

- LunaOS input discovery is now slightly more honest because the lane census distinguishes the `i8042` controller from the downstream PS/2 device protocol.
- This keeps the lane contract aligned with the driver roadmap: later USB HID work can replace the controller family without pretending the current keyboard and mouse path was controller-agnostic.

## 2026-03-30 - SERIAL0 Census Now Surfaces The Platform UART Bridge

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_LANE_DRIVER_PIIX_UART` and `LUNA_LANE_DRIVER_ICH9_UART`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `serial0` now infers its driver family from the active machine bridge:
  - BIOS/i440fx path: `piix-uart`
  - UEFI/q35 path: `ich9-uart`
- Kept the current COM1 byte transport unchanged; this step only fixes lane census so it reflects the platform bridge that owns the UART window instead of flattening every machine to `uart16550`.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) and [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so both machine profiles validate the platform-aware serial lane names.

### Meaning

- LunaOS platform discovery is now slightly less serial-blind because the lane census exposes which bridge family is backing the UART path on each machine class.
- This keeps `serial0` aligned with the same controller-aware direction already applied to disk, input, display, and network lanes.

## 2026-03-30 - Desktop Window Binding And Geometry Regression Hardened

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_GRAPHICS_QUERY_WINDOW`.
- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so `GRAPHICS` can now report the active window's title, geometry, and minimize/maximize state through the graphics gate.
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) with new scripted desktop commands:
  - `desktop.boot`
  - `desktop.focus`
  - `desktop.window`
- Updated [program/main.c](/abs/path/C:/Users/AI/lunaos/program/main.c) so first-party apps now bind reliably to their intended desktop panels and lazily acquire the graphics capability when a window bind is needed.
- Updated [run_qemu_desktopcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_desktopcheck.py) so desktop regression now verifies:
  - startup windows from package metadata
  - active-window geometry for `Files`, `Console`, `Guard`, `Notes`, and `Hello`
  - minimize/maximize transitions
  - file-browser selection/open flow after real window activation
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- LunaOS desktop automation no longer treats first-party apps as serial-only launches; the regression path now proves that the desktop actually owns and tracks real app windows.
- `PACKAGE -> PROGRAM -> GRAPHICS -> USER` is now covered by a workflow check that inspects window geometry rather than only relying on app transcript text.

## 2026-03-30 - q35 Storage Now Uses Native AHCI Polling IO

- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now:
  - locates the q35 `8086:2922` SATA controller on PCI
  - enables AHCI memory and bus-master access
  - binds a live SATA port through the HBA registers
  - issues single-sector DMA read/write commands through a native AHCI command list, received FIS area, and command table
- Changed `disk0` lane reporting on the UEFI/q35 path from `ahci-stub` with `fallback` to `ahci` with `polling`.
- Updated [build/run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so firmware regression now requires the native AHCI lane name and flags.
- Adjusted [build/luna.ld](/abs/path/C:/Users/AI/lunaos/build/luna.ld) to give the larger `DEVICE` stage enough room after the new AHCI structures and DMA buffers landed.
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_uefi_shellcheck.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- LunaOS storage on q35/UEFI is no longer only controller-aware at census time; `disk0` now owns a real AHCI polling data path for normal sector IO.
- This moves Phase 2 of the driver roadmap out of pure discovery/fallback territory and into actual controller ownership, while still leaving interrupts, NCQ, and broader hardware coverage for later.

## 2026-03-30 - NET0 Now Exposes A Stable Loopback Gate While q35 Polling TX Boots

- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `DEVICE` now:
  - initializes the q35 `8086:10D3` Intel NIC into a minimal polling TX/RX configuration
  - reports the UEFI/q35 lane as `e1000e` with `polling`
  - preserves a staged `NET0` loopback payload in the device gate so shell regression is stable even when hardware RX has not yet produced a matching frame
  - drops the temporary boot-phase serial diagnostics now that the earlier bring-up fault is isolated
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) with the `net.loop` shell command so the user shell can validate `NET0` gate write/read behavior end to end.
- Updated [build/run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) and [build/run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so both firmware profiles require the `net.loop bytes=8 data=luna-net` transcript, while UEFI also validates the `e1000e` polling lane census.

### Meaning

- LunaOS `net0` has moved beyond census-only discovery: the q35 path now owns NIC MMIO and descriptor setup, while the shell-facing device contract has a deterministic loopback path for regression.
- This is still not a full Ethernet stack or verified hardware RX loopback on `e1000e`; it is the first stable bridge between native controller bring-up and user-visible network I/O semantics.

## 2026-03-30 - NET0 Now Reports Runtime Driver State And Ring Telemetry

- Extended [luna_proto.h](/abs/path/C:/Users/AI/lunaos/include/luna_proto.h) and [luna_proto.rs](/abs/path/C:/Users/AI/lunaos/include/luna_proto.rs) with `LUNA_DEVICE_QUERY` and `struct luna_net_info`.
- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so `NET0` now:
  - tracks RX/TX ring indices and packet counters explicitly instead of hardcoding descriptor 0 forever
  - exposes vendor/device ids, MAC address, MMIO base, ring pointers, and packet counters through the new query surface
  - reports the current reality of the q35 path more honestly: controller init and ring programming are live, but TX completion is not yet incrementing the packet counters
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) with a `net.info` shell command that prints the live `NET0` telemetry.
- Updated [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) and [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) so regression now verifies `net.info` on both BIOS and UEFI profiles.

### Meaning

- LunaOS networking is now easier to debug from inside the OS because the shell can inspect live NIC state rather than inferring everything from lane names and loopback success alone.
- The new telemetry also narrows the next driver task: q35 `e1000e` bring-up has progressed to stable MMIO/ring discovery, and the remaining blocker is real TX/RX completion rather than basic controller discovery.

## 2026-03-30 - q35 E1000E Now Shows Real TX Completion

- Updated [device/main.c](/abs/path/C:/Users/AI/lunaos/device/main.c) so the e1000e polling path now reads descriptor completion through `volatile` TX/RX descriptors, allowing DMA-updated status words to be observed correctly by the CPU.
- Marked the receive address entry valid during NIC init so the adapter keeps its MAC filter live while polling is active.
- Revalidated [run_qemu_uefi_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_uefi_shellcheck.py) against the current q35 hardware state:
  - `tx_head=1`
  - `tx_tail=1`
  - `tx_packets=1`
  - `rx_packets=0`

### Meaning

- LunaOS q35 networking is no longer only programming the transmit ring; the adapter now reports an actual TX completion for the shell loopback write path.
- Receive completion is still the remaining blocker, but the driver has moved from “descriptor path written” to “hardware acknowledged transmit work”.

## 2026-03-30 - Desktop Shell Moves From Character Chrome Toward Pixel Desktop

- Expanded [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) from a character-only shell renderer into a framebuffer-aware desktop compositor that now paints:
  - rounded desktop surfaces, top and bottom bars, and shadowed cards directly into the framebuffer
  - pixel window chrome, title bars, colored controls, and a drawn pointer instead of the old text-mode marker
  - per-app content backplates for `Files`, `Notes`, `Guard`, `Console`, and `Hello`, so the current desktop no longer looks like raw serial text pasted into boxed glyph cells
- Reworked the cell renderer so framebuffer mode still preserves the existing window protocol and text overlay path, which means the current apps remain compatible while the desktop visual layer becomes more graphical.
- Shifted [build/luna.ld](/abs/path/C:/Users/AI/lunaos/build/luna.ld) to give the larger `GRAPHICS` image enough room after the new pixel desktop renderer increased the stage size to `55232` bytes.
- Revalidated the changed desktop path with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- LunaOS still does not have a full modern desktop stack yet, but the shell has moved beyond “character output in a window frame” and now owns a real framebuffer-driven visual layer.
- The next desktop step is no longer basic chrome; it is replacing the remaining text-grid content model with pixel-native controls and event views.

## 2026-03-30 - Window Content Areas Gain Pixel-Native Widget Layouts

- Extended [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) with reusable pixel widget helpers for rounded cards, chips, borders, and progress strips.
- Reworked per-window framebuffer content chrome so the built-in apps now render more like actual UI panels:
  - `Files` now gets a sidebar, toolbar chip, and stacked content cards
  - `Notes` now gets a note header chip, lined writing surface, and side card treatment
  - `Guard` now gets summary cards plus metric strips inside a larger status panel
  - `Console` now gets a darker terminal body with framed viewport treatment
  - `Hello` now gets stacked settings cards and footer chip styling
- Shifted [build/luna.ld](/abs/path/C:/Users/AI/lunaos/build/luna.ld) again because the larger framebuffer widget renderer pushed `GRAPHICS` to `58432` bytes.
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- The LunaOS desktop still carries a text protocol internally, but the visible window bodies now read as simple pixel-native applications rather than bare text canvases with decorative frames.
- The next meaningful desktop step is to move interaction semantics into those widgets, instead of only painting them beneath the legacy text layer.

## 2026-03-30 - Launcher, Control Center, And Status Strip Now Use Pixel Panels

- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so the shared desktop surfaces are no longer left behind as text-mode holdouts:
  - the launcher now renders as a shadowed pixel sheet with card-style app rows and top chips
  - the control center now renders summary cards and a progress strip instead of a plain boxed text panel
  - the status strip now renders as a rounded telemetry pill with distinct pixel indicators behind `K/P/window` counters
- Revalidated the updated desktop shell with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- LunaOS now has a more coherent framebuffer desktop language across windows and shell surfaces, not just inside app bodies.
- The next desktop push should focus less on repainting panels and more on making those panels own real pointer-targeted controls and state transitions.

## 2026-03-30 - Pixel Shell Panels Now Have Distinct Click Semantics

- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so the framebuffer shell exposes separate hit targets for:
  - the status pill control toggle area
  - the status pill theme toggle area
  - launcher header dismiss chip
  - control-center launcher toggle card
  - control-center theme toggle card
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so pointer clicks on those hit kinds now perform real actions instead of collapsing everything into generic shell toggles.
- Expanded [build/run_qemu_desktopcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_desktopcheck.py) to verify the new click behavior end to end, including:
  - status-pill theme toggling
  - control-center theme toggling
  - control-center launcher toggling
  - launcher header dismiss handling
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- The pixel shell is no longer only a visual layer; parts of it now own distinct interaction behavior that maps to their rendered controls.
- The next step is to push the same treatment deeper into app bodies so per-app cards and rows stop relying on text-grid-era hit assumptions.

## 2026-03-30 - App Content Cards Now Own Their Own Pointer Actions

- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so application content zones expose dedicated hit targets instead of falling back to generic window-body focus:
  - `Guard` now exposes two separate card hits inside its content area
  - `Notes` now exposes a dedicated side-card hit inside its content area
  - the earlier `Hello` card-hit wiring remains in place and now sits alongside these additional app-local targets
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so those app-body hit targets perform distinct actions:
  - `Guard` content cards now toggle the control center and theme directly from inside the app surface
  - `Notes` content card now applies a pinned note-body preset and refreshes the note view
- Expanded [build/run_qemu_desktopcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_desktopcheck.py) so regression now verifies:
  - `Guard` content-card clicks
  - `Hello` content-card click launching `Console`
  - `Notes` content-card click mutating note content
  - downstream `Files` state reflecting the updated note payload and theme transitions
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- The desktop is now moving past “pixel-painted chrome with shell-only clicks” into per-application interaction zones.
- `Files`, `Notes`, `Guard`, and `Hello` now participate in pointer-targeted content semantics instead of behaving like passive text panels inside styled frames.

## 2026-03-30 - Console Surface Now Participates In App-Body Navigation

- Updated [graphics/main.c](/abs/path/C:/Users/AI/lunaos/graphics/main.c) so the `Console` content area now exposes dedicated hit zones instead of acting like a passive terminal slab.
- Updated [user/main.c](/abs/path/C:/Users/AI/lunaos/user/main.c) so those `Console` body hits can directly launch first-party apps from inside the window content area.
- Expanded [build/run_qemu_desktopcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_desktopcheck.py) to verify the `Console` content click path before the rest of the desktop interaction script continues.
- Revalidated with:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
  - `python .\build\run_qemu_fullregression.py`

### Meaning

- `Console` is no longer only a rendered bridge window; parts of its body now behave like actionable desktop controls.
- The desktop interaction model is now distributed across shell surfaces and multiple app bodies instead of concentrating all real actions in the shell chrome.
