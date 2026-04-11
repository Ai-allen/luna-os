# LunaOS Ecosystem Trust Chain v1

## Status

This document freezes the current release-grade signer/source trust chain for
`LunaOS Ecosystem v1` as of `2026-04-11`.

## Trust Root Model

The current trust root is contract-driven rather than marketplace-driven.

- trusted signers are recorded in the `PACKAGE`-owned trusted signer set
- trusted sources are recorded in the `PACKAGE`-owned trusted source set
- source admission can be bound to a specific signer
- `SECURITY` remains the policy authority for signer and source admission

## Acceptance Rules

Bundle acceptance requires all of the following:

- `.luna` structure is valid
- signer/source identity fields are valid for the bundle version
- if `source_id` is non-zero, `signer_id` must be non-zero
- signer is present in the trusted signer root
- signer is allowed by `SECURITY` signer policy
- source is present in the trusted source root
- source is allowed by `SECURITY` source policy
- when source policy carries a signer binding, the bundle signer must match

Any failure must be rejected before install persistence.

## Deterministic Seal Boundary

The current system still uses deterministic seal material as the bundle
integrity and signature primitive.

That is distinct from the trust-root contract:

- deterministic seal answers whether the bundle payload and header material are
  internally consistent
- signer/source trust chain answers whether the issuer is allowed to enter the
  ecosystem

## Audit Visibility

The current frozen trust-chain behavior is expected to surface through audit
lines during install and update paths, including:

- approval by `SECURITY`
- persistence by `DATA`
- denial reasons when source, signer, or binding admission fails

## Frozen v1 Boundary

Included:

- trusted signer root
- trusted source root
- source-to-signer binding
- install-time enforcement
- host-side manifest constraint for signer/source identity coherence

Out of scope for v1:

- delegated multi-level PKI
- external CA integration
- signer revocation UX beyond current policy enforcement
- richer online trust distribution infrastructure
