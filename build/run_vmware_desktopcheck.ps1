[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$vmxPath = Join-Path $PSScriptRoot 'vmware_lunaos_desktopcheck.vmx'
$rawDiskPath = Join-Path $PSScriptRoot 'lunaos_vmware_desktopcheck.img'
$sourceDiskPath = Join-Path $PSScriptRoot 'lunaos_disk.img'
$prepareScript = Join-Path $PSScriptRoot 'prepare_vmware_desktopcheck.py'
$vmdkPath = Join-Path $PSScriptRoot 'vmware_desktopcheck.vmdk'
$serialPath = Join-Path $PSScriptRoot 'vmware_desktopcheck.serial.log'
$vmwareLogPath = Join-Path $PSScriptRoot 'vmware_desktopcheck.vmware.log'
$screenPath = Join-Path $PSScriptRoot 'vmware_desktopcheck.png'
$vmrun = 'C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe'
$qemuImg = @(
  'C:\Program Files\QEMU\qemu-img.exe',
  'C:\Program Files\qemu\qemu-img.exe'
) | Where-Object { Test-Path $_ } | Select-Object -First 1

function Throw-VmwareGateFailure {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Category,
    [Parameter(Mandatory = $true)]
    [string]$Detail,
    [int]$Attempt = 0
  )

  $message = "VMware desktopcheck $Category"
  if ($Attempt -gt 0) {
    $message += " on attempt $Attempt"
  }
  throw "${message}: $Detail"
}

if (-not (Test-Path $vmxPath)) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "missing VMX: $vmxPath"
}
if (-not (Test-Path $vmrun)) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "missing vmrun.exe: $vmrun"
}
if (-not (Test-Path $sourceDiskPath)) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "missing source disk: $sourceDiskPath"
}
if (-not (Test-Path $prepareScript)) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "missing prepare script: $prepareScript"
}
if (-not $qemuImg) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail 'missing qemu-img.exe'
}

function Copy-LatestVmwareLog {
  $vmwareLog = Get-ChildItem $PSScriptRoot -Filter 'vmware*.log' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if ($vmwareLog) {
    Copy-Item $vmwareLog.FullName $vmwareLogPath -Force
  }
}

function Invoke-VmwareAttempt {
  param(
    [int]$Attempt
  )

  Remove-Item $serialPath -Force -ErrorAction SilentlyContinue
  Remove-Item $vmwareLogPath -Force -ErrorAction SilentlyContinue
  Remove-Item $screenPath -Force -ErrorAction SilentlyContinue

  & $vmrun -T ws stop $vmxPath hard *> $null
  Start-Sleep -Seconds 2

  $startOutput = & $vmrun -T ws start $vmxPath nogui 2>&1
  if ($LASTEXITCODE -ne 0) {
    Copy-LatestVmwareLog
    $detail = ($startOutput | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($detail)) {
      $detail = 'vmrun returned no diagnostic output'
    }
    Throw-VmwareGateFailure -Category 'host-start failure' -Detail $detail -Attempt $Attempt
  }

  try {
    $deadline = (Get-Date).AddSeconds(45)
    do {
      Start-Sleep -Milliseconds 500
      if (Test-Path $serialPath) {
        $text = Get-Content $serialPath -Raw -Encoding utf8
        if (-not [string]::IsNullOrEmpty($text) -and
            $text.Contains('[USER] shell ready') -and
            $text.Contains('desktop.launch Files') -and
            $text.Contains('[DESKTOP] launch Files ok')) {
          break
        }
      }
    } while ((Get-Date) -lt $deadline)

    & $vmrun -T ws captureScreen $vmxPath $screenPath *> $null
    Copy-LatestVmwareLog
  }
  finally {
    & $vmrun -T ws stop $vmxPath hard *> $null
    Start-Sleep -Seconds 2
  }

  if (-not (Test-Path $serialPath)) {
    Copy-LatestVmwareLog
    Throw-VmwareGateFailure -Category 'log-capture failure' -Detail 'serial log was not created after VMware run' -Attempt $Attempt
  }
}

python $prepareScript | Out-Null
if (-not (Test-Path $rawDiskPath)) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "missing prepared raw disk: $rawDiskPath"
}
Remove-Item $vmdkPath -Force -ErrorAction SilentlyContinue
& $qemuImg convert -f raw -O vmdk $rawDiskPath $vmdkPath | Out-Null
if ($LASTEXITCODE -ne 0) {
  Throw-VmwareGateFailure -Category 'host-start failure' -Detail "qemu-img convert failed for $rawDiskPath -> $vmdkPath"
}

for ($attempt = 1; $attempt -le 2; $attempt++) {
  try {
    Invoke-VmwareAttempt -Attempt $attempt
    break
  }
  catch {
    if ($attempt -eq 2) {
      throw
    }
    Start-Sleep -Seconds 3
  }
}

if (-not (Test-Path $serialPath)) {
  Throw-VmwareGateFailure -Category 'log-capture failure' -Detail 'serial log was not created after VMware retries'
}

$serial = Get-Content $serialPath -Raw -Encoding utf8
$required = @(
  '[BOOT] dawn online',
  '[GRAPHICS] console ready',
  '[USER] shell ready',
  '[USER] input lane ready',
  'first-setup required: no hostname or user configured',
  'setup.init luna dev secret',
  'setup.init ok: host and first user created',
  'login ok: session active',
  'desktop.boot',
  '[DESKTOP] boot ok',
  'desktop.launch Settings',
  '[DESKTOP] launch Settings ok',
  'desktop.launch Files',
  '[DESKTOP] launch Files ok',
  'desktop.launch Console',
  '[DESKTOP] launch Console ok',
  'dev@luna:~$'
)

foreach ($needle in $required) {
  if (-not $serial.Contains($needle)) {
    Throw-VmwareGateFailure -Category 'guest failure' -Detail "missing expected output: $needle"
  }
}

Get-Content $serialPath -Encoding utf8
