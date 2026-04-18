# LunaOS Release Candidate

LunaOS is now frozen at the third release-candidate stage.

The current tree is no longer described as a prototype baseline. Its BIOS main
path, desktop and shell entry, package-visible app catalog, and developer loop
have been tightened into a release-candidate operating envelope.

## RC Freeze Status

Current RC recommendation: `LunaOS RC3 (2026-04-10)`.

This freeze records the currently accepted release-candidate contract:

- stable boot into LunaOS shell on the BIOS validation path
- stable `setup -> login -> home -> settings -> install -> run -> remove`
  user path
- stable developer loop `pack -> install -> run -> remove`
- `update.status` and `update.apply` as reliable update surfaces
- `pairing.status` as a reliable local `loop peer trust` surface
- `network` as a reliable `adapter inspect`, `local-only loop`, external
  inbound minimal closure, and external message-stack minimal main path
- `net.status`, `net.connect`, `net.send`, and `net.recv` as the frozen
  external network stack default path
- default shell/runtime success paths expressed in product-facing output rather
  than `[PACKAGE]` / `[PROGRAM]` execution traces

The following are explicitly outside the RC3 promise:

- protocol expansion beyond the current minimal external message path
- more generalized multi-peer or multi-channel networking semantics
- any broader update orchestration beyond the current minimal `apply` closure

## Ecosystem v1 Freeze Status

Current ecosystem recommendation: `LunaOS Ecosystem v1 Frozen (2026-04-11)`.

This freeze records the currently accepted ecosystem contract:

- `.luna v2` as the distribution and install unit
- `.la ABI/SDK v1` as the runtime executable contract for external apps
- release-grade `signer/source` trust chain enforced across `luna_pack`,
  `PACKAGE`, and `SECURITY`
- audit-chain visibility for `install`, `remove`, and `update.apply`
- shell and update automation baselines aligned to the current ecosystem
  contract rather than legacy package semantics
- 15 spaces plus `LunaLink` as the formal cross-space orchestration model

The following documents freeze the current ecosystem contract:

- `docs/ecosystem-architecture-v1.md`
- `docs/app-package-contract-v1.md`
- `docs/developer-model-v1.md`
- `docs/user-ecosystem-view-v1.md`
- `docs/ecosystem-trust-chain-v1.md`

## Layout

- `boot/`: `lunaloader` BIOS/UEFI entry code, BOOT runtime entry, linker scripts
- `security/`: Rust SECURITY implementation and linker script
- `include/`: protocol and binary record definitions
- `build/`: image layout, build script, self-hosted image emitters, generated artifacts

## Terms

- `space`: an isolated execution realm with its own memory span and pulse stack
- `space gate`: a shared pulse slab used for explicit cross-space exchange
- `CID`: a 128-bit numeric capability seal
- `constellation manifest`: BOOT-readable embedded image map
- `reserve horizon`: preclaimed physical/virtual span for future AI space growth
- `lunaloader`: the self-developed LunaOS loader system that brings execution into space `BOOT`

## Toolchains

The repository expects local Windows toolchains under `toolchains/`:

- `toolchains/winlibs/mingw64/bin`: `gcc`, `ld`, `nm`, `nasm`
- `toolchains/rustup-home/toolchains/stable-x86_64-pc-windows-gnu/bin`: `rustc`

`build/build.py` will also honor explicit overrides such as `LUNA_GCC`, `LUNA_LD`, `LUNA_NASM`, `LUNA_RUSTC`, and `LUNA_NM`.

## Build

```powershell
python .\build\build.py
```

## RC3 Freeze Baseline

The current frozen RC3 baseline is defined by these passing validations:

```powershell
python .\build\build.py
pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1
python .\build\run_qemu_shellcheck.py
python .\build\run_qemu_desktopcheck.py
python .\build\run_qemu_uefi_shellcheck.py
python .\build\run_qemu_uefi_stabilitycheck.py
python .\build\run_qemu_inboundcheck.py
python .\build\run_qemu_externalstackcheck.py
python .\build\run_qemu_updateapplycheck.py
python .\build\run_qemu_fullregression.py
```

These passing results define the current RC3 freeze baseline across BIOS,
desktop, update apply, external inbound, external stack, UEFI shell/stability,
and full regression.

## Developer Loop

The current formal minimal developer example is:

```powershell
python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna
```

The current RC developer loop is:

- pack a `.luna` bundle on the host
- install with `package.install sample`
- verify `sample.luna` is visible in Apps
- run with `run sample`
- remove with `package.remove sample`
- verify the catalog no longer exposes `sample.luna`

This RC success path no longer depends on:

- `sample/sample.luna -> console.luna` remapping
- `PROGRAM` embedded fallback

`PROGRAM` embedded fallback is still retained in code as a damage fallback, but
it is outside the frozen RC3 success baseline.

`PACKAGE` install/index fallback is still retained in code as a damage
fallback, but it is outside the frozen RC3 success baseline.

## Ecosystem Developer Entry

The current frozen ecosystem developer entry is:

```powershell
python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna
python .\tools\luna_pack.py inspect .\build\devloop_sample.luna
python .\tools\luna_orchestrate_audit.py --bundle .\build\devloop_sample.luna --log .\build\qemu_shellcheck.log
```

External developers must target the frozen `.la` ABI declared in
`include/luna_la_abi.h`, package via `.luna v2`, declare required capability
keys in the manifest, and satisfy the current signer/source trust-chain rules
before `PACKAGE` will accept installation.

## Repository Governance Entry

The current repository governance and developer entry documents are:

- `AGENTS.md`
- `docs/platform-contract-index.md`
- `docs/compatibility-matrix.md`
- `docs/developer-entry.md`
- `docs/change-and-regression-rules.md`
