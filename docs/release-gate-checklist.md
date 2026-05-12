# LunaOS RC3 Frozen Release Gate Checklist

## Frozen Validation Gate

Every release candidate package must pass all frozen automated checks:

- [ ] `python .\build\build.py`
- [ ] `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- [ ] `python .\build\run_qemu_shellcheck.py`
- [ ] `python .\build\run_qemu_desktopcheck.py`
- [ ] `pwsh -NoProfile -File .\build\run_vmware_desktopcheck.ps1`
- [ ] `python .\build\run_qemu_uefi_shellcheck.py`
- [ ] `python .\build\run_qemu_uefi_stabilitycheck.py`
- [ ] `python .\build\run_qemu_inboundcheck.py`
- [ ] `python .\build\run_qemu_externalstackcheck.py`
- [ ] `python .\build\run_qemu_updateapplycheck.py`
- [ ] `python .\build\run_qemu_updaterollbackcheck.py`
- [ ] `python .\build\run_qemu_installer_failurecheck.py`
- [ ] `python .\build\run_qemu_fullregression.py`

## Manual Release Gate

The release owner must also confirm:

- [ ] release notes match current RC3 promise
- [ ] not-ready list matches current RC3 exclusions
- [ ] known limits list still matches runtime reality
- [ ] version stem matches the frozen tag date
- [ ] artifact names follow the release packaging rules
- [ ] published checksum file uses `sha256`
- [ ] saved logs exist for all frozen validation gates
- [ ] no new feature or protocol work has been merged after freeze
- [ ] `PROGRAM embedded fallback` is still treated as outside the success baseline
- [ ] `PACKAGE install/index fallback` is still treated as outside the success baseline

## Release Decision

The package is releasable only if every automated gate and every manual gate is
checked complete without widening the RC3 scope.
