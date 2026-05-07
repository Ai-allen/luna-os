[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$vmxPath = Join-Path $PSScriptRoot 'vmware_lunaos_inputcheck.vmx'
$vmxTemplatePath = Join-Path $PSScriptRoot 'vmware_lunaos_desktopcheck.vmx'
$vmxfPath = Join-Path $PSScriptRoot 'vmware_lunaos_inputcheck.vmxf'
$rawDiskPath = Join-Path $PSScriptRoot 'lunaos_vmware_inputcheck.img'
$sourceDiskPath = Join-Path $PSScriptRoot 'lunaos_disk.img'
$vmdkPath = Join-Path $PSScriptRoot 'vmware_inputcheck.vmdk'
$serialPath = Join-Path $PSScriptRoot 'vmware_inputcheck.serial.log'
$screenPath = Join-Path $PSScriptRoot 'vmware_inputcheck.png'
$vmwareLogPath = Join-Path $PSScriptRoot 'vmware_inputcheck.vmware.log'
$vncHost = '127.0.0.1'
$vncPort = 5997
$vncPassword = 'lunaos'
$vmrun = 'C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe'
$qemuImg = @(
  'C:\Program Files\QEMU\qemu-img.exe',
  'C:\Program Files\qemu\qemu-img.exe'
) | Where-Object { Test-Path $_ } | Select-Object -First 1

$forbiddenCommon = @(
  'Exception Type',
  "Can't find image information",
  '[SYSTEM] driver gate=recovery',
  '[USER] input lane src=operator',
  '[DEVICE] input event src=serial-operator',
  '[USER] input recovery=operator-shell',
  '[USER] session script cmd=',
  'setup.init luna dev secret',
  "desktop.launch Files`r`n[USER] session script cmd=desktop.launch Files"
)

function Throw-VmwareInputFailure {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Category,
    [Parameter(Mandatory = $true)]
    [string]$Detail
  )

  throw "VMware inputcheck ${Category}: $Detail"
}

function Convert-HexLiteral {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text
  )

  return [Convert]::ToUInt64(($Text -replace '^0x', ''), 16)
}

function ConvertTo-VmxPath {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path
  )

  return ([System.IO.Path]::GetFullPath($Path)).Replace('\', '\\')
}

function Set-VmxString {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text,
    [Parameter(Mandatory = $true)]
    [string]$Key,
    [Parameter(Mandatory = $true)]
    [string]$Value
  )

  $lines = $Text -split "\r?\n"
  $found = 0
  $pattern = '^{0} = ".*"$' -f [regex]::Escape($Key)
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match $pattern) {
      $lines[$i] = '{0} = "{1}"' -f $Key, $Value
      $found += 1
    }
  }
  if ($found -ne 1) {
    Throw-VmwareInputFailure -Category 'host-start failure' -Detail "could not replace VMX key: $Key"
  }
  return ($lines -join "`r`n")
}

function New-InputCheckVmx {
  $text = Get-Content $vmxTemplatePath -Raw -Encoding utf8
  $text = Set-VmxString -Text $text -Key 'displayName' -Value 'LunaOS VMware InputCheck'
  $text = Set-VmxString -Text $text -Key 'serial0.fileName' -Value (ConvertTo-VmxPath $serialPath)
  $text = Set-VmxString -Text $text -Key 'sata0:0.fileName' -Value (ConvertTo-VmxPath $vmdkPath)
  $text = Set-VmxString -Text $text -Key 'extendedConfigFile' -Value 'vmware_lunaos_inputcheck.vmxf'
  $text = Set-VmxString -Text $text -Key 'vmxstats.filename' -Value 'vmware_lunaos_inputcheck.scoreboard'
  $text += "`r`nRemoteDisplay.vnc.enabled = `"TRUE`""
  $text += "`r`nRemoteDisplay.vnc.port = `"$vncPort`""
  $text += "`r`nRemoteDisplay.vnc.password = `"$vncPassword`""
  $text += "`r`nRemoteDisplay.vnc.keyMap = `"us101`""
  Set-Content -Path $vmxPath -Value $text -Encoding utf8

  $vmId = ([guid]::NewGuid().ToString('N') -replace '(.{2})(?=.)', '$1 ').Trim()
  $vmId = $vmId.Substring(0, 23) + '-' + $vmId.Substring(24)
  $vmxf = @(
    '<?xml version="1.0"?>',
    '<Foundry>',
    '<VM>',
    "<VMId type=`"string`">$vmId</VMId>",
    '<ClientMetaData>',
    '<clientMetaDataAttributes/>',
    '<HistoryEventList/></ClientMetaData>',
    "<vmxPathName type=`"string`">$(Split-Path -Leaf $vmxPath)</vmxPathName></VM></Foundry>"
  ) -join "`n"
  Set-Content -Path $vmxfPath -Value $vmxf -Encoding utf8
}

