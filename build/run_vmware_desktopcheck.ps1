[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
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

if (-not (Test-Path $vmxPath)) {
  throw "Missing VMX: $vmxPath"
}
if (-not (Test-Path $vmrun)) {
  throw "Missing vmrun.exe: $vmrun"
}
if (-not (Test-Path $sourceDiskPath)) {
  throw "Missing source disk: $sourceDiskPath"
}
if (-not (Test-Path $prepareScript)) {
  throw "Missing prepare script: $prepareScript"
}
if (-not $qemuImg) {
  throw 'Missing qemu-img.exe'
}

python $prepareScript | Out-Null
if (-not (Test-Path $rawDiskPath)) {
  throw "Missing prepared raw disk: $rawDiskPath"
}
Remove-Item $vmdkPath -Force -ErrorAction SilentlyContinue
& $qemuImg convert -f raw -O vmdk $rawDiskPath $vmdkPath | Out-Null

Remove-Item $serialPath -Force -ErrorAction SilentlyContinue
Remove-Item $vmwareLogPath -Force -ErrorAction SilentlyContinue
Remove-Item $screenPath -Force -ErrorAction SilentlyContinue

& $vmrun -T ws start $vmxPath nogui | Out-Null
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

  & $vmrun -T ws captureScreen $vmxPath $screenPath | Out-Null

  $vmwareLog = Get-ChildItem $PSScriptRoot -Filter 'vmware*.log' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if ($vmwareLog) {
    Copy-Item $vmwareLog.FullName $vmwareLogPath -Force
  }
}
finally {
  & $vmrun -T ws stop $vmxPath hard | Out-Null
}

if (-not (Test-Path $serialPath)) {
  throw 'VMware desktopcheck missing serial log'
}

$serial = Get-Content $serialPath -Raw -Encoding utf8
$required = @(
  '[BOOT] dawn online',
  '[GRAPHICS] console ready',
  '[USER] shell ready',
  '[USER] input lane ready',
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
    throw "VMware desktopcheck missing expected output: $needle"
  }
}

Get-Content $serialPath -Encoding utf8
