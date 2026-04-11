# LunaOS Component Registry

This document freezes the official names of the self-developed LunaOS components.

LunaOS is not assembled from borrowed platform identities. Its boot chain, spaces,
protocols, stores, system centers, apps, and developer tools must all use LunaOS
owned names.

This registry is a naming authority document. Architecture and implementation must
follow it unless a later constitutional change explicitly replaces an entry.

## Naming Law

- LunaOS has exactly 15 core spaces.
- No additional core space may be named or introduced.
- Diagnostics, payloads, apps, tools, and bootstrap helpers are not spaces.
- Every major subsystem must have:
  - a public product name
  - a concise technical role
  - a fixed relationship to one or more of the 15 spaces

## System Identity

- System product name:
  - `LunaOS`
- Current major line:
  - `LunaOS v1.x`
- Long-range line:
  - `LunaOS v2.x`
- Core philosophy banner:
  - `Independent Spaces, Cooperative Connections`
- short product slogan:
  - `Secure by Structure`
- official system family mark:
  - `Luna Constellation`
- canonical install target name:
  - `Luna Station`
- canonical desktop session name:
  - `Luna Session`

## Release, Media, and Artifact Naming

- primary bootable image:
  - `lunaos.img`
- install image family:
  - `Luna Install Media`
- disk image family:
  - `Luna Disk`
- EFI system image:
  - `Luna ESP`
- update payload family:
  - `wave package`
- recovery payload family:
  - `recovery capsule`
- factory reset payload family:
  - `origin capsule`
- release channel family:
  - `orbit channel`
- stable release ring:
  - `full moon`
- preview release ring:
  - `crescent`
- experimental release ring:
  - `eclipse`
- nightly line:
  - `night tide`

## Boot and Image Chain

- Self-developed loader system:
  - `lunaloader`
- BIOS loader path:
  - `lunaloader-bios`
- UEFI loader path:
  - `lunaloader-uefi`
- Runtime handoff space:
  - `BOOT`
- Embedded layout map:
  - `constellation manifest`
- Whole-image map artifact:
  - `constellation.map`
- Installable disk image family:
  - `luna image`
- Recovery mode family:
  - `Luna Recovery`
- boot measurement record:
  - `dawn record`
- boot trust store:
  - `anchor store`
- boot fallback path:
  - `safe dawn`
- early serial diagnostic stream:
  - `dawn line`

## Core Runtime Objects

- Independent execution realm:
  - `Space`
- Reviewed cross-space relationship:
  - `Connection`
- Session capability grant:
  - `CID`
- Sensitive one-shot action proof:
  - `SEAL`
- Shared cross-space route slab:
  - `Gate`
- Constellation startup view:
  - `bootview`
- explicit cross-space handshake:
  - `link handshake`
- reviewed session route:
  - `route`
- bounded execution unit inside `PROGRAM`:
  - `cell`
- reusable app launch proof:
  - `launch ticket`
- connection teardown proof:
  - `close seal`
- bounded resource promise:
  - `lease`
- reviewed multi-step workflow:
  - `choreography`
- runtime identity card:
  - `space mark`

## Protocol and Contract Names

- system-wide connection model:
  - `Luna Connection Model`
- capability and action proof model:
  - `Luna Capability Model`
- public gate protocol family:
  - `Luna Gate Protocol`
- system object metadata contract:
  - `Luna Object Contract`
- program manifest contract:
  - `Luna Manifest Contract`
- package contract:
  - `Luna Package Contract`
- update wave contract:
  - `Luna Wave Contract`
- observability event format:
  - `Luna Trace Record`
- identity claim format:
  - `Luna Identity Claim`
- inter-space message envelope:
  - `Luna Envelope`
- object mutation contract:
  - `Luna Mutation Contract`
- workspace state contract:
  - `Luna Workspace Contract`
- security consent contract:
  - `Luna Consent Contract`
- browser document contract:
  - `Luna Document Contract`
- collaboration state contract:
  - `Luna Presence Contract`
- desktop scene contract:
  - `Luna Scene Contract`
- desktop theme contract:
  - `Luna Theme Contract`
- desktop workspace contract:
  - `Luna Workspace Contract`
- app window/view contract:
  - `Luna View Contract`