function Get-SessionLayout {
  $imageBase = $null
  foreach ($line in Get-Content (Join-Path $PSScriptRoot 'luna.ld') -Encoding utf8) {
    $parts = $line.Trim() -split '\s+'
    if ($parts.Count -eq 2 -and $parts[0] -eq 'IMAGE_BASE') {
      $imageBase = Convert-HexLiteral $parts[1]
    }
    if ($parts.Count -eq 4 -and $parts[0] -eq 'SCRIPT' -and $parts[1] -eq 'SESSION') {
      if ($null -eq $imageBase) {
        Throw-VmwareInputFailure -Category 'host-start failure' -Detail 'missing IMAGE_BASE entry before SCRIPT SESSION'
      }
      return [PSCustomObject]@{
        ImageBase = $imageBase
        ScriptBase = Convert-HexLiteral $parts[2]
        ScriptSize = Convert-HexLiteral $parts[3]
      }
    }
  }
  Throw-VmwareInputFailure -Category 'host-start failure' -Detail 'missing SCRIPT SESSION layout entry'
}

function Clear-SessionScript {
  param(
    [Parameter(Mandatory = $true)]
    [string]$DiskPath
  )

  $layout = Get-SessionLayout
  $scriptSize = [int]$layout.ScriptSize
  $blob = [System.IO.File]::ReadAllBytes($DiskPath)
  $rootDirSectors = [int][Math]::Floor((512 * 32 + 512 - 1) / 512)
  $dataStartLba = 1 + (2 * 64) + $rootDirSectors
  $stageLba = 2048 + $dataStartLba + (64 - 2)
  $offset = ([int64]$stageLba * 512) + ([int64]$layout.ScriptBase - [int64]$layout.ImageBase)
  $payload = New-Object byte[] $scriptSize

  [BitConverter]::GetBytes([uint32]0x54504353).CopyTo($payload, 0)
  [BitConverter]::GetBytes([uint32]0).CopyTo($payload, 4)
  [Array]::Copy($payload, 0, $blob, $offset, $scriptSize)
  [System.IO.File]::WriteAllBytes($DiskPath, $blob)
}

function Initialize-InputDisk {
  Remove-Item $rawDiskPath -Force -ErrorAction SilentlyContinue
  Remove-Item $vmdkPath -Force -ErrorAction SilentlyContinue
  Remove-Item $serialPath -Force -ErrorAction SilentlyContinue
  Remove-Item $screenPath -Force -ErrorAction SilentlyContinue
  Remove-Item $vmwareLogPath -Force -ErrorAction SilentlyContinue
  Remove-Item $vmxPath -Force -ErrorAction SilentlyContinue
  Remove-Item $vmxfPath -Force -ErrorAction SilentlyContinue

  Copy-Item $sourceDiskPath $rawDiskPath -Force
  Clear-SessionScript -DiskPath $rawDiskPath
  & $qemuImg convert -f raw -O vmdk $rawDiskPath $vmdkPath | Out-Null
  if ($LASTEXITCODE -ne 0) {
    Throw-VmwareInputFailure -Category 'host-start failure' -Detail "qemu-img convert failed for $rawDiskPath"
  }
}

function Invoke-VmrunChecked {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$VmrunArgs,
    [Parameter(Mandatory = $true)]
    [string]$Failure
  )

  $output = & $vmrun -T ws @VmrunArgs 2>&1
  if ($LASTEXITCODE -ne 0) {
    $detail = ($output | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($detail)) {
      $detail = "vmrun exited with $LASTEXITCODE"
    }
    Throw-VmwareInputFailure -Category $Failure -Detail $detail
  }
}

function Invoke-VmrunOptional {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$VmrunArgs
  )

  & $vmrun -T ws @VmrunArgs *> $null
}

function Copy-LatestVmwareLog {
  $vmwareLog = Get-ChildItem $PSScriptRoot -Filter 'vmware*.log' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if ($vmwareLog) {
    Copy-Item $vmwareLog.FullName $vmwareLogPath -Force
  }
}

