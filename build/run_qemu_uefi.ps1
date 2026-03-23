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

& $qemu.Source `
  -machine q35 `
  -drive "if=pflash,format=raw,readonly=on,file=$firmware" `
  -drive "format=raw,file=$disk" `
  -serial stdio `
  -display none `
  -no-reboot `
  -no-shutdown
