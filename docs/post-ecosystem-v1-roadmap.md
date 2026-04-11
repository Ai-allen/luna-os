# LunaOS Post-Ecosystem-v1 Roadmap

## Status

This document records the immediate follow-up priorities after
`LunaOS Ecosystem v1 Frozen (2026-04-11)`.

It is a planning document only. It does not expand the frozen v1 contract.

## Priority 1: Richer Signing and Trust Infrastructure

Why it is not in v1:

- v1 freezes a release-grade signer/source trust root and enforcement path, but
  not a full signing platform
- delegated trust, revocation distribution, signer lifecycle tooling, and
  external PKI-style operations would expand both policy and operational surface

Minimum completion condition:

- formal root rotation procedure
- signer revocation records and rejection path
- source onboarding/offboarding tooling
- auditable release-sign operation beyond the current deterministic seal

## Priority 2: Mature Ecosystem Orchestration Tooling

Why it is not in v1:

- v1 includes the minimum audit and orchestration inspection loop, but not a
  comprehensive operational toolchain
- current tooling is sufficient for contract verification, not full-scale
  maintainer or developer operations

Minimum completion condition:

- unified install/update/remove/recovery event view
- consistent cross-space error classification
- bundle, audit, and policy inspection in one operator-facing tool flow
- stable maintainer diagnostics for approval, denial, and persistence outcomes

## Priority 3: Broader Third-Party Distribution and Compatibility

Why it is not in v1:

- v1 freezes the bundle, runtime ABI, and trust admission contract, but not a
  mature third-party ecosystem program
- broader compatibility promises would require stronger source governance, SDK
  maturity, migration guarantees, and operational tooling

Minimum completion condition:

- frozen compatibility policy for `.la` ABI evolution
- versioned SDK release process
- third-party distribution onboarding rules
- compatibility and rejection guidance for out-of-range bundles