function Read-SerialLog {
  if (-not (Test-Path $serialPath)) {
    return ''
  }
  $text = Get-Content $serialPath -Raw -Encoding utf8
  if ($null -eq $text) {
    return ''
  }
  return ($text -replace "`e\[[0-9;?=]*[ -/]*[@-~]", '')
}

function Assert-NoForbidden {
  param(
    [Parameter(Mandatory = $true)]
    [AllowNull()]
    [object]$Text
  )

  if ($null -eq $Text) {
    $Text = ''
  } elseif ($Text -is [array]) {
    $Text = ($Text -join "`n")
  } else {
    $Text = [string]$Text
  }

  foreach ($needle in $forbiddenCommon) {
    if ($Text.Contains($needle)) {
      Throw-VmwareInputFailure -Category 'provenance failure' -Detail "forbidden output observed: $needle"
    }
  }
}

function Wait-ForLog {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$Required,
    [Parameter(Mandatory = $true)]
    [double]$TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  do {
    $text = Read-SerialLog
    Assert-NoForbidden -Text $text
    $ok = $true
    foreach ($needle in $Required) {
      if (-not $text.Contains($needle)) {
        $ok = $false
        break
      }
    }
    if ($ok) {
      return $text
    }
    Start-Sleep -Milliseconds 50
  } while ((Get-Date) -lt $deadline)

  $text = Read-SerialLog
  Assert-NoForbidden -Text $text
  foreach ($needle in $Required) {
    if (-not $text.Contains($needle)) {
      Throw-VmwareInputFailure -Category 'guest failure' -Detail "missing expected output: $needle"
    }
  }
  return $text
}

function Wait-ForOrderedPatterns {
  param(
    [Parameter(Mandatory = $true)]
    [string[]]$Patterns,
    [Parameter(Mandatory = $true)]
    [double]$TimeoutSeconds,
    [Parameter(Mandatory = $true)]
    [string]$Description
  )

  $compiled = @($Patterns | ForEach-Object { [regex]::new($_) })
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  do {
    $text = Read-SerialLog
    Assert-NoForbidden -Text $text
    $offset = 0
    $matched = $true
    foreach ($pattern in $compiled) {
      $match = $pattern.Match($text, $offset)
      if (-not $match.Success) {
        $matched = $false
        break
      }
      $offset = $match.Index + $match.Length
    }
    if ($matched) {
      return $text
    }
    Start-Sleep -Milliseconds 50
  } while ((Get-Date) -lt $deadline)

  Throw-VmwareInputFailure -Category 'provenance failure' -Detail "missing ordered keyboard path: $Description"
}

function Read-ExactBytes {
  param(
    [Parameter(Mandatory = $true)]
    [System.IO.Stream]$Stream,
    [Parameter(Mandatory = $true)]
    [int]$Count
  )

  $buffer = New-Object byte[] $Count
  $offset = 0
  while ($offset -lt $Count) {
    $read = $Stream.Read($buffer, $offset, $Count - $offset)
    if ($read -le 0) {
      Throw-VmwareInputFailure -Category 'host-input blocker' -Detail 'VNC connection closed during keyboard injection'
    }
    $offset += $read
  }
  return $buffer
}

function Read-U32BE {
  param(
    [Parameter(Mandatory = $true)]
    [byte[]]$Bytes,
    [Parameter(Mandatory = $true)]
    [int]$Offset
  )

  return (([uint32]$Bytes[$Offset] -shl 24) -bor
    ([uint32]$Bytes[$Offset + 1] -shl 16) -bor
    ([uint32]$Bytes[$Offset + 2] -shl 8) -bor
    [uint32]$Bytes[$Offset + 3])
}

function Write-VncKeyEvent {
  param(
    [Parameter(Mandatory = $true)]
    [System.IO.Stream]$Stream,
    [Parameter(Mandatory = $true)]
    [uint32]$Keysym,
    [Parameter(Mandatory = $true)]
    [bool]$Down
  )

  $message = New-Object byte[] 8
  $message[0] = 4
  $message[1] = if ($Down) { 1 } else { 0 }
  $message[4] = [byte](($Keysym -shr 24) -band 0xFF)
  $message[5] = [byte](($Keysym -shr 16) -band 0xFF)
  $message[6] = [byte](($Keysym -shr 8) -band 0xFF)
  $message[7] = [byte]($Keysym -band 0xFF)
  $Stream.Write($message, 0, $message.Length)
  $Stream.Flush()
}

