[CmdletBinding()]
param(
  [string]$ImagePath = (Join-Path $PSScriptRoot 'lunaos_disk.img'),
  [int]$DiskNumber = -1,
  [switch]$List,
  [switch]$Force,
  [switch]$SkipVerify
)

$ErrorActionPreference = 'Stop'

function Test-LunaAdministrator {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Format-LunaSize {
  param([UInt64]$Bytes)

  $units = @('B', 'KiB', 'MiB', 'GiB', 'TiB')
  $value = [double]$Bytes
  $unitIndex = 0
  while ($value -ge 1024 -and $unitIndex -lt ($units.Count - 1)) {
    $value /= 1024
    $unitIndex += 1
  }
  return ('{0:N2} {1}' -f $value, $units[$unitIndex])
}

function Get-LunaDiskRecords {
  $records = @()

  try {
    $records = Get-Disk | ForEach-Object {
      [PSCustomObject]@{
        Number = [int]$_.Number
        FriendlyName = [string]$_.FriendlyName
        BusType = [string]$_.BusType
        PartitionStyle = [string]$_.PartitionStyle
        Size = [UInt64]$_.Size
        IsBoot = [bool]$_.IsBoot
        IsSystem = [bool]$_.IsSystem
        IsOffline = [bool]$_.IsOffline
        IsReadOnly = [bool]$_.IsReadOnly
        Path = "\\.\PhysicalDrive$($_.Number)"
        Source = 'Get-Disk'
      }
    }
  } catch {
    $records = @()
  }

  if ($records.Count -gt 0) {
    return $records
  }

  try {
    $records = Get-CimInstance Win32_DiskDrive | ForEach-Object {
      $index = [int]$_.Index
      [PSCustomObject]@{
        Number = $index
        FriendlyName = [string]$_.Model
        BusType = [string]$_.InterfaceType
        PartitionStyle = '?'
        Size = [UInt64]$_.Size
        IsBoot = $false
        IsSystem = $false
        IsOffline = $false
        IsReadOnly = $false
        Path = "\\.\PhysicalDrive$index"
        Source = 'Win32_DiskDrive'
      }
    }
  } catch {
    $records = @()
  }

  return $records
}

function Show-LunaDisks {
  param([object[]]$Records)

  if (-not $Records -or $Records.Count -eq 0) {
    Write-Host 'No disk inventory available in this session.'
    return
  }

  $Records |
    Sort-Object Number |
    Select-Object `
      Number,
      FriendlyName,
      BusType,
      PartitionStyle,
      @{ Name = 'Size'; Expression = { Format-LunaSize $_.Size } },
      IsBoot,
      IsSystem,
      IsOffline,
      IsReadOnly |
    Format-Table -AutoSize
}

function Update-LunaHash {
  param(
    [Parameter(Mandatory = $true)]$Hash,
    [byte[]]$Buffer,
    [int]$Count
  )

  if ($Count -le 0) {
    return
  }
  [void]$Hash.TransformBlock($Buffer, 0, $Count, $null, 0)
}

function Complete-LunaHash {
  param([Parameter(Mandatory = $true)]$Hash)

  [void]$Hash.TransformFinalBlock([byte[]]::new(0), 0, 0)
  return ([BitConverter]::ToString($Hash.Hash)).Replace('-', '').ToLowerInvariant()
}

function Get-LunaFileHashPartial {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [UInt64]$Length = 0
  )

  $stream = [System.IO.File]::OpenRead($Path)
  try {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
      $buffer = [byte[]]::new(4MB)
      [UInt64]$remaining = if ($Length -gt 0) { $Length } else { [UInt64]$stream.Length }
      while ($remaining -gt 0) {
        $chunk = [Math]::Min([UInt64]$buffer.Length, $remaining)
        $read = $stream.Read($buffer, 0, [int]$chunk)
        if ($read -le 0) {
          throw "Unexpected end of file while hashing $Path."
        }
        Update-LunaHash -Hash $sha -Buffer $buffer -Count $read
        $remaining -= [UInt64]$read
      }
      return Complete-LunaHash -Hash $sha
    } finally {
      $sha.Dispose()
    }
  } finally {
    $stream.Dispose()
  }
}

function Get-LunaDeviceHashPartial {
  param(
    [Parameter(Mandatory = $true)][string]$DevicePath,
    [Parameter(Mandatory = $true)][UInt64]$Length
  )

  $stream = [System.IO.File]::Open($DevicePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
      $buffer = [byte[]]::new(4MB)
      [UInt64]$remaining = $Length
      while ($remaining -gt 0) {
        $chunk = [Math]::Min([UInt64]$buffer.Length, $remaining)
        $read = $stream.Read($buffer, 0, [int]$chunk)
        if ($read -le 0) {
          throw "Unexpected end of device while hashing $DevicePath."
        }
        Update-LunaHash -Hash $sha -Buffer $buffer -Count $read
        $remaining -= [UInt64]$read
      }
      return Complete-LunaHash -Hash $sha
    } finally {
      $sha.Dispose()
    }
  } finally {
    $stream.Dispose()
  }
}

if (-not (Test-Path $ImagePath)) {
  throw "Image not found: $ImagePath"
}

if (-not (Test-LunaAdministrator)) {
  throw 'Run this script from an elevated PowerShell session.'
}

$records = Get-LunaDiskRecords
if ($List) {
  Show-LunaDisks -Records $records
  return
}

if ($DiskNumber -lt 0) {
  Show-LunaDisks -Records $records
  throw 'Specify -DiskNumber N after confirming the target device.'
}

$record = $records | Where-Object { $_.Number -eq $DiskNumber } | Select-Object -First 1
if (-not $record) {
  throw "Disk $DiskNumber was not found."
}

$imageItem = Get-Item $ImagePath
$imageBytes = [UInt64]$imageItem.Length
if ($record.Size -gt 0 -and $record.Size -lt $imageBytes) {
  throw "Target disk is smaller than the image. Image=$(Format-LunaSize $imageBytes), Disk=$(Format-LunaSize $record.Size)"
}

$busType = [string]$record.BusType
if (-not $Force) {
  if ($record.IsBoot -or $record.IsSystem) {
    throw 'Refusing to write a boot/system disk without -Force.'
  }
  if ($busType -and $busType -notmatch 'USB|SD|MMC') {
    throw "Refusing non-removable bus type '$busType' without -Force."
  }
}

$diskCmdletsAvailable = $record.Source -eq 'Get-Disk'
$devicePath = "\\.\PhysicalDrive$DiskNumber"
$tookOffline = $false

try {
  if ($diskCmdletsAvailable) {
    Set-Disk -Number $DiskNumber -IsReadOnly $false | Out-Null
    $latest = Get-Disk -Number $DiskNumber
    if (-not $latest.IsOffline) {
      Set-Disk -Number $DiskNumber -IsOffline $true | Out-Null
      $tookOffline = $true
    }
  }

  $imageStream = [System.IO.File]::OpenRead($ImagePath)
  try {
    $deviceStream = [System.IO.File]::Open($devicePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    try {
      $buffer = [byte[]]::new(4MB)
      [UInt64]$written = 0
      while ($true) {
        $read = $imageStream.Read($buffer, 0, $buffer.Length)
        if ($read -le 0) {
          break
        }
        $deviceStream.Write($buffer, 0, $read)
        $written += [UInt64]$read
        $percent = [int](($written * 100) / $imageBytes)
        Write-Progress -Activity 'Writing LunaOS image' -Status "$percent% complete" -PercentComplete $percent
      }
      $deviceStream.Flush()
      Write-Progress -Activity 'Writing LunaOS image' -Completed
    } finally {
      $deviceStream.Dispose()
    }
  } finally {
    $imageStream.Dispose()
  }

  if (-not $SkipVerify) {
    Write-Host 'Verifying image...'
    $imageHash = Get-LunaFileHashPartial -Path $ImagePath -Length $imageBytes
    $deviceHash = Get-LunaDeviceHashPartial -DevicePath $devicePath -Length $imageBytes
    if ($imageHash -ne $deviceHash) {
      throw "Verification failed. Image hash=$imageHash Device hash=$deviceHash"
    }
  }
} finally {
  if ($diskCmdletsAvailable -and $tookOffline) {
    Set-Disk -Number $DiskNumber -IsOffline $false | Out-Null
  }
}

Write-Host "LunaOS image written to $devicePath"
