# LunaOS RC3 Frozen Operations Guide

## Scope

This guide freezes the operator path for first install, update apply, failure
recovery, and re-entry into the system.

## First Install

1. Build the current image:
   - `python .\build\build.py`
2. Boot the image through the frozen QEMU path.
3. Wait for shell readiness:
   - `[USER] shell ready`
4. If the system is uninitialized, run:
   - `setup.init <hostname> <username> <password>`
5. Confirm session entry:
   - `login ok: session active`
   - `whoami`
   - `hostname`
6. Confirm the default product apps are visible:
   - `list-apps`

## Update Apply

1. Enter the shell on a valid session.
2. Inspect update state:
   - `update.status`
3. Confirm a non-current target exists.
4. Execute:
   - `update.apply`
5. Confirm apply result reports a real staged or committed apply outcome.
6. Reboot through the normal system path.
7. Re-enter the shell.
8. Confirm post-reboot convergence:
   - `update.status`
   - `current` and `target` must match

## Failure Recovery

If `update.apply` fails before commit:

1. Keep the current bootable system.
2. Reboot the normal image.
3. Re-enter the shell.
4. Confirm the current system still boots and the session is usable:
   - `[USER] shell ready`
   - `login ok: session active`
5. Inspect the update state:
   - `update.status`
6. Do not treat the failed target as active unless post-reboot state confirms it.

If a validation step fails before release:

1. Stop release packaging.
2. Re-run the frozen baseline from `docs/test-baseline.md`.
3. Only accept fixes that remove the regression without expanding RC3 scope.

## Re-Entry Path

The frozen re-entry path is:

1. boot the current image
2. wait for `[USER] shell ready`
3. log in through the current session path
4. use:
   - `settings.status`
   - `update.status`
   - `pairing.status`
   - `net.status`
5. enter desktop only after shell-level health is confirmed

## Operator Rule

The release package is acceptable only when the current system still boots,
still enters shell, and still reports consistent state after install, update,
or recovery.
