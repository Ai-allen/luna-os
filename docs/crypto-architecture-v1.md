# LunaOS Crypto Architecture v1

## Status

This document defines LunaOS Crypto Architecture v1 as of `2026-04-17`.

`SECURITY` owns system crypto authority. Other spaces may request crypto
services, but they do not own parallel key or release policy.

## Sovereignty

`SECURITY` owns:

- encrypt / decrypt authority
- seal / unseal authority
- key derivation
- integrity hash / MAC derivation
- signature verification
- trust verification

`DATA` may persist crypto-protected material, but it does not choose keys,
algorithms, or release policy.

## Key Classes

The phase-one key classes are:

- user auth key
- trust verification key
- install-bound `LDAT` protection key

All phase-one keys are derived under `SECURITY` using install identity and
subject identity as part of release policy.

## Phase-One Entry Points

The first formal Crypto v1 entry points are:

- `LUNA_GATE_CRYPTO_SECRET`
  for user credential derivation and install-bound secret protection
- `LUNA_GATE_TRUST_EVAL`
  for signer/source trust and bundle integrity/signature verification
- `LUNA_GATE_AUTH_LOGIN`
  for governed credential verification using security-owned derivation

## Phase-One System Behavior

The first landed crypto behaviors are:

- user setup stores install-bound security-derived credential material instead
  of caller-owned password folding
- login verifies credentials through `SECURITY` using the same install-bound
  derivation path
- package install trust and integrity judgment runs inside `SECURITY`
- `LDAT` user secret objects are marked as security-owned and install-bound

## Audit

Phase-one crypto events are emitted through security audit lines, including:

- crypto secret release
- auth allow / deny
- auth session issue / revoke
- trust allow / deny

These events are consumable by the existing observability path.
