$image = Join-Path $PSScriptRoot "lunaos.img"
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
$serialMode = 'stdio'
if ($env:LUNA_QEMU_SERIAL_LOG) {
  $serialMode = "file:$($env:LUNA_QEMU_SERIAL_LOG)"
}
$args = @(
  '-drive', "format=raw,file=$image",
  '-serial', $serialMode,
  '-display', 'gtk,show-cursor=on,grab-on-hover=on,zoom-to-fit=off',
  '-no-reboot',
  '-no-shutdown'
)

& $qemu.Source @args
