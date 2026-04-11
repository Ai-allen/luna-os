$ErrorActionPreference = 'Stop'
python (Join-Path $PSScriptRoot 'run_qemu_fullregression.py')
exit $LASTEXITCODE