- hardware lane contract:
  - `Luna Lane Contract`

## Fixed Space Set

The following 15 names and IDs are frozen:

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

## Space Product Names

Each space may have an implementation or product identity in addition to its fixed
 architectural name.

- `BOOT`
  - product identity: `lunaloader runtime`
- `SYSTEM`
  - product identity: `atlas`
- `DATA`
  - product identity: `lafs host`
- `SECURITY`
  - product identity: `LunaGuard Core`
- `GRAPHICS`
  - product identity: `Luna Scene`
- `DEVICE`
  - product identity: `lane`
- `NETWORK`
  - product identity: `Perigee`
- `USER`
  - product identity: `Luna Session`
- `TIME`
  - product identity: `pulse`
- `LIFECYCLE`
  - product identity: `ledger`
- `OBSERVABILITY`
  - product identity: `ribbon`
- `AI`
  - product identity: `YueSi`
- `PROGRAM`
  - product identity: `cell runtime`
- `PACKAGE`
  - product identity: `catalog`
- `UPDATE`
  - product identity: `wave`

## Data, Security, and Connection Components

- Self-developed object store:
  - `lafs`
  - expanded: `Luna Attribute File System`
- desktop shell product:
  - `Luna Shell`
- compositor product:
  - `Luna Scene`
- self-developed driver stack:
  - `Luna Lane Stack`
- hardware discovery ledger:
  - `lane census`
- desktop theme family:
  - `moonglass theme`
- workspace restore record:
  - `workspace snapshot`
- launcher surface:
  - `orbit launcher`
- task rail surface:
  - `dock rail`
- Object security policy descriptor:
  - `SPD`
- Self-developed connection fabric:
  - `Perigee`
- Future secure Perigee mode family:
  - `Perigee-Q`
- Security center backend:
  - `LunaGuard Core`
- User-facing security control center:
  - `LunaGuard`
- Capability policy view:
  - `Capability Ledger`
- Connection audit view:
  - `Connection Ledger`
- active capability register:
  - `CID Ledger`
- one-shot proof register:
  - `SEAL Ledger`
- security policy family:
  - `Luna Policy`
- identity family:
  - `Luna Identity`
- user/device trust root:
  - `Trust Anchor`
- secured object share descriptor:
  - `Share Seal`
- future remote object bridge:
  - `Perigee Object Bridge`

## Hardware Lane Naming

Hardware remains inside `DEVICE`. Public hardware routes should be named as lanes.

- serial hardware lane:
  - `serial0`
- disk hardware lane:
  - `disk0`
- display hardware lane:
  - `display0`
- clock hardware lane:
  - `clock0`
- future input lane:
  - `input0`
- future network interface lane:
  - `net0`
- future audio lane:
  - `audio0`
- future camera lane:
  - `camera0`
- future microphone lane:
  - `mic0`
- future printer lane:
  - `print0`
- future bluetooth lane:
  - `radio0`
- future battery and power lane:
  - `power0`
- future usb lane:
  - `usb0`
- future storage expansion lane:
  - `disk1`
- future wireless network lane:
  - `net1`

## Session, Identity, and Presence Naming

- session family:
  - `Luna Session`
- session token family:
  - `session seal`
- active user presence:
  - `Luna Presence`
- account identity:
  - `LunaID`
- device-paired identity:
  - `paired identity`
- trusted session resume:
  - `soft return`
- locked session hold:
  - `frozen gate`
- multi-user switch fabric:
  - `portal ring`
- privacy-protected guest mode:
  - `guest moon`

## Storage and Object Naming

- default object store:
  - `lafs`
- object namespace browser:
  - `Luna Object View`
- local persistent vault:
  - `Luna Vault`
- object snapshot family:
  - `moonshots`
- object revision stream:
  - `tide history`
- object sharing surface:
  - `Luna Share Table`
- encrypted private object class:
  - `private object`
- collaborative object class:
  - `shared object`
- system-owned immutable object class:
  - `anchor object`
- transient in-memory object class:
  - `drift object`
- user-owned workspace object class:
  - `work object`
- object import operation:
  - `ingest`
- object export operation:
  - `release`
- object fork operation:
  - `branch`
- object merge operation:
  - `weave`
- object deletion operation:
  - `shred`
