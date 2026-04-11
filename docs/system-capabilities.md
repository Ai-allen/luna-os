# LunaOS System Capabilities

## RC Status

LunaOS is currently frozen as a release candidate: `LunaOS RC3 (2026-04-10)`.

This document records the capabilities that are inside and outside the current
RC promise.

## Boot

- BIOS boot is supported and part of the frozen RC baseline.
- The current frozen boot-visible spaces are:
  - `SECURITY`
  - `LIFECYCLE`
  - `SYSTEM`
  - `DEVICE`
  - `DATA`
  - `GRAPHICS`
  - `NETWORK`
  - `TIME`
  - `OBSERVE`
  - `PROGRAM`
  - `AI`
  - `PACKAGE`
  - `UPDATE`
  - `USER`
- UEFI shell/stability, update apply, inbound, external stack, and full
  regression validation are part of the frozen RC3 baseline.

## Data System (LAFS)

- `LAFS` is the current data store.
- Verified RC properties:
  - version `2`
  - mounted `1`
  - formatted `1`
  - clean state
  - object count `34`
  - nonce `88`
  - health `ok`
  - invalid count `0`
- Verified user-facing data surfaces:
  - `store-info`
  - `store-check`

## Software System (.luna / PACKAGE / PROGRAM)

- `.luna` is the software unit exposed by the package path.
- `PACKAGE` exposes the installed catalog through `list-apps`.
- `PROGRAM` can launch apps from the package-visible set.
- Current RC catalog entries:
  - `Settings`
  - `Files`
  - `Notes`
  - `Guard`
  - `Console`
- Current RC developer bundle path:
  - `tools/luna_pack.py` packs `.luna`
  - required manifest fields: `name`, `source_id`, `version`, `entry_offset`
  - formal minimal example:
    - `tools/luna_manifest.minimal.json`
    - `build/obj/console_app.bin`
  - current frozen shell path:
    - `package.install sample`
    - `list-apps` shows `sample.luna`
    - `run sample` resolves and launches the installed `sample.luna` object directly
    - `package.remove sample`
    - `run sample` returns `launch failed`
- Current RC runtime facts:
  - `run sample` no longer depends on a `sample/sample.luna -> console.luna`
    remap
  - current success path no longer depends on `PROGRAM` embedded fallback
  - `PROGRAM` embedded fallback is still retained in code, but it is outside the
    RC success baseline
  - `PACKAGE` install/index fallback branches are still retained in code, but
    they are outside the current RC success baseline
- Current default runtime surface uses product-facing install/run/remove output
  rather than `[PACKAGE]` / `[PROGRAM]` success-path traces.

## Policy System

- Policy state is visible through shell and settings surfaces.
- Capability revocation is live and observable:
  - `revoke-cap program.load` reduces cap count from `021` to `020`
  - `revoke-cap device.list` reduces cap count from `020` to `019`
- Guard exposes policy-adjacent runtime state through its current security
  center output.

## Communication System (LunaLink)

- `NETWORK` is online as `lunalink`.
- The current RC3 promise includes:
  - `adapter inspect`
  - `local-only loop`
  - `external outbound-only`
  - external inbound minimal closure
  - external message-stack minimal main path through `net.status`,
    `net.connect`, `net.send`, and `net.recv`
- Verified RC network-facing shell outputs include:
  - `network state=ready loopback=available external=outbound-only`
  - `network.entry inspect=net.info verify=net.external diagnostics=net.loop`
  - `net.loop state=ready scope=local-only`
  - `net.loop bytes=8 data=luna-net`
  - `net.external state=ready scope=outbound-only`
  - `net.external bytes=8 data=luna-ext`
  - `net.inbound state=ready scope=external-raw`
  - `network.inbound state=ready entry=net.inbound filter=dst-mac|broadcast event=seen rx_packets=1`
  - `net.status state=ready transport=external-message peer=external`
- The following are explicitly outside the RC3 promise:
  - protocol expansion beyond the current minimal message path
  - generalized multi-peer or multi-channel networking semantics

## Update System

- `UPDATE` space is online.
- The current RC3 promise includes:
  - `update.status` as a reliable status surface
  - `update.apply` as a reliable minimal apply closure
- The following are explicitly outside the RC3 promise:
  - broader update orchestration beyond the current minimal apply closure

## Desktop and Shell Product Surface

- Shell is available after boot.
- Desktop session is available after boot.
- Verified desktop actions in the current freeze baseline:
  - `desktop.boot`
  - `desktop.focus`
  - `desktop.control`
  - `desktop.settings`
  - `desktop.theme`
  - `desktop.launch Settings`
  - `desktop.launch Guard`
  - `desktop.launch Notes`
  - `desktop.launch Files`
  - `desktop.launch Console`
  - `desktop.files.next`
  - `desktop.files.open`
- Current default user surface is product-facing.
- Diagnostics remain available through status and diagnostics paths, but default
  install/run/remove success paths no longer rely on `[PACKAGE]` /
  `[PROGRAM]` traces.

## User System

- First boot without persisted host/user state enters first-setup guidance.
- Verified RC user commands:
  - `setup.status`
  - `setup.init <hostname> <username> <password>`
  - `whoami`
  - `hostname`
  - `home.status`
  - `settings.status`
- Verified RC user-facing outputs include:
  - first-setup guidance
  - active login/session state
  - home ownership
  - structured settings rows that clearly distinguish actionable, read-only, and
    not-ready system state

## Residual Fallbacks

- `PROGRAM` embedded fallback remains retained in code, but it is outside the
  RC3 success baseline.
- `PACKAGE` install/index fallback remains retained in code, but it is outside
  the RC3 success baseline.
