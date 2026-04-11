# LunaOS RC3 Frozen Versioning And Packaging Rules

## Version Format

- marketing name: `LunaOS RC3 Frozen`
- canonical version tag: `rc3-frozen-2026-04-10`
- display version format: `LunaOS RC3 Frozen (YYYY-MM-DD)`

The date in the display version and canonical tag must match the freeze date of
the released package.

## Artifact Naming

Released artifacts should use the same version stem.

- image: `lunaos-rc3-frozen-2026-04-10.img`
- manifest: `lunaos-rc3-frozen-2026-04-10-manifest.txt`
- checksums: `lunaos-rc3-frozen-2026-04-10-sha256.txt`
- release notes: `lunaos-rc3-frozen-2026-04-10-release-notes.md`

If BIOS and UEFI packaging are split later, the suffix should be appended after
the version stem rather than replacing it.

## Release Directory Layout

The release directory should be structured as:

```text
release/
  rc3-frozen-2026-04-10/
    image/
    notes/
    checksums/
    logs/
```

Required contents:

- `image/`:
  - frozen bootable image artifacts
- `notes/`:
  - release notes
  - operations guide
  - release gate checklist
- `checksums/`:
  - sha256 checksum file
- `logs/`:
  - saved outputs from the frozen baseline gates

## Checksum Rule

- checksum algorithm: `sha256`
- every published image artifact must have a matching sha256 entry
- checksum file name must include the exact release stem
- checksum entries must use artifact file names exactly as published

## Packaging Rule

- no artifact may be published under a name that does not match the frozen version stem
- no checksum may be published for an artifact that was not produced by the frozen build
- no release directory may mix artifacts from different freeze dates
