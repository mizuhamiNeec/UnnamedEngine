param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [switch]$FailOnDeprecated,
    [switch]$OutputJson,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$SupportedEngineApi = @("1")
$SupportedGameApi = @("1")
$DeprecatedEngineApi = @("0")
$DeprecatedGameApi = @("0")
$EngineDeprecationNote = "engineApi '0' is deprecated and scheduled for removal after 2026-09-30."
$GameDeprecationNote = "gameApi '0' is deprecated and scheduled for removal after 2026-09-30."

function Test-InList {
    param(
        [string[]]$Values,
        [string]$Target
    )

    foreach ($v in $Values) {
        if ($v -eq $Target) {
            return $true
        }
    }
    return $false
}

function Evaluate-Api {
    param(
        [string]$ApiLabel,
        [string]$Requested,
        [string[]]$Supported,
        [string[]]$Deprecated,
        [string]$DeprecationNote
    )

    if (Test-InList -Values $Supported -Target $Requested) {
        return [PSCustomObject]@{
            level = "supported"
            message = "$ApiLabel '$Requested' is supported."
        }
    }
    if (Test-InList -Values $Deprecated -Target $Requested) {
        return [PSCustomObject]@{
            level = "deprecated"
            message = "$ApiLabel '$Requested' is deprecated. $DeprecationNote"
        }
    }

    $supportedText = ($Supported -join ", ")
    $deprecatedText = ($Deprecated -join ", ")
    return [PSCustomObject]@{
        level = "unsupported"
        message = "$ApiLabel '$Requested' is unsupported. supported=[$supportedText] deprecated=[$deprecatedText]"
    }
}

if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
    throw "GameRoot directory does not exist: $GameRoot"
}
$resolvedGameRoot = (Resolve-Path $GameRoot).Path
$modsRoot = Join-Path $resolvedGameRoot "mods"
if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
    if (-not $Quiet) {
        Write-Host "No mods directory found: $modsRoot"
    }
    exit 0
}

$results = @()
$unsupportedCount = 0
$deprecatedCount = 0

foreach ($modDir in Get-ChildItem -LiteralPath $modsRoot -Directory) {
    $manifestPath = Join-Path $modDir.FullName "mod_manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        continue
    }

    $manifest = $null
    try {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    }
    catch {
        $unsupportedCount++
        $results += [PSCustomObject]@{
            modId = $modDir.Name
            manifestPath = $manifestPath
            status = "unsupported"
            engineApi = ""
            gameApi = ""
            message = "Failed to parse mod_manifest.json: $($_.Exception.Message)"
        }
        continue
    }

    $modId = $modDir.Name
    if ($manifest.PSObject.Properties.Match("id").Count -gt 0) {
        $modId = [string]$manifest.id
    }

    $engineApi = ""
    $gameApi = ""
    if ($manifest.PSObject.Properties.Match("engineApi").Count -gt 0) {
        $engineApi = [string]$manifest.engineApi
    }
    if ($manifest.PSObject.Properties.Match("gameApi").Count -gt 0) {
        $gameApi = [string]$manifest.gameApi
    }

    if ([string]::IsNullOrWhiteSpace($engineApi) -or [string]::IsNullOrWhiteSpace($gameApi)) {
        $unsupportedCount++
        $results += [PSCustomObject]@{
            modId = $modId
            manifestPath = $manifestPath
            status = "unsupported"
            engineApi = $engineApi
            gameApi = $gameApi
            message = "mod_manifest.json is missing required field(s): engineApi/gameApi"
        }
        continue
    }

    $engineResult = Evaluate-Api -ApiLabel "engineApi" -Requested $engineApi -Supported $SupportedEngineApi -Deprecated $DeprecatedEngineApi -DeprecationNote $EngineDeprecationNote
    $gameResult = Evaluate-Api -ApiLabel "gameApi" -Requested $gameApi -Supported $SupportedGameApi -Deprecated $DeprecatedGameApi -DeprecationNote $GameDeprecationNote

    $status = "supported"
    if ($engineResult.level -eq "unsupported" -or $gameResult.level -eq "unsupported") {
        $status = "unsupported"
        $unsupportedCount++
    }
    elseif ($engineResult.level -eq "deprecated" -or $gameResult.level -eq "deprecated") {
        $status = "deprecated"
        $deprecatedCount++
    }

    $results += [PSCustomObject]@{
        modId = $modId
        manifestPath = $manifestPath
        status = $status
        engineApi = $engineApi
        gameApi = $gameApi
        message = "$($engineResult.message) $($gameResult.message)"
    }
}

if ($OutputJson) {
    $payload = [PSCustomObject]@{
        gameRoot = $resolvedGameRoot
        supportedEngineApi = $SupportedEngineApi
        deprecatedEngineApi = $DeprecatedEngineApi
        supportedGameApi = $SupportedGameApi
        deprecatedGameApi = $DeprecatedGameApi
        unsupportedCount = $unsupportedCount
        deprecatedCount = $deprecatedCount
        results = $results
    }
    $payload | ConvertTo-Json -Depth 8
}
elseif (-not $Quiet) {
    Write-Host "API compatibility check result:"
    Write-Host "  GameRoot     : $resolvedGameRoot"
    Write-Host "  Unsupported  : $unsupportedCount"
    Write-Host "  Deprecated   : $deprecatedCount"
    foreach ($row in $results) {
        Write-Host "  - [$($row.status)] $($row.modId) (engineApi=$($row.engineApi), gameApi=$($row.gameApi))"
        Write-Host "      $($row.message)"
    }
}

if ($unsupportedCount -gt 0) {
    exit 1
}
if ($FailOnDeprecated -and $deprecatedCount -gt 0) {
    exit 2
}
exit 0