- object archival operation:
  - `bury`
- object restore operation:
  - `recover`

## User Environment and Desktop

- Session environment family:
  - `Luna Session`
- Desktop environment:
  - `Luna Desktop`
- Visual scene/compositor family:
  - `Luna Scene`
- Launcher:
  - `LunaLauncher`
- Settings and control center:
  - `Luna Control Center`
- Security center:
  - `LunaGuard`
- File manager:
  - `LunaFile`
- Terminal:
  - `LunaTerminal`
- Notes application:
  - `LunaNote`
- Text editor family:
  - `LunaEdit`
- Browser family:
  - `LunaBrowse`
- workspace model:
  - `Luna Workspace`
- desktop top bar:
  - `skybar`
- launcher shelf:
  - `dockline`
- notification stream:
  - `signal tray`
- task and focus strip:
  - `focus rail`
- app surface family:
  - `Luna Surface`
- window family:
  - `Luna Window`
- view binding route:
  - `view bind`
- session switcher:
  - `Luna Portal`
- lock screen:
  - `Luna Gate`
- app switcher:
  - `orbit switcher`
- quick command surface:
  - `quick tide`
- desktop search surface:
  - `signal search`
- embedded shell prompt family:
  - `moon prompt`
- notification center:
  - `signal center`
- clipboard history:
  - `echo shelf`

## Productivity and Communication Suite

The long-range first-party application family is:

- suite name:
  - `Luna Suite`

Initial and planned members:

- rich document editor:
  - `LunaWriter`
- table and structured grid editor:
  - `LunaSheet`
- presentation editor:
  - `LunaSlide`
- vector drawing:
  - `LunaDraw`
- PDF viewer and signer:
  - `LunaPDF`
- mail:
  - `LunaMail`
- chat:
  - `LunaChat`
- calling and live presence:
  - `LunaCall`
- local and remote sharing:
  - `LunaShare`
- photo manager:
  - `LunaPhoto`
- music player:
  - `LunaMusic`
- video player:
  - `LunaVideo`
- lightweight game host:
  - `LunaGame`
- bookmark and reading shelf:
  - `LunaShelf`
- calendar and planning hub:
  - `LunaPlan`
- whiteboard and live canvas:
  - `LunaBoard`
- password and secret manager:
  - `LunaKey`
- download manager:
  - `LunaDrop`
- screenshot and capture tool:
  - `LunaCapture`
- office suite family:
  - `Luna Office`
- document collaboration fabric:
  - `Luna Presence`
- spreadsheet formula engine:
  - `grid core`
- presentation scene engine:
  - `deck scene`
- media transcoding family:
  - `Luna Media`
- personal knowledge workspace:
  - `Luna Atlas`

## App Payload and Runtime Naming

- Luna native executable program suffix:
  - `.la`
- meaning of `.la`:
  - `Luna Application`
- `.la` role:
  - single runnable Luna program unit loaded by `PROGRAM`
- package/archive suffix:
  - `.luna`
- `.luna` role:
  - distributable Luna package that may contain one or more `.la` programs plus
    manifest, resources, and signatures
- first-party embedded security center executable:
  - `guard.la`
- bundled hello sample:
  - `hello.luna`
- bundled hello executable:
  - `hello.la`
- bundled files sample:
  - `files.luna`
- bundled files executable:
  - `files.la`
- bundled notes sample:
  - `notes.luna`
- bundled notes executable:
  - `notes.la`
- bundled console sample:
  - `console.luna`
- bundled console executable:
  - `console.la`
- user-installable application class:
  - `Luna App`
- runtime-bound execution unit:
  - `cell`
- app surface binding:
  - `view bind`
- app workspace slot:
  - `panel`
- suspended app state:
  - `rest cell`

## System Centers

The first-party system center family is:

- family name:
  - `Luna Centers`

Members:

- security center:
  - `LunaGuard`
- control/settings center:
  - `Luna Control Center`
- package center:
  - `LunaStore`
- update center:
  - `Luna Update Center`
- observability center:
  - `Luna Observe`
- recovery center:
  - `Luna Recovery`
- installer:
  - `Luna Installer`
- identity and account center:
  - `Luna Identity Center`
- storage and object center:
  - `Luna Data Center`
- device center:
  - `Luna Device Center`
