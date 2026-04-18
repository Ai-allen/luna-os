# LunaOS Developer Entry

## Purpose

This is the minimum external developer entry for the frozen LunaOS ecosystem on
`main`.

## What You Build

- write a LunaOS executable payload as `.la`
- describe it through a Luna manifest
- package it into a `.luna v2` bundle
- install, run, update, and remove it through LunaOS ecosystem paths

## Minimum Authoring Path

### 1. Target the current `.la ABI`

- target the frozen runtime ABI in
  [`include/luna_la_abi.h`](/abs/path/C:/Users/AI/lunaos/include/luna_la_abi.h)
- treat `.la ABI/SDK v1` as the only external ABI contract accepted on `main`
- do not assume Linux, POSIX, Windows, or macOS file/process semantics

### 2. Prepare a Luna manifest

Use one of:

- [`tools/luna_manifest.minimal.json`](/abs/path/C:/Users/AI/lunaos/tools/luna_manifest.minimal.json)
- [`tools/luna_manifest.example.json`](/abs/path/C:/Users/AI/lunaos/tools/luna_manifest.example.json)

The manifest defines:

- app identity
- bundle metadata
- ABI and SDK bounds
- capability declarations
- source and signer identity

### 3. Pack the bundle

```powershell
python .\tools\luna_pack.py .\tools\luna_manifest.minimal.json .\build\obj\console_app.bin .\build\devloop_sample.luna
```

Optional inspection:

```powershell
python .\tools\luna_pack.py inspect .\build\devloop_sample.luna
```

### 4. Install and run

Inside LunaOS:

- `package.install sample`
- `run sample`
- `package.remove sample`

### 5. Audit the flow

```powershell
python .\tools\luna_orchestrate_audit.py --bundle .\build\devloop_sample.luna --log .\build\qemu_shellcheck.log
```

## Capability Contract

- capabilities are declared in the `.luna` manifest
- SECURITY approves or denies the resulting install/run path
- PACKAGE persists installation state
- PROGRAM loads the `.la` runtime payload
- DATA performs the actual persisted write path when mutation occurs

Do not treat capability declaration as self-granting. Declared capability is a
request, not an automatic entitlement.

## Trust-Chain Acceptance

Bundle acceptance requires:

- structurally valid `.luna v2`
- coherent signer/source identity
- trusted signer root membership
- trusted source root membership
- SECURITY policy approval
- source-to-signer binding match when policy requires it

If any of these fail, installation is rejected before persistence.

## Minimum Install / Run / Update / Remove Path

- install:
  - host packs `.luna`
  - PACKAGE validates bundle and trust
  - SECURITY approves
  - DATA persists install records
- run:
  - USER requests app launch
  - PROGRAM resolves installed app identity and loads `.la`
- update:
  - UPDATE coordinates package-visible target changes under current ecosystem
    and storage contracts
- remove:
  - PACKAGE revokes installed/index ownership
  - PROGRAM can no longer resolve launch for the removed app

## What External Developers Cannot Do Today

- define new protocol families as part of the frozen ecosystem contract
- bypass SECURITY or trust-chain admission
- write directly to DEVICE, DATA, or boot-critical state without approved
  capability and owner flow
- rely on legacy package semantics outside `.luna v2`
- rely on driver-only runtime privileges from a normal app `.la`
- assume undocumented ABI stability beyond `.la ABI/SDK v1`

## Related Frozen Specs

- [`docs/ecosystem-architecture-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/ecosystem-architecture-v1.md)
- [`docs/app-package-contract-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/app-package-contract-v1.md)
- [`docs/developer-model-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/developer-model-v1.md)
- [`docs/ecosystem-trust-chain-v1.md`](/abs/path/C:/Users/AI/lunaos/docs/ecosystem-trust-chain-v1.md)
