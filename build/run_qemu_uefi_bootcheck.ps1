$script = Join-Path $PSScriptRoot "run_qemu_uefi_bootcheck.py"
python $script
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
