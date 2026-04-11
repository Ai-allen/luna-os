[CmdletBinding()]
param(
  [int]$DiskNumber = -1,
  [switch]$List,
  [switch]$Force,
  [switch]$SkipVerify
)

$args = @{
  ImagePath = (Join-Path $PSScriptRoot 'lunaos_esp.img')
}

if ($DiskNumber -ge 0) {
  $args.DiskNumber = $DiskNumber
}
if ($List) {
  $args.List = $true
}
if ($Force) {
  $args.Force = $true
}
if ($SkipVerify) {
  $args.SkipVerify = $true
}

& (Join-Path $PSScriptRoot 'write_lunaos_usb.ps1') @args
exit $LASTEXITCODE
