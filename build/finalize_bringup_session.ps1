[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$SessionDir,
  [string]$LogPath,
  [ValidateSet('auto', 'physical-candidate', 'virtualized-prephysical', 'unknown')]
  [string]$EvidenceScope = 'auto'
)

$ErrorActionPreference = 'Stop'

function Resolve-OptionalPath {
  param([string]$BaseDir, [string]$Candidate)
  if ([string]::IsNullOrWhiteSpace($Candidate)) {
    return $null
  }
  if ([System.IO.Path]::IsPathRooted($Candidate)) {
    return [System.IO.Path]::GetFullPath($Candidate)
  }
  return [System.IO.Path]::GetFullPath((Join-Path $BaseDir $Candidate))
}

function Find-SessionLog {
  param([string]$SessionPath)
  $patterns = @(
    'serial-capture.log',
    '*.serial.log',
    'serial.log',
    'vmware*.log',
    'qemu*.log'
  )
  foreach ($pattern in $patterns) {
    $match = Get-ChildItem -Path $SessionPath -File -Filter $pattern -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTimeUtc -Descending |
      Select-Object -First 1
    if ($match) {
      return $match.FullName
    }
  }
  return $null
}

function Get-KeyValueLine {
  param(
    [object[]]$Lines,
    [string]$Key
  )
  $prefix = "$Key="
  foreach ($line in $Lines) {
    if ($null -ne $line -and $line.ToString().StartsWith($prefix)) {
      return $line.ToString().Substring($prefix.Length)
    }
  }
  return $null
}

function Get-KeyValueMap {
  param([object[]]$Lines)
  $values = @{}
  foreach ($line in $Lines) {
    if ($null -eq $line) {
      continue
    }
    $text = $line.ToString().Trim()
    $index = $text.IndexOf('=')
    if ($index -lt 0) {
      continue
    }
    $key = $text.Substring(0, $index).Trim()
    $value = $text.Substring($index + 1).Trim()
    if (-not [string]::IsNullOrWhiteSpace($key)) {
      $values[$key] = $value
    }
  }
  return $values
}

