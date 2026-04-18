# LunaOS Change And Regression Rules

## Purpose

This document defines how frozen-contract changes are classified, what gates
must be rerun, and which changes are allowed on `main`.

## Change Classes

### Regression Fix

A regression fix restores previously frozen behavior without widening contract
scope.

Allowed on `main`:

- yes

Required work:

- code fix
- matching spec correction if docs drifted
- rerun gates for the touched frozen surface

### Compatibility Update

A compatibility update preserves contract meaning but tightens acceptance,
clarifies enforcement, or adds support within an already frozen contract.

Allowed on `main`:

- only when it does not widen the public contract and is backed by current
  runtime evidence

Required work:

- update affected contract docs
- update affected baseline docs
- rerun affected gates

### Breaking Change

A breaking change alters version boundaries, ABI expectations, accepted bundle
formats, storage compatibility rules, or user-visible frozen behavior.

Allowed on `main`:

- no

Required path:

- move to an explicit post-v1 or post-GA branch/stage
- define new contract docs and new gates before merge

## Gate Rerun Rules

- shell-visible behavior:
  - `python .\build\build.py`
  - `python .\build\run_qemu_shellcheck.py`
- desktop-visible behavior:
  - `python .\build\build.py`
  - `python .\build\run_qemu_desktopcheck.py`
- boot, lifecycle, storage, setup:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
  - `python .\build\run_qemu_shellcheck.py`
- update/apply/activation:
  - `python .\build\build.py`
  - `python .\build\run_qemu_updateapplycheck.py`
  - `python .\build\run_qemu_fullregression.py`
- ecosystem trust, `.luna`, `.la`, audit tooling:
  - `python .\build\build.py`
  - `python .\build\run_qemu_shellcheck.py`
  - `python .\build\run_qemu_updateapplycheck.py`
  - relevant host tooling invocation
- driver governance:
  - `python .\build\build.py`
  - `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
  - `python .\build\run_qemu_shellcheck.py`

## Mainline Allow / Deny Rules

Allowed directly on `main`:

- regression fixes against frozen contracts
- factual doc corrections
- gate fixes that restore alignment to current real behavior
- release prep and repository-governance documentation that does not widen
  platform contract

Not allowed directly on `main`:

- new product surfaces
- protocol expansion
- new storage architecture
- new ecosystem contract layers
- ABI-breaking `.la` changes
- incompatible `.luna` format changes
- driver privilege expansion beyond frozen governance rules

## Regression Definition

Treat any of the following as a regression until proven otherwise:

- a frozen gate turns red
- current runtime output no longer matches frozen docs
- a previously accepted install/run/update/remove path no longer works
- trust-chain acceptance or rejection changes without matching spec and gate
  updates
- volume-state, mount, or writable behavior drifts from the frozen storage
  contract

## Required Review Question

Before merging, answer:

- is this a regression fix, a compatibility-preserving enforcement fix, or a
  breaking change?

If the answer is not clearly the first two, it should not land on `main`.
