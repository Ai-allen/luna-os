# LunaOS GA Gate

## Scope

This document defines the current general-availability gate derived from the
frozen RC3 baseline.

## Automated GA Gate

The automated GA gate is:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- `python .\build\run_qemu_shellcheck.py`
- `python .\build\run_qemu_desktopcheck.py`
- `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
- `python .\build\run_qemu_uefi_shellcheck.py`
- `python .\build\run_qemu_uefi_stabilitycheck.py`
- `python .\build\run_qemu_inboundcheck.py`
- `python .\build\run_qemu_externalstackcheck.py`
- `python .\build\run_qemu_updateapplycheck.py`
- `python .\build\run_qemu_fullregression.py`

## Manual GA Gate

The manual GA gate is:

- verify fresh install from a clean image
- verify upgrade path through `update.apply`
- verify failure recovery preserves bootability and store health
- verify re-entry after install, upgrade, and recovery
- verify release version stem matches the published directory and artifact names
- verify manifest entries match the published release payload
- verify sha256 entries match every published image artifact
- verify release logs are archived with the published package
- verify GA promise, GA not-ready list, and known limits still match runtime reality

## Capabilities That Stay Out Of GA Promise

The following remain outside the GA promise:

- broader update orchestration
- broader external networking semantics
- protocol expansion beyond the minimal message path
- generalized multi-peer or multi-channel behavior
- hardware or virtualization compatibility claims beyond current validation
- fallback removal as a shipped success-path guarantee

## Decision Rule

GA is acceptable only when the automated GA gate remains green, the manual GA
gate is complete, and the GA promise does not exceed current validated
behavior.
