# LunaOS Developer Model v1

## Status

This document freezes the minimum external developer contract for
`LunaOS Ecosystem v1` as of `2026-04-11`.

## Minimum Entry

External developers target three stable host/runtime entry points:

- `tools/luna_pack.py` for `.luna v2` packaging
- `include/luna_la_abi.h` for `.la ABI/SDK v1`
- `tools/luna_orchestrate_audit.py` for bundle and audit inspection

## Packaging Contract

Minimum host path:

```powershell
python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna
python .\tools\luna_pack.py inspect .\build\devloop_sample.luna
```

The manifest must declare:

- bundle name
- source identity
- signer identity when source is non-zero
- app version
- entry offset
- ABI and SDK version
- proto compatibility bounds
- requested capability keys

## Runtime ABI Contract

`.la ABI/SDK v1` is declared in `include/luna_la_abi.h`.

Frozen external ABI fields:

- `LUNA_PROGRAM_APP_MAGIC`
- `LUNA_PROGRAM_APP_VERSION`
- `LUNA_PROGRAM_BUNDLE_MAGIC`
- `LUNA_PROGRAM_BUNDLE_VERSION`
- `LUNA_LA_ABI_MAJOR`
- `LUNA_LA_ABI_MINOR`
- `LUNA_SDK_MAJOR`
- `LUNA_SDK_MINOR`
- `struct luna_la_header`
- `struct luna_luna_header_v2`

`PROGRAM` must reject payloads whose ABI or proto range does not match the
runtime contract.

## Capability Contract

- capability requests are declared in the bundle manifest
- runtime start is not equivalent to implicit full trust
- `SECURITY` remains the approval gate for load/start and subsequent writes
- developers must design for explicit denial when capability, signer, source,
  policy, or mode is not accepted

## Trust-Chain Admission

A bundle is not accepted only because it is structurally valid.

Minimum acceptance conditions are:

- signer/source fields satisfy the bundle contract
- signer is allowed by the current trusted signer root
- source is allowed by the current trusted source root
- source policy binding matches the signer
- `SECURITY` policy admits the install action

## Audit and Debug Entry

The minimum orchestration inspection path is:

```powershell
python .\tools\luna_orchestrate_audit.py --bundle .\build\devloop_sample.luna --log .\build\qemu_shellcheck.log
```

The tool is the current frozen developer-facing entry for:

- bundle header inspection
- install/remove/update audit-line grouping
- approval, denial, and persistence trace summaries