function ConvertTo-VncDesKey {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Password
  )

  $raw = [System.Text.Encoding]::ASCII.GetBytes($Password)
  $key = New-Object byte[] 8
  for ($i = 0; $i -lt 8; $i++) {
    $value = 0
    if ($i -lt $raw.Length) {
      $value = [int]$raw[$i]
    }
    $reversed = 0
    for ($bit = 0; $bit -lt 8; $bit++) {
      $reversed = ($reversed -shl 1) -bor (($value -shr $bit) -band 1)
    }
    $key[$i] = [byte]$reversed
  }
  return $key
}

function Invoke-VncAuth {
  param(
    [Parameter(Mandatory = $true)]
    [System.IO.Stream]$Stream
  )

  $challenge = Read-ExactBytes -Stream $Stream -Count 16
  $des = [System.Security.Cryptography.DES]::Create()
  try {
    $des.Mode = [System.Security.Cryptography.CipherMode]::ECB
    $des.Padding = [System.Security.Cryptography.PaddingMode]::None
    $des.Key = ConvertTo-VncDesKey -Password $vncPassword
    $encryptor = $des.CreateEncryptor()
    $response = $encryptor.TransformFinalBlock($challenge, 0, $challenge.Length)
    $Stream.Write($response, 0, $response.Length)
    $Stream.Flush()
  }
  finally {
    $des.Dispose()
  }
}

function ConvertTo-Keysyms {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text
  )

  $keys = New-Object System.Collections.Generic.List[uint32]
  foreach ($ch in $Text.ToCharArray()) {
    $code = [int][char]$ch
    if ($code -eq 10 -or $code -eq 13) {
      $keys.Add([uint32]0xFF0D)
    } elseif ($code -ge 0x20 -and $code -le 0x7E) {
      $keys.Add([uint32]$code)
    } else {
      Throw-VmwareInputFailure -Category 'host-input blocker' -Detail "unsupported VNC keysym for character code $code"
    }
  }
  return $keys
}

function Invoke-VncKeystrokes {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text
  )

  $deadline = (Get-Date).AddSeconds(20)
  $client = $null
  do {
    try {
      $client = [System.Net.Sockets.TcpClient]::new()
      $connect = $client.BeginConnect($vncHost, $vncPort, $null, $null)
      if ($connect.AsyncWaitHandle.WaitOne(1000)) {
        $client.EndConnect($connect)
        break
      }
      $client.Close()
      $client = $null
    }
    catch {
      if ($null -ne $client) {
        $client.Close()
        $client = $null
      }
    }
    Start-Sleep -Milliseconds 250
  } while ((Get-Date) -lt $deadline)

  if ($null -eq $client -or -not $client.Connected) {
    Throw-VmwareInputFailure -Category 'host-input blocker' -Detail "VNC console did not open on $vncHost`:$vncPort"
  }

  try {
    $stream = $client.GetStream()
    $stream.ReadTimeout = 5000
    $stream.WriteTimeout = 5000
    $protocol = Read-ExactBytes -Stream $stream -Count 12
    $stream.Write($protocol, 0, $protocol.Length)
    $countBytes = Read-ExactBytes -Stream $stream -Count 1
    $typeCount = [int]$countBytes[0]
    if ($typeCount -eq 0) {
      $reasonLenBytes = Read-ExactBytes -Stream $stream -Count 4
      $reasonLen = [int](Read-U32BE -Bytes $reasonLenBytes -Offset 0)
      $reason = ''
      if ($reasonLen -gt 0 -and $reasonLen -lt 4096) {
        $reason = [System.Text.Encoding]::ASCII.GetString((Read-ExactBytes -Stream $stream -Count $reasonLen))
      }
      Throw-VmwareInputFailure -Category 'host-input blocker' -Detail "VNC security rejected: $reason"
    }
    $types = Read-ExactBytes -Stream $stream -Count $typeCount
    if ($types -contains 1) {
      $securityType = [byte[]]@(1)
      $stream.Write($securityType, 0, 1)
    } elseif ($types -contains 2) {
      $securityType = [byte[]]@(2)
      $stream.Write($securityType, 0, 1)
      Invoke-VncAuth -Stream $stream
    } else {
      $offered = ($types | ForEach-Object { $_.ToString() }) -join ','
      Throw-VmwareInputFailure -Category 'host-input blocker' -Detail "VNC console did not offer supported security type; offered=$offered"
    }
    $result = Read-ExactBytes -Stream $stream -Count 4
    if ((Read-U32BE -Bytes $result -Offset 0) -ne 0) {
      Throw-VmwareInputFailure -Category 'host-input blocker' -Detail 'VNC security handshake failed'
    }
    $shared = [byte[]]@(1)
    $stream.Write($shared, 0, 1)
    $serverInit = Read-ExactBytes -Stream $stream -Count 24
    $nameLen = [int](Read-U32BE -Bytes $serverInit -Offset 20)
    if ($nameLen -gt 0 -and $nameLen -lt 4096) {
      [void](Read-ExactBytes -Stream $stream -Count $nameLen)
    }

    foreach ($keysym in (ConvertTo-Keysyms -Text $Text)) {
      Write-VncKeyEvent -Stream $stream -Keysym $keysym -Down $true
      Start-Sleep -Milliseconds 30
      Write-VncKeyEvent -Stream $stream -Keysym $keysym -Down $false
      Start-Sleep -Milliseconds 30
    }
  }
  finally {
    $client.Close()
  }
}

