[CmdletBinding()]
param(
  [string]$Machine = 'vmware-uefi-m1',
  [string]$VersionStem = 'ga-2026-04-10'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$artifactsRoot = Join-Path $root 'artifacts\vmware_bringup'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$sessionDir = Join-Path $artifactsRoot "$stamp-$Machine"
$isoPath = Join-Path $PSScriptRoot 'lunaos_installer.iso'
$efiPath = Join-Path $PSScriptRoot 'manual_bootx64.efi'
$vmxTemplatePath = Join-Path $PSScriptRoot 'vmware_lunaos_m1.vmx'

New-Item -ItemType Directory -Force $sessionDir | Out-Null

$shaPath = Join-Path $sessionDir 'sha256.txt'
$notesPath = Join-Path $sessionDir 'operator-notes.txt'
$checklistPath = Join-Path $sessionDir 'checklist.txt'
$machinePath = Join-Path $sessionDir 'machine.txt'
$vmxPath = Join-Path $sessionDir 'lunaos-vmware-m1.vmx'
$finalizePath = Join-Path $sessionDir 'finalize-session.ps1'
$summaryPath = Join-Path $sessionDir 'firsthop-summary.txt'
$classificationPath = Join-Path $sessionDir 'firsthop-classification.txt'
$referencePath = Join-Path $sessionDir 'firsthop-reference.txt'
$deltaPath = Join-Path $sessionDir 'firsthop-delta.txt'
$verdictPath = Join-Path $sessionDir 'firsthop-verdict.txt'

$hashTargets = @($isoPath, $efiPath) | Where-Object { Test-Path $_ }
if ($hashTargets.Count -gt 0) {
  Get-FileHash -Algorithm SHA256 -Path $hashTargets |
    ForEach-Object { '{0}  {1}' -f $_.Hash.ToLowerInvariant(), [System.IO.Path]::GetFileName($_.Path) } |
    Set-Content -Encoding ascii $shaPath
}

@(
  "version_stem=$VersionStem"
  "machine=$Machine"
  "created_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
  "boot_iso=$(Split-Path -Leaf $isoPath)"
  "boot_efi=$(Split-Path -Leaf $efiPath)"
  'firmware=efi'
  'storage=sata-cdrom + optional sata-disk'
  'network=disconnected or e1000e observe-only'
) | Set-Content -Encoding ascii $machinePath

@(
  '1. Create a new VMware Workstation VM from the generated .vmx or equivalent settings.'
  '2. Keep firmware = UEFI and Secure Boot disabled.'
  '3. Boot from lunaos_installer.iso first; do not start from a custom VMDK conversion path.'
  '4. Keep one vCPU and 1024 MB RAM for first bring-up unless firmware issues require 2 vCPU.'
  '5. Use SATA CD-ROM for the ISO and SATA disk only if persistence needs checking later.'
  '6. Prefer network disconnected for the first shell-ready bring-up; if attached, use e1000e as observe-only.'
  '7. Record the last visible LunaLoader line and whether [BOOT] dawn online appears.'
  '8. Record whether [GRAPHICS] framebuffer ready or [GRAPHICS] console ready appears.'
  '9. Record whether [USER] shell ready appears and whether setup/status or BOOT state can be read.'
  '10. Save vmware.log / serial capture into this session directory after each run.'
  '11. Run .\finalize-session.ps1 after saving vmware_m1.serial.log or serial-capture.log to generate firsthop-summary.txt / firsthop-classification.txt / firsthop-delta.txt.'
) | Set-Content -Encoding ascii $checklistPath

@(
  'Paste VMware run notes here.'
  'Include: VMware Workstation version, firmware setting, virtual hardware version, display mode, last visible line, shell-ready result.'
) | Set-Content -Encoding ascii $notesPath

@(
  'Awaiting VMware serial capture.'
  'Run .\finalize-session.ps1 after placing vmware_m1.serial.log or serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $summaryPath

@(
  'Awaiting firsthop classification.'
  'Run .\finalize-session.ps1 after placing vmware_m1.serial.log or serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $classificationPath

@(
  'Awaiting firsthop baseline selection.'
  'Run .\finalize-session.ps1 after placing vmware_m1.serial.log or serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $referencePath

@(
  'Awaiting firsthop baseline delta.'
  'Run .\finalize-session.ps1 after placing vmware_m1.serial.log or serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $deltaPath

@(
  'Awaiting firsthop verdict.'
  'Run .\finalize-session.ps1 after placing vmware_m1.serial.log or serial-capture.log in this directory.'
) | Set-Content -Encoding ascii $verdictPath

@(
  'param([string]$LogPath = '''')'
  'if ([string]::IsNullOrWhiteSpace($LogPath)) {'
  '  $candidate = Join-Path $PSScriptRoot ''vmware_m1.serial.log'''
  '  if (Test-Path $candidate) {'
  '    $LogPath = $candidate'
  '  } else {'
  '    $LogPath = Join-Path $PSScriptRoot ''serial-capture.log'''
  '  }'
  '}'
  ('$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ''..\..\..''))')
  ('& (Join-Path $repoRoot ''build\finalize_bringup_session.ps1'') -SessionDir $PSScriptRoot -LogPath $LogPath')
) | Set-Content -Encoding ascii $finalizePath

if (Test-Path $vmxTemplatePath) {
  Copy-Item $vmxTemplatePath $vmxPath -Force
}

Write-Host "VMware bring-up session prepared: $sessionDir"
