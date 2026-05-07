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
$finalizePath = Join-Path $sessionDir 'finalize-session.ps1'
$summaryPath = Join-Path $sessionDir 'firsthop-summary.txt'
$classificationPath = Join-Path $sessionDir 'firsthop-classification.txt'
$referencePath = Join-Path $sessionDir 'firsthop-reference.txt'
$deltaPath = Join-Path $sessionDir 'firsthop-delta.txt'
$verdictPath = Join-Path $sessionDir 'firsthop-verdict.txt'
$manifestPath = Join-Path $sessionDir 'evidence-manifest.txt'

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
  'target_support_cell=intel-x86_64+uefi+sata-ahci+gop+keyboard'
  'evidence_scope=physical-candidate'
  'support_cell_status=not-established-until-reviewed'
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
  '8. Record storage mode, GOP result, keyboard path, and whether shell accepts input.'
  '9. Capture [DEVICE] * path and [DEVICE] * select lines so driver-family and selection basis can be compared against the virtualized baseline.'
  '10. Keep photos of the final visible screen in this session directory.'
  '11. Fill every operator-notes.txt key before finalization; empty keys keep physical_evidence_status=missing.'
  '12. Run .\finalize-session.ps1 after saving serial-capture.log to generate firsthop-summary.txt / firsthop-classification.txt / firsthop-delta.txt.'
  '13. Confirm firsthop-verdict.txt says evidence_scope=physical-candidate and physical_evidence_status=present before treating the result as real support-cell evidence.'
  '14. Do not switch to NVMe/RAID/VMD during Milestone 1.'
) | Set-Content -Encoding ascii $checklistPath

@(
  'Replace every value before finalization.'
  'machine_model='
  'firmware_version='
  'sata_mode='
  'usb_port='
  'capture_source=serial|display-photo|operator-transcript'
  'last_visible_line='
  'shell_ready=yes|no|not-reached'
  'gop_result=ready|missing|not-reached'
  'keyboard_result=ready|blocked|not-reached'
) | Set-Content -Encoding ascii $notesPath

@(
  'Awaiting serial capture.'
  'Run .\finalize-session.ps1 after placing serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $summaryPath

@(
  'Awaiting firsthop classification.'
  'Run .\finalize-session.ps1 after placing serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $classificationPath

@(
  'Awaiting firsthop baseline selection.'
  'Run .\finalize-session.ps1 after placing serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $referencePath

@(
  'Awaiting firsthop baseline delta.'
  'Run .\finalize-session.ps1 after placing serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $deltaPath

@(
  'Awaiting firsthop verdict.'
  'Run .\finalize-session.ps1 after placing serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $verdictPath

@(
  'Awaiting physical evidence manifest.'
  'Run .\finalize-session.ps1 after placing the physical capture log in this directory.'
  'The final manifest records capture log, operator notes, and machine metadata hashes.'
) | Set-Content -Encoding ascii $manifestPath

@(
  'param([string]$LogPath = (Join-Path $PSScriptRoot ''serial-capture.log''))'
  ('$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ''..\..\..''))')
  ('& (Join-Path $repoRoot ''build\finalize_bringup_session.ps1'') -SessionDir $PSScriptRoot -LogPath $LogPath')
) | Set-Content -Encoding ascii $finalizePath

Write-Host "Physical bring-up session prepared: $sessionDir"