function Invoke-GuestKeystrokes {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text
  )

  Invoke-VncKeystrokes -Text $Text
}

if (-not (Test-Path $vmxTemplatePath)) {
  Throw-VmwareInputFailure -Category 'host-start failure' -Detail "missing VMX template: $vmxTemplatePath"
}
if (-not (Test-Path $vmrun)) {
  Throw-VmwareInputFailure -Category 'host-start failure' -Detail "missing vmrun.exe: $vmrun"
}
if (-not (Test-Path $sourceDiskPath)) {
  Throw-VmwareInputFailure -Category 'host-start failure' -Detail "missing source disk: $sourceDiskPath"
}
if (-not $qemuImg) {
  Throw-VmwareInputFailure -Category 'host-start failure' -Detail 'missing qemu-img.exe'
}

Initialize-InputDisk
New-InputCheckVmx

Invoke-VmrunOptional -VmrunArgs @('stop', $vmxPath, 'hard')
Start-Sleep -Seconds 2
Invoke-VmrunChecked -VmrunArgs @('start', $vmxPath, 'nogui') -Failure 'host-start failure'

try {
  $bootText = Wait-ForLog -Required @(
    '[BOOT] dawn online',
    '[DEVICE] input path kbd=',
    '[DEVICE] input select basis=',
    'usb-hid=not-bound',
    '[USER] shell ready',
    '[USER] input lane ready',
    'first-setup required: no hostname or user configured'
  ) -TimeoutSeconds 60

  if ($bootText -notmatch '\[DEVICE\] input path kbd=(virtio-kbd|i8042-kbd)') {
    Throw-VmwareInputFailure -Category 'guest failure' -Detail 'missing keyboard path selection'
  }
  if ($bootText -notmatch '\[DEVICE\] input select basis=(virtio-kbd|i8042)') {
    Throw-VmwareInputFailure -Category 'guest failure' -Detail 'missing keyboard selection basis'
  }

  Invoke-GuestKeystrokes -Text "help`r"
  $serial = Wait-ForOrderedPatterns -Patterns @(
    '\[DEVICE\] input event src=(virtio-kbd|i8042-kbd) key=68',
    '\[USER\] input lane src=keyboard key=68',
    '\[USER\] shell accept src=keyboard cmd=help',
    '\[USER\] shell execute src=keyboard cmd=help'
  ) -TimeoutSeconds 25 -Description 'DEVICE keyboard help -> USER keyboard command'

  Invoke-VmrunOptional -VmrunArgs @('captureScreen', $vmxPath, $screenPath)
  Copy-LatestVmwareLog
}
finally {
  Invoke-VmrunOptional -VmrunArgs @('stop', $vmxPath, 'hard')
  Start-Sleep -Seconds 2
}

$serial = Read-SerialLog
Assert-NoForbidden -Text $serial
$required = @(
  '[USER] input lane src=keyboard key=68',
  '[USER] shell accept src=keyboard cmd=help',
  '[USER] shell execute src=keyboard cmd=help',
  'core: setup.status'
)
foreach ($needle in $required) {
  if (-not $serial.Contains($needle)) {
    Throw-VmwareInputFailure -Category 'guest failure' -Detail "missing expected output: $needle"
  }
}
if ($serial -notmatch '\[DEVICE\] input event src=(virtio-kbd|i8042-kbd) key=68') {
  Throw-VmwareInputFailure -Category 'provenance failure' -Detail 'missing DEVICE real keyboard event for help'
}

Get-Content $serialPath -Encoding utf8
