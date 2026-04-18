[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$SessionDir,
  [string]$LogPath
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
$verdictText = & python $renderVerdictScript $capturedCopyPath
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
$referenceName = if ($baselinePath) { Split-Path -Leaf $baselinePath } else { '(missing)' }

@(
  "log=$(Split-Path -Leaf $capturedCopyPath)"
  "reference=$referenceName"
  "firmware=$firmware"
  "split_layer=$splitLayer"
  "priority_blocker=$priorityBlocker"
  "driver_family_delta=$driverFamilyDelta"
  "generated_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
) | Set-Content -Encoding ascii $metaPath

Write-Host "Bring-up firsthop summary written: $summaryPath"
