$root = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $PSScriptRoot 'run_qemu.ps1'
Start-Process 'C:\Program Files\PowerShell\7\pwsh.exe' -ArgumentList @(
  '-NoExit',
  '-Command',
  "Set-Location '$root'; & '$runner'"
)
