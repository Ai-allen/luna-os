$disk = Join-Path $PSScriptRoot "lunaos_disk.img"
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

$firmware = $env:LUNA_OVMF_CODE
if (-not $firmware) {
  foreach ($candidate in @(
    'C:\Program Files\qemu\share\edk2-x86_64-code.fd',
    'C:\Program Files\QEMU\share\edk2-x86_64-code.fd'
  )) {
    if (Test-Path $candidate) {
      $firmware = $candidate
      break
    }
  }
}
if (-not $firmware) {
  throw 'OVMF firmware not found. Set LUNA_OVMF_CODE to an edk2 x86_64 code fd file.'
}

$varsTemplate = $env:LUNA_OVMF_VARS
if (-not $varsTemplate) {
  foreach ($candidate in @(
    'C:\Program Files\qemu\share\edk2-i386-vars.fd',
    'C:\Program Files\QEMU\share\edk2-i386-vars.fd',
    (Join-Path $PSScriptRoot 'ovmf-vars.fd')
  )) {
    if (Test-Path $candidate) {
      $varsTemplate = $candidate
      break
    }
  }
}
if (-not $varsTemplate) {
  throw 'OVMF vars firmware not found. Set LUNA_OVMF_VARS to a writable vars fd template.'
}

$varsRun = Join-Path $PSScriptRoot 'ovmf-run-vars.fd'
Copy-Item $varsTemplate $varsRun -Force
$serialMode = 'stdio'
if ($env:LUNA_QEMU_MONITOR) {
  $monitorMode = $env:LUNA_QEMU_MONITOR
} else {
  $monitorMode = $null
}
if ($env:LUNA_QEMU_SERIAL_LOG) {
  $serialMode = "file:$($env:LUNA_QEMU_SERIAL_LOG)"
}

$args = @(
  '-machine', 'q35',
  '-device', 'virtio-keyboard-pci',
  '-drive', "if=pflash,format=raw,readonly=on,file=$firmware",
  '-drive', "if=pflash,format=raw,file=$varsRun",
  '-drive', "format=raw,file=$disk",
  '-display', 'gtk,show-cursor=on,grab-on-hover=on,zoom-to-fit=off',
  '-serial', $serialMode,
  '-no-reboot',
  '-no-shutdown'
)
if ($monitorMode) {
  $args += @('-monitor', $monitorMode)
}

& $qemu.Source @args
