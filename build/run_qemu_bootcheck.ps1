$image = Join-Path $PSScriptRoot "lunaos.img"
$bootcheckImage = Join-Path $PSScriptRoot "lunaos_bootcheck.img"
$qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
if (-not $qemu) {
  foreach ($candidate in @(
    'C:\Program Files\qemu\qemu-system-x86_64.exe',
    'C:\Program Files\QEMU\qemu-system-x86_64.exe'
  )) {
    if (Test-Path $candidate) {
      $qemu = @{ Source = $candidate }
      break
    }
  }
}
if (-not $qemu) {
  throw 'qemu-system-x86_64 not found. Install QEMU or add it to PATH.'
}

$stdoutPath = Join-Path $PSScriptRoot "qemu_bootcheck.log"
$stderrPath = Join-Path $PSScriptRoot "qemu_bootcheck.err.log"
if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }
if (Test-Path $stderrPath) { Remove-Item $stderrPath -Force }
Copy-Item $image $bootcheckImage -Force

$proc = Start-Process -FilePath $qemu.Source `
  -ArgumentList @(
    '-drive', "format=raw,file=$bootcheckImage",
    '-serial', 'stdio',
    '-display', 'none',
    '-no-reboot',
    '-no-shutdown'
  ) `
  -RedirectStandardOutput $stdoutPath `
  -RedirectStandardError $stderrPath `
  -PassThru

try {
  $deadline = (Get-Date).AddSeconds(25)
  $matched = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    if ($proc.HasExited) { break }
    if (Test-Path $stdoutPath) {
      $text = Get-Content $stdoutPath -Raw
      if ($text -match '\[USER\] shell ready' -and $text -match '\[USER\] input lane ready') {
        $matched = $true
        break
      }
    }
  }
  if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
  }
  if (Test-Path $stdoutPath) {
    Get-Content $stdoutPath
  }
  if (-not $matched) {
    throw 'bootcheck did not reach the USER session before timeout'
  }
} finally {
  if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
  }
}
