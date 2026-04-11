# LunaOS GA Release Page

## Release Identity

- product: `LunaOS`
- release class: `general availability`
- source baseline: `RC3 Frozen (2026-04-10)`

## Release Promise

This release promises only behavior already established by current repository
validation:

- stable boot into LunaOS shell on the validated BIOS path
- stable setup and first-session path
- stable product app launch path for `Settings`, `Files`, `Notes`, `Guard`,
  and `Console`
- stable shell and developer software loop through `pack -> install -> run ->
  remove`
- stable `update.status` and minimal `update.apply`
- stable local trust status through `pairing.status`
- stable network inspect, local-only loop, external outbound-only, external
  inbound minimal closure, and minimal external message-stack main path

## Not Included

This release does not promise:

- broader update orchestration
- broader external protocol support
- generalized multi-peer or multi-channel transport behavior
- compatibility claims beyond the validated Windows and QEMU workflow
- removal of retained fallback branches from the codebase

## Supported Environment

- Windows host build workflow
- repository-local toolchains
- validated QEMU BIOS path
- validated QEMU UEFI path

## Operational References

- release operations: `docs/release-operations.md`
- packaging rules: `docs/release-packaging-rules.md`
- GA gate: `docs/ga-gate.md`

## Known Limits

- the current update surface is minimal rather than fully orchestrated
- the current external network stack is minimal rather than generalized
- retained fallback branches remain present but outside the success baseline
- early boot logs remain engineering-facing