- network center:
  - `Luna Network Center`
- time and schedule center:
  - `Luna Pulse Center`
- workspace center:
  - `Luna Workspace Center`
- identity center:
  - `Luna Identity Center`
- media center:
  - `Luna Media Center`
- recovery control console:
  - `Luna Recovery Console`

## AI and Developer Platform

- AI product identity:
  - `YueSi`
- AI workspace:
  - `YueSi Studio`
- first-party IDE:
  - `LunaIDE`
- first-party debugger:
  - `LunaDebug`
- build tool:
  - `LunaBuild`
- package output format:
  - `.luna`
- developer platform:
  - `Luna SDK`
- public application runtime contract:
  - `Luna App Contract`
- package repository service:
  - `Luna Harbor`
- application publishing path:
  - `Harbor Publish`
- application signing service:
  - `Luna Sign`
- local developer shell:
  - `Luna Dev Shell`
- software development profile:
  - `Luna Forge`
- official command shell:
  - `Luna Shell`
- developer command-line tool:
  - `lunactl`
- build graph generator:
  - `moonforge`
- package manifest editor:
  - `manifest studio`
- app test harness:
  - `tidecheck`
- SDK reference manual family:
  - `Luna Reference`

## Browser and Web Stack Names

- browser product:
  - `LunaBrowse`
- browser engine family:
  - `tidelight`
- document renderer:
  - `selene`
- browser network bridge:
  - `Perigee Web Bridge`
- secure download verifier:
  - `Luna Verify`
- browser session store:
  - `surf cache`
- reading mode:
  - `quiet tide`
- browser tab family:
  - `surf tab`
- browser history shelf:
  - `wake trail`

## Package, Update, and Distribution Names

- package repository family:
  - `Luna Harbor`
- signed package archive:
  - `.luna`
- package catalog view:
  - `Harbor Catalog`
- local package cache:
  - `mooncache`
- update stream family:
  - `wave train`
- staged update candidate:
  - `wave candidate`
- rollback marker:
  - `rewind mark`
- system recovery bundle:
  - `recovery capsule`
- installer media family:
  - `Luna Install Media`
- package install operation:
  - `dock`
- package uninstall operation:
  - `undock`
- package verification operation:
  - `prove`
- package publish operation:
  - `launch`
- signed repository snapshot:
  - `harbor snapshot`
- system rollback operation:
  - `rewind`
- staged system promotion:
  - `rise`
- failed update quarantine:
  - `cold orbit`

## Observability and Reliability Names

- system evidence stream:
  - `ribbon stream`
- structured event family:
  - `ribbon record`
- crash digest:
  - `fault digest`
- health summary:
  - `health slate`
- degraded operating mode:
  - `dim mode`
- recovery operating mode:
  - `recovery mode`
- isolated component restart:
  - `space revive`
- hardware lane restart:
  - `lane revive`
- structured boot failure report:
  - `dawn fault`
- live system degradation marker:
  - `dim mark`
- self-healing attempt:
  - `revive attempt`
- long-term health journal:
  - `health ledger`
- operator export bundle:
  - `evidence pack`

## Identity and Trust Names

- device identity:
  - `Luna Device Identity`
- user identity:
  - `LunaID`
- session proof:
  - `session seal`
- trust root store:
  - `anchor store`
- consent record:
  - `consent ledger`
- security relationship console:
  - `LunaGuard`
- biometric unlock flow:
  - `touch seal`
- delegated trust bundle:
  - `trust packet`
- temporary approval token:
  - `consent seal`

## Collaboration and Sharing Names

- local collaboration workspace:
  - `shared workspace`
- remote collaboration bridge:
  - `Perigee Share Bridge`
- live participant state:
  - `presence bead`
- access invitation:
  - `invite seal`
- temporary share route:
  - `share lane`
- collaboration history:
  - `presence trail`

## Enterprise and Deployment Names

- managed deployment profile:
  - `Luna Fleet`
- enterprise policy pack:
  - `fleet policy`
- device enrollment:
  - `station bind`
- signed provisioning bundle:
  - `fleet capsule`
- administrative overview surface:
  - `Fleet Console`
- long-term support line:
  - `harvest line`

## Installation, Setup, and First-Run Names

