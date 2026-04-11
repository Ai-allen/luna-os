[CmdletBinding()]
param(
  [string]$Machine = 'intel-uefi-ahci',
  [string]$VersionStem = 'ga-2026-04-10'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$artifactsRoot = Join-Path $root 'artifacts\physical_bringup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$sessionDir = Join-Path $artifactsRoot "$stamp-$Machine"
$imagePath = Join-Path $PSScriptRoot 'lunaos_esp.img'
$isoPath = Join-Path $PSScriptRoot 'lunaos_installer.iso'
$bootx64Path = Join-Path $PSScriptRoot 'manual_bootx64.efi'

New-Item -ItemType Directory -Force $sessionDir | Out-Null

$shaPath = Join-Path $sessionDir 'sha256.txt'
$notesPath = Join-Path $sessionDir 'operator-notes.txt'
$checklistPath = Join-Path $sessionDir 'checklist.txt'
$machinePath = Join-Path $sessionDir 'machine.txt'

$hashTargets = @($imagePath, $isoPath, $bootx64Path) | Where-Object { Test-Path $_ }
if ($hashTargets.Count -gt 0) {
  Get-FileHash -Algorithm SHA256 -Path $hashTargets |
    ForEach-Object { '{0}  {1}' -f $_.Hash.ToLowerInvariant(), [System.IO.Path]::GetFileName($_.Path) } |
    Set-Content -Encoding ascii $shaPath
}

@(
  "version_stem=$VersionStem"
  "machine=$Machine"
  "created_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
  "usb_image=$(Split-Path -Leaf $imagePath)"
  "installer_iso=$(Split-Path -Leaf $isoPath)"
  "bootx64=$(Split-Path -Leaf $bootx64Path)"
) | Set-Content -Encoding ascii $machinePath

@(
  '1. Verify target machine matches Milestone 1 support boundary: Intel x86_64, UEFI, Secure Boot off, single SATA disk.'
  '2. Run: pwsh -NoProfile -File .\build\write_lunaos_uefi_usb.ps1 -List'
  '3. Write USB: pwsh -NoProfile -File .\build\write_lunaos_uefi_usb.ps1 -DiskNumber <N>'
  '4. Capture firmware boot choice and any LunaLoader screen output.'
  '5. Record the last visible LunaLoader line if boot stops before stage jump.'
  '6. Record whether [BOOT] dawn online and [USER] shell ready appear.'
  '7. If the board exposes serial, capture COM1 at 38400 8N1 and save raw log beside this checklist.'
  '8. Record storage mode, GOP resolution, keyboard path, and whether shell accepts input.'
  '9. Keep photos of the final visible screen in this session directory.'
  '10. Do not switch to NVMe/RAID/VMD during Milestone 1.'
) | Set-Content -Encoding ascii $checklistPath

@(
  'Paste run notes here.'
  'Include: machine model, firmware version, SATA mode, USB port used, last visible line, shell-ready result.'
) | Set-Content -Encoding ascii $notesPath

Write-Host "Physical bring-up session prepared: $sessionDir"
