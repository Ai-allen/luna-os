# LunaOS User Ecosystem View v1

## Status

This document freezes the user-facing ecosystem model for `LunaOS Ecosystem v1`
as of `2026-04-11`.

## User-Facing App Model

Users interact with applications as product-visible entries, not as raw bundle
files or storage objects.

The frozen system app set remains:

- `Settings`
- `Files`
- `Notes`
- `Guard`
- `Console`

Third-party apps become visible only after the package contract succeeds and
the installed app is projected into the package-visible catalog.

## User-Facing Lifecycle

- install:
  - user runs `package.install <name>`
  - success appears as product-facing install output
  - the app becomes visible in `Apps` and `list-apps`
- run:
  - user launches by name from shell or desktop
  - runtime launch is resolved from the installed catalog
- update:
  - update state is visible through `update.status` and settings
  - current ecosystem baseline does not promise richer app-store style update UX
- remove:
  - app disappears from `Apps` and `list-apps`
  - launch is blocked after removal

## Data and Retention View

Users should understand the ecosystem as three different surfaces:

- app presence: whether the app is installed and launchable
- app runtime: whether a session is active
- app/user data: whether user-owned data is retained

Removing a package revokes package visibility and launch resolution. It does
not automatically imply full user-data destruction unless the owning contract
explicitly says so.

## Settings / Files / Notes / Future Apps

- `Settings` is the structured system state surface
- `Files` is the user-facing data projection surface
- `Notes` is a user-workspace app surface
- future apps must enter through the same install, capability, trust, and
  audit contracts rather than side-loading new semantics

## Frozen User Contract Boundary

Included:

- clear install/run/remove behavior
- package-visible app catalog
- product-facing status output
- stable shell and desktop app-launch entry

Excluded from v1:

- broader store UX
- richer graphical orchestration surfaces
- generalized multi-source consumer app management