- first-run setup flow:
  - `Luna Firstlight`
- language and region setup:
  - `Luna Locale`
- network onboarding flow:
  - `Luna Join`
- account bootstrap flow:
  - `Luna Arrival`
- storage selection flow:
  - `Luna Grounding`
- install progress surface:
  - `dawn progress`
- repair and reinstall flow:
  - `Luna Reforge`
- migration and import flow:
  - `Luna Migration`

## Account, Access, and Session Tools

- account manager:
  - `Luna Accounts`
- sign-in surface:
  - `Luna Sign-In`
- session unlock surface:
  - `Luna Unlock`
- guest session program:
  - `Luna Guest`
- parental and family controls:
  - `Luna Family`
- consent and access review surface:
  - `Consent Board`
- session history viewer:
  - `Session Ledger`

## File, Search, and Content Tools

- universal content search:
  - `Luna Search`
- object finder:
  - `Object Finder`
- quick preview surface:
  - `Luna Preview`
- archive manager:
  - `Luna Archive`
- file transfer monitor:
  - `Transfer Slate`
- trash and restore center:
  - `Luna Reclaim`
- sync conflict resolver:
  - `Merge Board`

## Settings, Control, and System Management

- top-level settings surface:
  - `Luna Settings`
- advanced control surface:
  - `Luna Control Center`
- startup and boot settings:
  - `Dawn Settings`
- storage settings:
  - `Vault Settings`
- display settings:
  - `Scene Settings`
- network settings:
  - `Perigee Settings`
- power settings:
  - `Pulse Power`
- update settings:
  - `Wave Settings`
- security policy settings:
  - `Guard Settings`
- developer settings:
  - `Forge Settings`

## Monitoring, Diagnostics, and Administration

- system monitor:
  - `Luna Monitor`
- task and cell inspector:
  - `Cell Monitor`
- space inspector:
  - `Space Monitor`
- connection inspector:
  - `Connection Monitor`
- resource monitor:
  - `Resource Slate`
- boot log viewer:
  - `Dawn Journal`
- crash and fault viewer:
  - `Fault Board`
- diagnostic bundle exporter:
  - `Evidence Pack`
- maintenance shell:
  - `Luna Maintenance`

## Accessibility and Input Experience

- accessibility center:
  - `Luna Access`
- screen magnifier:
  - `Luna Magnify`
- screen reader:
  - `Luna Voice`
- on-screen keyboard:
  - `Luna Keys`
- captioning surface:
  - `Luna Caption`
- pointer settings:
  - `Pointer Board`
- input method manager:
  - `Input Weave`

## Printing, Scanning, and External Devices

- printer manager:
  - `Luna Print`
- scan manager:
  - `Luna Scan`
- camera app:
  - `Luna Camera`
- device pairing surface:
  - `Lane Pair`
- external display manager:
  - `Display Weave`
- removable media manager:
  - `Media Dock`

## Networking, Internet, and Remote Work

- browser:
  - `LunaBrowse`
- mail:
  - `LunaMail`
- chat:
  - `LunaChat`
- call and conferencing:
  - `LunaCall`
- remote desktop surface:
  - `Luna Remote`
- VPN and secure tunnel surface:
  - `Perigee Tunnel`
- download manager:
  - `LunaDrop`
- web app host:
  - `Surf Host`

## Backup, Sync, and Recovery Tools

- backup center:
  - `Luna Backup`
- sync center:
  - `Luna Sync`
- restore assistant:
  - `Luna Restore`
- snapshot browser:
  - `Moonshot Browser`
- emergency rescue environment:
  - `Luna Rescue`
- rollback chooser:
  - `Wave Rewind`

## Media, Creativity, and Capture

- photo manager:
  - `LunaPhoto`
- music player:
  - `LunaMusic`
- video player:
  - `LunaVideo`
- recording studio:
  - `Luna Studio`
- screenshot tool:
  - `LunaCapture`
- drawing surface:
  - `LunaDraw`
- presentation capture mode:
  - `Stage Cast`

## Office, Knowledge, and Productivity

- office suite family:
  - `Luna Office`
- writer:
  - `LunaWriter`
- spreadsheet:
  - `LunaSheet`
- presentations:
  - `LunaSlide`
- notes:
  - `LunaNote`
