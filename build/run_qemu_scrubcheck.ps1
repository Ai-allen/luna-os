$script = Join-Path $PSScriptRoot "run_qemu_scrubcheck.py"
python $script
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