function Get-FileSha256Lower {
  param([string]$Path)
  if (-not (Test-Path $Path -PathType Leaf)) {
    return ''
  }
  return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Get-FirstLogLine {
  param(
    [object[]]$Lines,
    [string]$Needle
  )
  foreach ($line in $Lines) {
    if ($null -ne $line -and $line.ToString().Contains($Needle)) {
      return $line.ToString()
    }
  }
  return ''
}

function Get-LineField {
  param(
    [string]$Line,
    [string]$Key
  )
  if ([string]::IsNullOrWhiteSpace($Line)) {
    return ''
  }
  $match = [regex]::Match($Line, "(^| )$([regex]::Escape($Key))=([^ ]+)")
  if ($match.Success) {
    return $match.Groups[2].Value
  }
  return ''
}

function Get-PhysicalNetworkManifestValues {
  param(
    [string]$SessionPath,
    [string]$CapturedLogPath
  )

  $logLines = @(Get-Content -Path $CapturedLogPath -ErrorAction Stop)
  $netPathLine = Get-FirstLogLine -Lines $logLines -Needle '[DEVICE] net path'
  $netPciLine = Get-FirstLogLine -Lines $logLines -Needle '[DEVICE] net pci '
  $netDriverLine = Get-FirstLogLine -Lines $logLines -Needle '[DEVICE] net driver '
  $netLinkLine = Get-FirstLogLine -Lines $logLines -Needle '[DEVICE] net link '
  $netMacLine = Get-FirstLogLine -Lines $logLines -Needle '[DEVICE] net mac='
  $pciPresent = -not [string]::IsNullOrWhiteSpace($netPciLine) -and $netPciLine.Contains(' vendor=')
  $vendor = Get-LineField -Line $netPciLine -Key 'vendor'
  $device = Get-LineField -Line $netPciLine -Key 'device'
  $class = Get-LineField -Line $netPciLine -Key 'class'
  $family = Get-LineField -Line $netDriverLine -Key 'family'
  if ([string]::IsNullOrWhiteSpace($family)) {
    $driver = Get-LineField -Line $netPathLine -Key 'driver'
    if ($driver -eq 'e1000' -or $driver -eq 'e1000-stub') {
      $family = 'e1000'
    } elseif ($driver -eq 'e1000e' -or $driver -eq 'e1000e-stub') {
      $family = 'e1000e'
    } elseif ($pciPresent) {
      $family = 'unsupported'
    } else {
      $family = 'none'
    }
  }
  $bind = Get-LineField -Line $netDriverLine -Key 'bind'
  if ([string]::IsNullOrWhiteSpace($bind)) {
    if ($netPathLine.Contains('lane=ready')) {
      $bind = 'ready'
    } elseif ($pciPresent -and ($family -eq 'e1000' -or $family -eq 'e1000e')) {
      $bind = 'degraded'
    } else {
      $bind = 'missing'
    }
  }
  $linkState = Get-LineField -Line $netLinkLine -Key 'state'
  if ([string]::IsNullOrWhiteSpace($linkState)) {
    $linkState = 'unknown'
  }
  $mac = Get-LineField -Line $netMacLine -Key 'mac'
  if ([string]::IsNullOrWhiteSpace($mac)) {
    $mac = '00:00:00:00:00:00'
  }
  $mode = Get-LineField -Line $netLinkLine -Key 'mode'
  $sessionText = $SessionPath.Replace('\', '/').ToLowerInvariant()
  if ($sessionText.Contains('/artifacts/physical_bringup/') -and -not (Test-VirtualizedLogPath -CapturedLogPath $CapturedLogPath) -and $pciPresent) {
    $mode = 'physical-candidate'
  } elseif ([string]::IsNullOrWhiteSpace($mode)) {
    if (-not $pciPresent) {
      $mode = 'soft-loop'
    } elseif ($bind -eq 'ready') {
      $mode = 'external'
    } else {
      $mode = 'offline'
    }
  }
  $blocker = Get-LineField -Line $netDriverLine -Key 'blocker'
  if ([string]::IsNullOrWhiteSpace($blocker)) {
    if (-not $pciPresent) {
      $blocker = 'nic-not-present'
    } elseif ($family -eq 'unsupported') {
      $blocker = 'unsupported-nic-family'
    } elseif ($bind -eq 'denied') {
      $blocker = 'driver-bind-denied'
    } elseif ($bind -ne 'ready') {
      $blocker = 'pci-mmio-map-failed'
    } elseif ($linkState -eq 'down') {
      $blocker = 'link-up-missing'
    } else {
      $blocker = 'none'
    }
  }
  $consequence = 'degraded'
  if ($blocker -eq 'none') {
    $consequence = 'continue'
  }
  return @{
    pci_nic_present = $(if ($pciPresent) { 'yes' } else { 'no' })
    nic_vendor_id = $(if ([string]::IsNullOrWhiteSpace($vendor)) { 'none' } else { $vendor })
    nic_device_id = $(if ([string]::IsNullOrWhiteSpace($device)) { 'none' } else { $device })
    nic_class = $(if ([string]::IsNullOrWhiteSpace($class)) { 'none' } else { $class })
    nic_driver_family = $family
    nic_bind = $bind
    link_state = $linkState
    mac_address = $mac
    network_mode = $mode
    runtime_consequence = $consequence
    blocker = $blocker
  }
}

function Write-PhysicalEvidenceManifest {
  param(
    [string]$SessionPath,
    [string]$CapturedLogPath
  )

  $sessionText = $SessionPath.Replace('\', '/').ToLowerInvariant()
  if (-not $sessionText.Contains('/artifacts/physical_bringup/')) {
    return
  }

  $manifestPath = Join-Path $SessionPath 'evidence-manifest.txt'
  $machinePath = Join-Path $SessionPath 'machine.txt'
  $notesPath = Join-Path $SessionPath 'operator-notes.txt'
  $shaPath = Join-Path $SessionPath 'sha256.txt'
  $network = Get-PhysicalNetworkManifestValues -SessionPath $SessionPath -CapturedLogPath $CapturedLogPath
  $lines = @(
    'schema=physical-evidence-v1'
    'target_support_cell=intel-x86_64+uefi+sata-ahci+gop+keyboard'
    'evidence_scope=physical-candidate'
    "capture_log=$([System.IO.Path]::GetFileName($CapturedLogPath))"
    "capture_log_sha256=$(Get-FileSha256Lower -Path $CapturedLogPath)"
    'operator_notes=operator-notes.txt'
    "operator_notes_sha256=$(Get-FileSha256Lower -Path $notesPath)"
    'machine=machine.txt'
    "machine_sha256=$(Get-FileSha256Lower -Path $machinePath)"
    "pci_nic_present=$($network.pci_nic_present)"
    "nic_vendor_id=$($network.nic_vendor_id)"
    "nic_device_id=$($network.nic_device_id)"
    "nic_class=$($network.nic_class)"
    "nic_driver_family=$($network.nic_driver_family)"
    "nic_bind=$($network.nic_bind)"
    "link_state=$($network.link_state)"
    "mac_address=$($network.mac_address)"
    "network_mode=$($network.network_mode)"
    "runtime_consequence=$($network.runtime_consequence)"
    "blocker=$($network.blocker)"
  )
  if (Test-Path $shaPath -PathType Leaf) {
    $lines += 'boot_artifacts_sha256=sha256.txt'
    $lines += "boot_artifacts_sha256_sha256=$(Get-FileSha256Lower -Path $shaPath)"
  }
  $lines | Set-Content -Encoding ascii $manifestPath
}

function Add-ManifestFileBlockers {
  param(
    [hashtable]$Values,
    [string]$SessionPath,
    [string]$FilenameKey,
    [string]$HashKey,
    [string]$ExpectedName,
    [System.Collections.Generic.List[string]]$Blockers
  )

  $filename = $Values[$FilenameKey]
  $filenameBlocker = $FilenameKey.Replace('_', '-')
  if ([string]::IsNullOrWhiteSpace($filename)) {
    $Blockers.Add("evidence-manifest-$filenameBlocker-missing")
    return
  }
  if ($filename -ne $ExpectedName) {
    $Blockers.Add("evidence-manifest-$filenameBlocker-mismatch")
  }
  if ($filename -ne [System.IO.Path]::GetFileName($filename)) {
    $Blockers.Add("evidence-manifest-$filenameBlocker-path-invalid")
    return
  }
  $filePath = Join-Path $SessionPath $filename
  if (-not (Test-Path $filePath -PathType Leaf)) {
    $Blockers.Add("evidence-manifest-$filenameBlocker-file-missing")
    return
  }
  $expectedHash = $Values[$HashKey]
  $hashBlocker = $HashKey.Replace('_', '-')
  if ([string]::IsNullOrWhiteSpace($expectedHash)) {
    $Blockers.Add("evidence-manifest-$hashBlocker-missing")
    return
  }
  $actualHash = Get-FileSha256Lower -Path $filePath
  if ($actualHash -ne $expectedHash.ToString().Trim().ToLowerInvariant()) {
    $Blockers.Add("evidence-manifest-$hashBlocker-mismatch")
  }
}

function Add-PhysicalManifestBlockers {
  param(
    [string]$SessionPath,
    [string]$CapturedLogPath,
    [System.Collections.Generic.List[string]]$Blockers
  )

  $manifestPath = Join-Path $SessionPath 'evidence-manifest.txt'
  if (-not (Test-Path $manifestPath -PathType Leaf)) {
    $Blockers.Add('evidence-manifest-missing')
    return
  }

  $manifestLines = @(Get-Content -Path $manifestPath -ErrorAction Stop)
  $manifestValues = Get-KeyValueMap -Lines $manifestLines
  $requiredKeys = @(
    'schema',
    'target_support_cell',
    'evidence_scope',
    'capture_log',
    'capture_log_sha256',
    'operator_notes',
    'operator_notes_sha256',
    'machine',
    'machine_sha256',
    'pci_nic_present',
    'nic_vendor_id',
    'nic_device_id',
    'nic_class',
    'nic_driver_family',
    'nic_bind',
    'link_state',
    'mac_address',
    'network_mode',
    'runtime_consequence',
    'blocker'
  )
  foreach ($key in $requiredKeys) {
    if (-not $manifestValues.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($manifestValues[$key])) {
      $Blockers.Add("evidence-manifest-$($key.Replace('_', '-'))-missing")
    }
  }
  if ($manifestValues.ContainsKey('schema') -and $manifestValues['schema'] -ne 'physical-evidence-v1') {
    $Blockers.Add('evidence-manifest-schema-invalid')
  }
  if ($manifestValues.ContainsKey('target_support_cell') -and $manifestValues['target_support_cell'] -ne 'intel-x86_64+uefi+sata-ahci+gop+keyboard') {
    $Blockers.Add('evidence-manifest-target-support-cell-mismatch')
  }
  if ($manifestValues.ContainsKey('evidence_scope') -and $manifestValues['evidence_scope'] -ne 'physical-candidate') {
    $Blockers.Add('evidence-manifest-evidence-scope-mismatch')
  }
  $enumRules = @{
    pci_nic_present = @('yes', 'no')
    nic_driver_family = @('e1000', 'e1000e', 'unsupported', 'none')
    nic_bind = @('ready', 'missing', 'denied', 'degraded')
    link_state = @('up', 'down', 'unknown')
    network_mode = @('offline', 'soft-loop', 'external', 'physical-candidate')
    runtime_consequence = @('continue', 'degraded', 'recovery', 'fatal')
  }
  foreach ($key in $enumRules.Keys) {
    if (-not $manifestValues.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($manifestValues[$key])) {
      continue
    }
    $value = $manifestValues[$key].ToString().Trim()
    if ($enumRules[$key] -notcontains $value) {
      $Blockers.Add("evidence-manifest-$($key.Replace('_', '-'))-invalid")
    }
  }

  Add-ManifestFileBlockers -Values $manifestValues -SessionPath $SessionPath -FilenameKey 'capture_log' -HashKey 'capture_log_sha256' -ExpectedName ([System.IO.Path]::GetFileName($CapturedLogPath)) -Blockers $Blockers
  Add-ManifestFileBlockers -Values $manifestValues -SessionPath $SessionPath -FilenameKey 'operator_notes' -HashKey 'operator_notes_sha256' -ExpectedName 'operator-notes.txt' -Blockers $Blockers
  Add-ManifestFileBlockers -Values $manifestValues -SessionPath $SessionPath -FilenameKey 'machine' -HashKey 'machine_sha256' -ExpectedName 'machine.txt' -Blockers $Blockers
}

function Add-PhysicalNoteBlockers {
  param(
    [string]$NotesPath,
    [System.Collections.Generic.List[string]]$Blockers
  )

  if (-not (Test-Path $NotesPath -PathType Leaf)) {
    $Blockers.Add('operator-notes-missing')
    return
  }

  $noteLines = @(Get-Content -Path $NotesPath -ErrorAction Stop)
  $noteValues = Get-KeyValueMap -Lines $noteLines
  $placeholderLines = @(
    'paste run notes here.',
    'include: machine model, firmware version, sata mode, usb port used, last visible line, shell-ready result.',
    'replace every value before finalization.'
  )
  foreach ($line in $noteLines) {
    if ($null -eq $line) {
      continue
    }
    $lower = $line.ToString().Trim().ToLowerInvariant()
    if ($placeholderLines -contains $lower) {
      $Blockers.Add('operator-notes-placeholder')
    }
  }

  $requiredNoteKeys = @(
    'machine_model',
    'firmware_version',
    'sata_mode',
    'usb_port',
    'capture_source',
    'last_visible_line',
    'shell_ready',
    'gop_result',
    'keyboard_result'
  )
  foreach ($key in $requiredNoteKeys) {
    if (-not $noteValues.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($noteValues[$key])) {
      $Blockers.Add("operator-note-$($key.Replace('_', '-'))-missing")
    }
  }

  $enumRules = @{
    capture_source = @('serial', 'display-photo', 'operator-transcript')
    shell_ready = @('yes', 'no', 'not-reached')
    gop_result = @('ready', 'missing', 'not-reached')
    keyboard_result = @('ready', 'blocked', 'not-reached')
  }
  foreach ($key in $enumRules.Keys) {
    if (-not $noteValues.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($noteValues[$key])) {
      continue
    }
    $value = $noteValues[$key].ToString().Trim().ToLowerInvariant()
    if ($enumRules[$key] -notcontains $value) {
      $Blockers.Add("operator-note-$($key.Replace('_', '-'))-invalid")
    }
  }
}

function Test-VirtualizedLogPath {
  param([string]$CapturedLogPath)
  $name = [System.IO.Path]::GetFileName($CapturedLogPath).ToLowerInvariant()
  $pathText = $CapturedLogPath.Replace('\', '/').ToLowerInvariant()
  return $name.Contains('qemu') -or $name.Contains('vmware') -or $pathText.Contains('/artifacts/vmware_bringup/')
}

function Get-PhysicalEvidenceState {
  param(
    [string]$SessionPath,
    [string]$CapturedLogPath
  )

  $sessionText = $SessionPath.Replace('\', '/').ToLowerInvariant()
  if (-not $sessionText.Contains('/artifacts/physical_bringup/')) {
    return @{
      Status = 'not-applicable'
      Blockers = 'not-applicable'
    }
  }

  $blockers = New-Object System.Collections.Generic.List[string]
  if (Test-VirtualizedLogPath -CapturedLogPath $CapturedLogPath) {
    $blockers.Add('virtualized-log-name')
  }

  $machinePath = Join-Path $SessionPath 'machine.txt'
  if (-not (Test-Path $machinePath -PathType Leaf)) {
    $blockers.Add('machine-metadata-missing')
  } else {
    $machineLines = @(Get-Content -Path $machinePath -ErrorAction Stop)
    $targetCell = Get-KeyValueLine -Lines $machineLines -Key 'target_support_cell'
    $scope = Get-KeyValueLine -Lines $machineLines -Key 'evidence_scope'
    if ($targetCell -ne 'intel-x86_64+uefi+sata-ahci+gop+keyboard') {
      $blockers.Add('target-cell-metadata-missing')
    }
    if ($scope -ne 'physical-candidate') {
      $blockers.Add('physical-scope-metadata-missing')
    }
  }

  $notesPath = Join-Path $SessionPath 'operator-notes.txt'
  Add-PhysicalNoteBlockers -NotesPath $notesPath -Blockers $blockers
  Add-PhysicalManifestBlockers -SessionPath $SessionPath -CapturedLogPath $CapturedLogPath -Blockers $blockers

  if ($blockers.Count -eq 0) {
    return @{
      Status = 'present'
      Blockers = 'none'
    }
  }
  return @{
    Status = 'missing'
    Blockers = ($blockers -join ',')
  }
}

function Resolve-EvidenceScope {
  param(
    [string]$SessionPath,
    [string]$CapturedLogPath,
    [string]$RequestedScope,
    [hashtable]$PhysicalEvidence
  )
  if ($RequestedScope -ne 'auto') {
    if ($RequestedScope -eq 'physical-candidate' -and $PhysicalEvidence.Status -ne 'present') {
      return 'unknown'
    }
    return $RequestedScope
  }
  $sessionText = $SessionPath.Replace('\', '/').ToLowerInvariant()
  if ((Test-VirtualizedLogPath -CapturedLogPath $CapturedLogPath) -or $sessionText.Contains('/artifacts/vmware_bringup/')) {
    return 'virtualized-prephysical'
  }
  if ($sessionText.Contains('/artifacts/physical_bringup/') -and $PhysicalEvidence.Status -eq 'present') {
    return 'physical-candidate'
  }
  return 'unknown'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$sessionPath = Resolve-OptionalPath -BaseDir $repoRoot -Candidate $SessionDir
if (-not (Test-Path $sessionPath -PathType Container)) {
  throw "session directory not found: $sessionPath"
}

$resolvedLogPath = Resolve-OptionalPath -BaseDir $sessionPath -Candidate $LogPath
if (-not $resolvedLogPath) {
  $resolvedLogPath = Find-SessionLog -SessionPath $sessionPath
}
if (-not $resolvedLogPath -or -not (Test-Path $resolvedLogPath -PathType Leaf)) {
  throw "bring-up log not found. Pass -LogPath or place serial-capture.log / *.serial.log in $sessionPath"
}

$summaryScript = Join-Path $PSScriptRoot 'summarize_firsthop.py'
if (-not (Test-Path $summaryScript -PathType Leaf)) {
  throw "missing firsthop summarizer: $summaryScript"
}
$classificationScript = Join-Path $PSScriptRoot 'classify_firsthop.py'
if (-not (Test-Path $classificationScript -PathType Leaf)) {
  throw "missing firsthop classifier: $classificationScript"
}
$selectBaselineScript = Join-Path $PSScriptRoot 'select_firsthop_baseline.py'
if (-not (Test-Path $selectBaselineScript -PathType Leaf)) {
  throw "missing firsthop baseline selector: $selectBaselineScript"
}
$renderVerdictScript = Join-Path $PSScriptRoot 'render_firsthop_verdict.py'
if (-not (Test-Path $renderVerdictScript -PathType Leaf)) {
  throw "missing firsthop verdict renderer: $renderVerdictScript"
}
$compareScript = Join-Path $PSScriptRoot 'compare_firsthop.py'
if (-not (Test-Path $compareScript -PathType Leaf)) {
  throw "missing firsthop compare: $compareScript"
}

$capturedName = [System.IO.Path]::GetFileName($resolvedLogPath)
$capturedCopyPath = Join-Path $sessionPath $capturedName
if ([System.IO.Path]::GetFullPath($resolvedLogPath) -ne [System.IO.Path]::GetFullPath($capturedCopyPath)) {
  Copy-Item $resolvedLogPath $capturedCopyPath -Force
}
Write-PhysicalEvidenceManifest -SessionPath $sessionPath -CapturedLogPath $capturedCopyPath
$physicalEvidence = Get-PhysicalEvidenceState -SessionPath $sessionPath -CapturedLogPath $capturedCopyPath
$resolvedEvidenceScope = Resolve-EvidenceScope -SessionPath $sessionPath -CapturedLogPath $capturedCopyPath -RequestedScope $EvidenceScope -PhysicalEvidence $physicalEvidence

$summaryText = & python $summaryScript $capturedCopyPath
if ($LASTEXITCODE -ne 0) {
  throw "firsthop summary failed for $capturedCopyPath"
}
$classificationText = & python $classificationScript $capturedCopyPath
if ($LASTEXITCODE -ne 0) {
  throw "firsthop classification failed for $capturedCopyPath"
}

$summaryPath = Join-Path $sessionPath 'firsthop-summary.txt'
$classificationPath = Join-Path $sessionPath 'firsthop-classification.txt'
$referencePath = Join-Path $sessionPath 'firsthop-reference.txt'
$deltaPath = Join-Path $sessionPath 'firsthop-delta.txt'
$verdictPath = Join-Path $sessionPath 'firsthop-verdict.txt'
$metaPath = Join-Path $sessionPath 'firsthop-log.txt'

$summaryText | Set-Content -Encoding ascii $summaryPath
$classificationText | Set-Content -Encoding ascii $classificationPath

$baselineSelectText = & python $selectBaselineScript $capturedCopyPath
if ($LASTEXITCODE -ne 0) {
  throw "firsthop baseline selection failed for $capturedCopyPath"
}
$baselineSelectText | Set-Content -Encoding ascii $referencePath
$verdictText = & python $renderVerdictScript --evidence-scope $resolvedEvidenceScope $capturedCopyPath
if ($LASTEXITCODE -ne 0) {
  throw "firsthop verdict render failed for $capturedCopyPath"
}
$verdictText | Set-Content -Encoding ascii $verdictPath
$baselinePath = Get-KeyValueLine -Lines $baselineSelectText -Key 'selected_path'
if ($baselinePath -and (Test-Path $baselinePath -PathType Leaf)) {
  $deltaText = & python $compareScript $baselinePath $capturedCopyPath
  if ($LASTEXITCODE -ne 0) {
    throw "firsthop delta compare failed for $capturedCopyPath"
  }
  $deltaText | Set-Content -Encoding ascii $deltaPath
} else {
  $deltaText = @(
    "reference=(missing)"
    "actual=$(Split-Path -Leaf $capturedCopyPath)"
    'status=baseline-unavailable'
    'delta_layers=unknown'
    'priority_blocker=unknown'
  ) -join [Environment]::NewLine
  @(
    "reference=(missing)"
    "actual=$(Split-Path -Leaf $capturedCopyPath)"
    'status=baseline-unavailable'
  ) | Set-Content -Encoding ascii $deltaPath
}

$firmware = Get-KeyValueLine -Lines $classificationText -Key 'firmware'
if (-not $firmware) {
  $firmware = 'unknown'
}
$splitLayer = Get-KeyValueLine -Lines $classificationText -Key 'split_layer'
if (-not $splitLayer) {
  $splitLayer = 'unknown'
}
$priorityBlocker = Get-KeyValueLine -Lines $deltaText -Key 'priority_blocker'
if (-not $priorityBlocker) {
  $priorityBlocker = 'unknown'
}
$driverFamilyDelta = Get-KeyValueLine -Lines $deltaText -Key 'driver_family_delta'
if (-not $driverFamilyDelta) {
  $driverFamilyDelta = 'unknown'
}
$supportCellStatus = Get-KeyValueLine -Lines $verdictText -Key 'support_cell_status'
if (-not $supportCellStatus) {
  $supportCellStatus = 'unknown'
}
$supportCellRuntimeGate = Get-KeyValueLine -Lines $verdictText -Key 'support_cell_runtime_gate'
if (-not $supportCellRuntimeGate) {
  $supportCellRuntimeGate = 'unknown'
}
$supportCellBlockers = Get-KeyValueLine -Lines $verdictText -Key 'support_cell_blockers'
if (-not $supportCellBlockers) {
  $supportCellBlockers = 'unknown'
}
$runtimeConsequence = Get-KeyValueLine -Lines $verdictText -Key 'runtime_consequence'
if (-not $runtimeConsequence) {
  $runtimeConsequence = 'unknown'
}
$physicalEvidenceStatus = Get-KeyValueLine -Lines $verdictText -Key 'physical_evidence_status'
if (-not $physicalEvidenceStatus) {
  $physicalEvidenceStatus = $physicalEvidence.Status
}
$physicalEvidenceBlockers = Get-KeyValueLine -Lines $verdictText -Key 'physical_evidence_blockers'
if (-not $physicalEvidenceBlockers) {
  $physicalEvidenceBlockers = $physicalEvidence.Blockers
}
$usbHidBindState = Get-KeyValueLine -Lines $verdictText -Key 'usb_hid_bind_state'
if (-not $usbHidBindState) {
  $usbHidBindState = 'unknown'
}
$usbHidBlocker = Get-KeyValueLine -Lines $verdictText -Key 'usb_hid_blocker'
if (-not $usbHidBlocker) {
  $usbHidBlocker = 'unknown'
}
$pciNicPresent = Get-KeyValueLine -Lines $verdictText -Key 'pci_nic_present'
if (-not $pciNicPresent) {
  $pciNicPresent = 'unknown'
}
$nicDriverFamily = Get-KeyValueLine -Lines $verdictText -Key 'nic_driver_family'
if (-not $nicDriverFamily) {
  $nicDriverFamily = 'unknown'
}
$nicBind = Get-KeyValueLine -Lines $verdictText -Key 'nic_bind'
if (-not $nicBind) {
  $nicBind = 'unknown'
}
$linkState = Get-KeyValueLine -Lines $verdictText -Key 'link_state'
if (-not $linkState) {
  $linkState = 'unknown'
}
$networkMode = Get-KeyValueLine -Lines $verdictText -Key 'network_mode'
if (-not $networkMode) {
  $networkMode = 'unknown'
}
$networkBlocker = Get-KeyValueLine -Lines $verdictText -Key 'network_blocker'
if (-not $networkBlocker) {
  $networkBlocker = 'unknown'
}
$referenceName = if ($baselinePath) { Split-Path -Leaf $baselinePath } else { '(missing)' }

@(
  "log=$(Split-Path -Leaf $capturedCopyPath)"
  "reference=$referenceName"
  "evidence_scope=$resolvedEvidenceScope"
  "physical_evidence_status=$physicalEvidenceStatus"
  "physical_evidence_blockers=$physicalEvidenceBlockers"
  "usb_hid_bind_state=$usbHidBindState"
  "usb_hid_blocker=$usbHidBlocker"
  "pci_nic_present=$pciNicPresent"
  "nic_driver_family=$nicDriverFamily"
  "nic_bind=$nicBind"
  "link_state=$linkState"
  "network_mode=$networkMode"
  "network_blocker=$networkBlocker"
  "firmware=$firmware"
  "split_layer=$splitLayer"
  "priority_blocker=$priorityBlocker"
  "driver_family_delta=$driverFamilyDelta"
  "support_cell_status=$supportCellStatus"
  "support_cell_runtime_gate=$supportCellRuntimeGate"
  "support_cell_blockers=$supportCellBlockers"
  "runtime_consequence=$runtimeConsequence"
  "generated_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
) | Set-Content -Encoding ascii $metaPath

Write-Host "Bring-up firsthop summary written: $summaryPath"