- knowledge workspace:
  - `Luna Atlas`
- calendar and planning:
  - `LunaPlan`
- whiteboard:
  - `LunaBoard`
- document signing:
  - `Luna Sign Desk`

## Development, Automation, and Engineering

- shell:
  - `Luna Shell`
- terminal:
  - `LunaTerminal`
- IDE:
  - `LunaIDE`
- debugger:
  - `LunaDebug`
- build tool:
  - `LunaBuild`
- command tool:
  - `lunactl`
- package publisher:
  - `Harbor Publish`
- automation runner:
  - `moonflow`
- SDK documentation set:
  - `Luna Reference`
- test harness:
  - `tidecheck`

## Virtualization, Sandboxing, and Execution

- app runtime:
  - `cell runtime`
- sandbox policy family:
  - `cell policy`
- isolated test cell:
  - `trial cell`
- suspended cell store:
  - `cell cache`
- compatibility layer family:
  - `bridge layer`
- automation and agent sandbox:
  - `agent cell`

## Search, AI, and Intent Surfaces

- AI system persona:
  - `YueSi`
- AI workspace:
  - `YueSi Studio`
- natural-language command surface:
  - `intent line`
- semantic search surface:
  - `signal search`
- system explanation surface:
  - `why view`
- recommendation surface:
  - `suggest rail`

## Software Delivery and Storefront

- package repository:
  - `Luna Harbor`
- app store:
  - `LunaStore`
- package catalog:
  - `Harbor Catalog`
- install queue:
  - `dock queue`
- verification service:
  - `Luna Verify`
- local trusted cache:
  - `mooncache`

## Reserved Technical Terms

- `kernel`
  - not an official LunaOS product identity
- `daemon`
  - avoid for first-party named spaces and centers
- `process`
  - use `cell` where LunaOS runtime semantics are intended
- `service`
  - use only as a generic description, not as a subsystem identity
- `driver`
  - use only as an implementation detail under `DEVICE`, not as a top-level LunaOS identity
- `filesystem`
  - prefer `object store` or `lafs` when LunaOS semantics are intended
- `permission`
  - prefer `CID`, `SEAL`, or `consent` when LunaOS semantics are intended

## Internal Technical Families

- scheduler family:
  - `pulse scheduler`
- space startup recipes:
  - `constellation recipes`
- system-wide crash and evidence stream:
  - `ribbon stream`
- package index model:
  - `catalog index`
- update train and rollback model:
  - `wave train`
- installation topology:
  - `constellation layout`
- space recovery topology:
  - `revive graph`
- desktop surface topology:
  - `scene graph`
- app routing topology:
  - `cell graph`
- session relationship topology:
  - `presence graph`
- update staging topology:
  - `wave graph`
- package dependency topology:
  - `harbor graph`

## Reserved Naming Rules

- `TEST` must never be presented as a space.
- `DIAG` refers only to diagnostic payloads.
- `hello`, `files`, `notes`, `console` are app payload names, not spaces.
- `lakernel`, `microkernel`, and borrowed platform identities are not official LunaOS
  subsystem names.
- avoid using Windows, macOS, Linux, Unix, POSIX, GRUB, Finder, Explorer, taskbar,
  dock, or start-menu names as LunaOS product identities.
- avoid reusing Android, iOS, GNOME, KDE, Chromium, WebKit, systemd, Finder,
  Explorer, Spotlight, Launchpad, Control Panel, or Device Manager names as
  LunaOS first-party identities.
- New names should prefer:
  - short lunar or navigational metaphors
  - one product name per subsystem
  - no overlap with the 15 fixed space names

## Current Priority Names

The following names are immediately active and should be used in code comments,
documentation, artifacts, and future user-facing surfaces:

- `LunaOS`
- `lunaloader`
- `lafs`
- `Perigee`
- `LunaGuard`
- `Luna Desktop`
- `Luna Session`
- `Luna Suite`
- `Luna SDK`
- `YueSi`
- `LunaGuard`
- `Luna Workspace`
- `Luna Harbor`
- `Luna Control Center`
- `LunaBrowse`
- `LunaID`
- `lunactl`
- `Luna Office`
- `Luna Shell`
- `moonforge`
- `Luna Installer`
- `Luna Recovery`
