# LunaOS App / Package Contract v1

## Status

This document freezes the app and package contract for `LunaOS Ecosystem v1`
as of `2026-04-11`.

## Dual Unit Model

- `.luna` is the distribution and installation unit.
- `.la` is the runtime executable payload embedded inside the `.luna` bundle.
- `PACKAGE` accepts, indexes, installs, removes, and updates `.luna`.
- `PROGRAM` loads and starts `.la`.

## Bundle Contract

The current formal bundle contract is `.luna v2`.

Required bundle identity and compatibility fields:

- `name`
- `bundle_id`
- `source_id`
- `signer_id`
- `version`
- `flags`
- `abi_major`
- `abi_minor`
- `sdk_major`
- `sdk_minor`
- `min_proto`
- `max_proto`
- `entry_offset`
- `capabilities`

The formal host entry point is:

```powershell
python .\tools\luna_pack.py <manifest.json> <payload.bin> <output.luna>
```

## Install / Run / Remove Contract

- install:
  - bundle arrives as `.luna`
  - `PACKAGE` validates structure, signer/source admission, and policy
  - `SECURITY` approves or denies
  - `DATA` persists package-visible records
  - install success is visible through product-facing output and audit lines
- run:
  - `USER` or desktop launch requests a package-visible app
  - `PROGRAM` resolves the app object
  - `SECURITY` gates runtime load/start
  - `PROGRAM` admits only ABI/proto-compatible payloads
- remove:
  - `PACKAGE` revokes installed/index visibility
  - `DATA` persists the removal result
  - runtime launch resolution is removed from the package-visible catalog

## Authoritative Ownership

- package catalog, install records, and source/signer admission state belong to
  `PACKAGE`
- runtime session and executable admission belong to `PROGRAM`
- persistent user and app data belong to `USER` / `DATA`
- write admission and policy approval belong to `SECURITY`

## Frozen Success Path

The current formal ecosystem developer loop is:

1. Pack `.luna` on the host.
2. `package.install <name>`
3. verify the app is visible via `list-apps`
4. `run <name>`
5. `package.remove <name>`
6. verify the app is no longer visible and launch is blocked

The current success path does not depend on:

- legacy `sample -> console.luna` remapping
- hidden success through `PROGRAM` embedded fallback
- hidden success through `PACKAGE` install/index fallback
