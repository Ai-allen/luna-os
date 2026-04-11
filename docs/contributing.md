# LunaOS Contributing Rules

## Scope
This document defines the minimum consistency rules for LunaOS repository changes.
It applies to code, docs, developer tools, and test baselines.

## Required Rule: Behavior Changes Must Ship With Docs And Tests
- Any user-visible behavior change must update the matching documentation in the same change.
- Any user-visible behavior change must update the matching automated baseline in the same change.
- A code change is incomplete if it changes runtime output, product names, shell commands, desktop actions, or developer tooling behavior without updating docs and tests.

## Required Documentation Targets
- Update [minimum-delivery-spec.md](/abs/path/C:/Users/AI/lunaos/docs/minimum-delivery-spec.md) when minimum supported behavior changes.
- Update [system-capabilities.md](/abs/path/C:/Users/AI/lunaos/docs/system-capabilities.md) when a capability is added, removed, or promoted from fallback to product path.
- Update [test-baseline.md](/abs/path/C:/Users/AI/lunaos/docs/test-baseline.md) when `shellcheck` or `desktopcheck` expectations change.

## Required Test Targets
- Update [run_qemu_shellcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_shellcheck.py) when shell-visible behavior changes.
- Update [run_qemu_desktopcheck.py](/abs/path/C:/Users/AI/lunaos/build/run_qemu_desktopcheck.py) when desktop-visible behavior changes.
- Do not leave historical names, old counters, or obsolete failure expectations in the required output list after behavior changes.

## Product Naming Rules
- User-visible product names must use current product labels such as `Settings`, `Files`, `Notes`, `Guard`, and `Console`.
- Internal identities such as `hello` may remain in storage or package internals, but they must not be treated as the product surface in docs or tests.

## Developer Entry Rules
- Changes to `.luna` bundle fields or manifest validation must update the developer-facing capability description in [system-capabilities.md](/abs/path/C:/Users/AI/lunaos/docs/system-capabilities.md).
- If `tools/luna_pack.py` changes accepted manifest fields or validation rules, the example manifest must stay valid.

## Verification Rule
- Changes touching user-visible behavior must be verified with:
  - `python build\\build.py`
  - `python build\\run_qemu_shellcheck.py`
  - `python build\\run_qemu_desktopcheck.py`
- If one of these is intentionally not run, the omission must be stated explicitly.

## Non-Rules
- This document does not require architecture expansion.
- This document does not authorize changing baseline requirements without matching runtime evidence.
