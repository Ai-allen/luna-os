# LunaOS Native Install Spec v1

## Status

Frozen commercial installation specification for LunaOS Native Storage / Install v1 as of `2026-04-10`.

## Install Layout

The formal install layout is:

- `ESP`
- `LSYS`
- `LDAT`
- optional `LRCV`

`ESP` is the only compatibility partition.

`LSYS` and `LDAT` are LunaOS native partitions and must not be replaced by ext4, NTFS, APFS, or other foreign data formats.

## Installer Contract

Installer must execute the following sequence:

1. select target disk
2. clear old LunaOS layout or explicitly preserve only supported retained data
3. write GPT
4. create `ESP + LSYS + LDAT`
5. initialize native super/checkpoint metadata for both `LSYS` and `LDAT`
6. stamp both native volumes with the same `install_uuid`
7. write peer LBA contract
8. write activation contract
9. seed minimal `LSYS` system object graph
10. seed minimal `LDAT` mutable object graph
11. write `BOOTX64.EFI` and boot manifest to `ESP`
12. verify pair identity before declaring success

## First-Boot Contract

A fresh installed image is only accepted when first boot proves:

- boot reaches LunaLoader and LunaOS stage
- `LSYS/LDAT` pair passes identity contract
- system reaches `first-setup required`
- `setup.init` succeeds on fresh image
- reboot preserves host, user, and home state

## First-Setup Touch Set

Before first setup, disk must already contain:

- valid `LSYS` super/checkpoint and minimal system graph
- valid `LDAT` super/checkpoint and mutable roots
- `setup.state = required`

`setup.init` is allowed to create or update:

- host record
- user record
- user secret
- user home root
- user profile
- initial home sets such as desktop, documents, downloads
- `setup.state = ready`

## Acceptance Gate

Native Install v1 acceptance requires all of:

- `python .\build\build.py`
- `pwsh -NoProfile -File .\build\run_qemu_bootcheck.ps1`
- fresh image boot reaches `first-setup required`
- fresh image `setup.init` succeeds
- second boot `login` succeeds
- second boot retained home state is visible

## Failure Rules

Installer must not claim success when:

- `LSYS/LDAT` pair identity is broken
- native super/checkpoint initialization fails
- activation contract is invalid
- first boot cannot reach `first-setup required`

Normal boot must be refused when identity or fatal state contract is broken.

Readonly or recovery mode is allowed only when governance explicitly places the volume there.
