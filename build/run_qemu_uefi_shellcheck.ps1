python (Join-Path $PSScriptRoot 'run_qemu_uefi_shellcheck.py')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
