# LunaOS Post-RC Roadmap

## Scope

This roadmap records work that remains intentionally outside the frozen RC3
promise.

## Priority 1: Update Orchestration

Why it is not in RC3:

- RC3 only commits to the current minimal `update.apply` closure
- broader version choreography, rollback management, and release orchestration
  would widen the frozen update surface

Minimum completion condition for the next phase:

- multiple update targets are represented explicitly
- apply, confirm, and reject states are fully convergent
- failed update attempts produce a clear rollback path
- post-reboot state is traceable without relying on ad hoc diagnostics

## Priority 2: External Networking Expansion

Why it is not in RC3:

- RC3 only commits to external inbound minimal closure and the minimal external
  message-stack main path
- broader transport semantics would widen the frozen protocol and test surface

Minimum completion condition for the next phase:

- more than one stable peer or channel path is supported intentionally
- session recovery and timeout behavior are explicit rather than incidental
- longer-running external traffic behavior is verified beyond the current minimal checks
- error surfaces remain product-readable under sustained external use

## Priority 3: Fallback Compression

Why it is not in RC3:

- RC3 allows `PROGRAM embedded fallback` and `PACKAGE install/index fallback`
  to remain in code as damage fallbacks
- removing them without a larger sweep would risk the frozen main path

Minimum completion condition for the next phase:

- success paths no longer require either fallback in any validated scenario
- failure paths return explicit errors instead of dropping into retained fallback behavior
- frozen regression gates remain green after fallback removal or deeper isolation

## Priority Order

1. update orchestration
2. external networking expansion
3. fallback compression
