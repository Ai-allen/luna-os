# LunaOS Security System v1

## Status

This document defines the formal LunaOS system security model as of
`2026-04-17`.

`SECURITY` is not only an approval entry. It is the sovereign system security
space for LunaOS. Other spaces may request security services through governed
gate calls, but they do not own independent parallel security stacks.

## Security Sovereignty

`SECURITY` formally owns these domains:

- identity
- authentication
- capability and authority issuance
- trust and integrity judgment
- secret and key management
- policy and response classification
- security audit and event emission

`DATA`, `USER`, `PACKAGE`, `PROGRAM`, `DEVICE`, and `UPDATE` may surface
security-relevant facts, but final allow/deny/readonly/recovery decisions
belong to `SECURITY`.

## Identity Model

The v1 identity set is:

- user identity: human account principal projected into `USER`
- session identity: issued login or setup session with explicit revoke point
- device identity: hardware-bound or firmware-visible machine identity
- install identity: install UUID and install-bound system instance identity
- source identity: package/update source authority
- signer identity: signer authority for `.luna` and `.la`
- driver identity: governed driver principal for device bind/load
- app/runtime identity: executing program or app runtime principal

Identity rules:

- no identity may be forged by direct caller-supplied fields alone
- install identity and device identity are security-owned anchors
- signer/source identity is resolved through trust roots, not package claims
- runtime identity must remain attributable to its loaded bundle and issuer

## Authentication Model

`SECURITY` owns:

- user authentication
- session issuance
- session revoke
- install-bound session validation
- device/install binding checks for privileged operations

Phase-one authentication priority:

- first-setup credential initialization
- login session issuance and revoke
- validation for install/update/storage governance decisions

## Capability And Authority

Capabilities remain the governed runtime projection mechanism. `SECURITY`
issues, scopes, and revokes authority for:

- runtime operations
- storage write paths
- driver load and device operations
- crypto operations
- query operations
- update/apply flows

Authority response classes in v1 are:

- `allow`
- `deny`
- `readonly`
- `recovery-required`
- `quarantine`
- `fatal`
- `rollback`
- `reject`

No non-`SECURITY` space may define a conflicting final authority model.

## Trust And Integrity

The v1 trust and integrity chain covers:

- `.luna` package trust chain
- `.la` runtime trust chain
- source and signer trust roots
- native storage integrity gates for `LSYS` and `LDAT`
- driver load trust gate
- update/apply integrity gate
- runtime integrity checks for launched payloads

Trust rules:

- signer/source trust is evaluated before install or activation
- integrity failure does not silently downgrade into normal execution
- storage/install/update trust decisions feed lifecycle state and response

## Secret And Key Management

`SECURITY` owns key classes and release policy for:

- device root key
- install-bound key
- user-derived key
- signer/source trust keys
- recovery/update temporary keys
- future session/network keys

Release rules:

- keys are not ordinary cross-space payload objects
- `DATA` may persist encrypted or sealed material, but does not choose keys
- key release is conditioned on identity, authority, state, and policy
- security-relevant key usage must remain auditable

## Security Audit And Event

The minimum v1 security event set includes:

- auth success and failure
- session issuance and revoke
- capability issue and deny
- trust accept and trust reject
- driver bind/load allow and deny
- storage/install/update security judgments
- secret release and secret release denial
- recovery/quarantine/fatal transitions caused by security judgment

`SECURITY` emits or records these events for `OBSERVABILITY` consumption. The
audit chain must support incident reconstruction across install, boot, login,
package, driver, and update paths.

Security and crypto events are emitted in the LunaOS structured record model
(`LSON`). `SECURITY` owns the meaning of those audit classes and the visibility
policy for their fields, while `OBSERVABILITY` owns log consumption and `DATA`
owns persistent storage and indexing.

## Integration Boundaries

- `DATA` stores governed state and sealed material, but does not own policy.
- `PACKAGE` and `PROGRAM` request signer/source/runtime judgment from
  `SECURITY`.
- `DEVICE` requests driver trust and authority judgment from `SECURITY`.
- `UPDATE` requests apply and activation judgment from `SECURITY`.
- `OBSERVABILITY` consumes security events; it does not replace security policy.

## Phase-One Landing

Security System v1 phase one is defined as:

- user auth and session security
- signer/source trust plus bundle/runtime integrity
- native storage/install/update security judgment chain

These paths are the priority landing surface before broader expansion.

## Phase-One Formal Entrances

The phase-one system entry points are:

- `LUNA_GATE_AUTH_LOGIN`
- `LUNA_GATE_AUTH_SESSION_OPEN`
- `LUNA_GATE_AUTH_SESSION_CLOSE`
- `LUNA_GATE_CRYPTO_SECRET`
- `LUNA_GATE_TRUST_EVAL`
- `LUNA_GATE_GOVERN`

The first-phase active callers are:

- `USER` for setup/login/logout credential and session flow
- `PACKAGE` for signer/source trust and bundle integrity judgment
- `DATA` / `UPDATE` for storage/install/update governance judgment
