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
$physicalEvidenceStatus = Get-KeyValueLine -Lines $verdictText -Key 'physical_evidence_status'
if (-not $physicalEvidenceStatus) {
  $physicalEvidenceStatus = $physicalEvidence.Status
}
$physicalEvidenceBlockers = Get-KeyValueLine -Lines $verdictText -Key 'physical_evidence_blockers'
if (-not $physicalEvidenceBlockers) {
  $physicalEvidenceBlockers = $physicalEvidence.Blockers
}
$referenceName = if ($baselinePath) { Split-Path -Leaf $baselinePath } else { '(missing)' }

@(
  "log=$(Split-Path -Leaf $capturedCopyPath)"
  "reference=$referenceName"
  "evidence_scope=$resolvedEvidenceScope"
  "physical_evidence_status=$physicalEvidenceStatus"
  "physical_evidence_blockers=$physicalEvidenceBlockers"
  "firmware=$firmware"
  "split_layer=$splitLayer"
  "priority_blocker=$priorityBlocker"
  "driver_family_delta=$driverFamilyDelta"
  "support_cell_status=$supportCellStatus"
  "support_cell_runtime_gate=$supportCellRuntimeGate"
  "support_cell_blockers=$supportCellBlockers"
  "generated_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
) | Set-Content -Encoding ascii $metaPath

Write-Host "Bring-up firsthop summary written: $summaryPath"
