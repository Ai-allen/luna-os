# LunaOS Compatibility Matrix

## Purpose

This document freezes the compatibility matrix for the current LunaOS frozen
contracts on `main`.

## Frozen Version Set

- GA system line: `GA 2026-04-10`
- RC base contract: `RC3 Frozen 2026-04-10`
- Native Storage / Install: `v1`
- Ecosystem: `v1`
- `.luna` distribution format: `v2`
- `.la ABI/SDK`: `v1`
- Driver Architecture: `v1`

## Compatibility Matrix

| Surface | Current version | Accepted with current main | Reject / special handling |
| --- | --- | --- | --- |
| `.luna` bundle format | `v2` | accepted when trust-chain and manifest rules pass | older legacy package assumptions are not accepted as passing ecosystem behavior |
| `.la ABI` | `v1` | accepted when manifest ABI bounds and PROGRAM load contract match | ABI mismatch must be rejected before runtime load |
| Native storage volume | `v1` | accepted when LSYS/LDAT identity, profile, and state rules pass | incompatible volume profile or incompatible flags must mount readonly, recovery, or refuse mount per state |
| Ecosystem contract | `v1` | accepted with `.luna v2`, `.la ABI v1`, trust-chain, and audit-chain | legacy no-trust or pre-audit semantics are not an acceptance path |
| Driver architecture | `v1` | accepted with governance approval, bind record, and failure classification | ungated direct bind is not compatible with current frozen contract |

## Compatibility Rules

### `.luna`

- `PACKAGE` only accepts the current `.luna v2` contract as the frozen
  ecosystem acceptance path.
- manifest signer/source identity must satisfy the current trust-root rules
  before persistence.
- bundle ABI and SDK bounds must be compatible with the current `.la ABI/SDK`
  contract.

### `.la`

- `.la ABI v1` is the current external runtime ABI.
- PROGRAM must reject a runtime payload whose ABI range does not include the
  current accepted ABI.
- driver `.la` and app `.la` are not interchangeable contracts even if both
  sit on the same broad ABI family.

### Native Storage / Install

- system boot requires a valid `LSYS` and `LDAT` pair, matching install
  identity, and a volume-state that is allowed by `SECURITY`.
- future incompatible storage revisions must use the current compat model:
  - compatible: mount normally
  - readonly compatible: mount readonly
  - incompatible: refuse normal mount and require recovery or rejection

### Cross-Version Policy

- compatible upgrades may proceed in place when gates and contract migration
  rules succeed.
- incompatible upgrades must not silently mount writable.
- unsupported combinations are reject-path combinations, not degraded success
  combinations.

## Upgrade / Reject Policy

- `.luna`:
  - reject when signer/source policy fails
  - reject when ABI bounds fail
  - reject when version is outside accepted distribution range
- `.la`:
  - reject load when ABI mismatches current PROGRAM contract
- Native Storage / Install:
  - reject writable mount when volume-state or incompat flags forbid it
  - enter recovery or readonly only when the storage contract explicitly allows
    it
- Drivers:
  - reject bind when driver identity, trust, capability class, or governance
    rules do not match the current driver contract

## Mainline Rule

Any change to these version boundaries must update:

- [`docs/platform-contract-index.md`](/abs/path/C:/Users/AI/lunaos/docs/platform-contract-index.md)
- [`docs/test-baseline.md`](/abs/path/C:/Users/AI/lunaos/docs/test-baseline.md)
- the affected contract specification documents
- the affected automated gates
