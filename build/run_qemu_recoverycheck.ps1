$script = Join-Path $PSScriptRoot "run_qemu_recoverycheck.py"
python $script
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
