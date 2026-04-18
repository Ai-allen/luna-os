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
      if ($text -match '\[LunaLoader\] Stage 1 plan chunks=[0-9A-F]{4} total_bytes=0x[0-9A-F]{8} entry=0x[0-9A-F]{8}' `
          -and $text -match '\[LunaLoader\] Stage 1 read chunk=0001/[0-9A-F]{4} offset=0x00000000 size=0x[0-9A-F]{8} total=0x[0-9A-F]{8} final=(yes|no) lba=0x[0-9A-F]{8}' `
          -and $text -match '\[LunaLoader\] Stage 1 copy chunk=0001/[0-9A-F]{4} offset=0x00000000 size=0x[0-9A-F]{8} total=0x[0-9A-F]{8} final=(yes|no) dest=0x[0-9A-F]{8}' `
          -and $text -match '\[LunaLoader\] Stage 2 loaded ok chunks=[0-9A-F]{4}/[0-9A-F]{4} total_bytes=0x[0-9A-F]{8} entry=0x[0-9A-F]{8}' `
          -and $text -match '\[USER\] shell ready' `
          -and $text -match '\[USER\] input lane ready' `
          -and $text -match '\[DEVICE\] disk path driver=piix-ide family=0000000C chain=ahci>fwblk>ata mode=normal' `
          -and $text -match '\[DEVICE\] disk select basis=piix-ide fwblk-src=missing fwblk-tgt=missing separate=missing ahci=missing pci=ready' `
          -and $text -match '\[DEVICE\] serial path driver=piix-uart family=00000012' `
          -and $text -match '\[DEVICE\] serial select basis=piix-pci pci=ready' `
          -and $text -match '\[DEVICE\] display select basis=std-vga-text gop=missing pci=ready' `
          -and $text -match '\[DEVICE\] input select basis=i8042 virtio-dev=missing virtio-ready=missing legacy=ready' `
          -and $text -match '\[DEVICE\] net path driver=e1000 family=0000000B lane=ready' `
          -and $text -match '\[DEVICE\] net select basis=e1000-ready pci=ready live=ready' `
          -and $text -match '\[DEVICE\] platform pci vendor=8086 device=1237 bdf=00:00.00 class=06/00/00 hdr=00') {
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
