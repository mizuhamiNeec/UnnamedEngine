param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [Parameter(Mandatory = $true)]
    [string]$ModId,
    [ValidateSet("minimal", "sample")]
    [string]$Template = "sample",
    [string]$Version = "1.0.0",
    [string]$EngineApi = "1",
    [string]$GameApi = "1",
    [string]$ContentRoot = "content",
    [string]$StartupScenePath = "scenes/bootstrap.json",
    [string]$UiOverridePath = "ui/main.ui.json",
    [string[]]$DependsOn = @(),
    [switch]$EnableInProfile,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Write-TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }
    Set-Content -Path $Path -Value $Content -Encoding UTF8
}

function Normalize-ModDirectoryName {
    param([string]$Value)
    $normalized = $Value.ToLowerInvariant()
    $normalized = $normalized -replace "[^a-z0-9._-]", "-"
    $normalized = $normalized.Trim("-")
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        throw "ModId '$Value' cannot be normalized to a valid directory name."
    }
    return $normalized
}

function Normalize-RelativeAssetPath {
    param([string]$Path)

    $trimmed = $Path.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        throw "Asset override path must not be empty."
    }

    if ([System.IO.Path]::IsPathRooted($trimmed)) {
        throw "Asset override path must be relative: '$trimmed'"
    }

    $normalized = $trimmed -replace "\\", "/"
    while ($normalized.StartsWith("./")) {
        $normalized = $normalized.Substring(2)
    }
    if ($normalized.StartsWith("../")) {
        throw "Asset override path must stay inside content root: '$trimmed'"
    }
    return $normalized
}

function Parse-DependencySpec {
    param([string]$Spec)

    $trimmed = $Spec.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        throw "DependsOn contains an empty dependency spec."
    }

    $pattern = "^(?<id>[A-Za-z0-9._-]+)(?<op>>=|<=|==|=|>|<)?(?<version>.*)$"
    $match = [regex]::Match($trimmed, $pattern)
    if (-not $match.Success) {
        throw "Invalid dependency spec '$trimmed'."
    }

    $dependencyId = $match.Groups["id"].Value
    $operator = $match.Groups["op"].Value
    $versionText = $match.Groups["version"].Value.Trim()

    $versionConstraint = ""
    if (-not [string]::IsNullOrWhiteSpace($operator)) {
        if ([string]::IsNullOrWhiteSpace($versionText)) {
            throw "Dependency '$trimmed' is missing version text."
        }
        $versionConstraint = "$operator$versionText"
    }

    return [PSCustomObject]@{
        id = $dependencyId
        version = $versionConstraint
    }
}

function Update-GameProfileEnabledMods {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProfilePath,
        [Parameter(Mandatory = $true)]
        [string]$TargetModId
    )

    if (-not (Test-Path -LiteralPath $ProfilePath -PathType Leaf)) {
        throw "game_profile.json was not found: $ProfilePath"
    }

    $profileRaw = Get-Content -Path $ProfilePath -Raw
    $profile = $profileRaw | ConvertFrom-Json
    if ($null -eq $profile) {
        throw "Failed to parse game_profile.json: $ProfilePath"
    }

    if ($profile.PSObject.Properties.Match("mods").Count -eq 0 -or $null -eq $profile.mods) {
        $profile | Add-Member -MemberType NoteProperty -Name "mods" -Value ([PSCustomObject]@{}) -Force
    }
    if (-not ($profile.mods -is [PSCustomObject])) {
        throw "game_profile.json field 'mods' must be an object to auto-enable mod."
    }
    if ($profile.mods.PSObject.Properties.Match("enabled").Count -eq 0 -or $null -eq $profile.mods.enabled) {
        $profile.mods | Add-Member -MemberType NoteProperty -Name "enabled" -Value @() -Force
    }
    if (-not ($profile.mods.enabled -is [System.Collections.IEnumerable])) {
        throw "game_profile.json field 'mods.enabled' must be an array."
    }

    $enabled = @()
    foreach ($entry in $profile.mods.enabled) {
        if ($null -ne $entry) {
            $enabled += [string]$entry
        }
    }
    if (-not ($enabled -contains $TargetModId)) {
        $enabled += $TargetModId
    }
    $profile.mods.enabled = $enabled

    $json = $profile | ConvertTo-Json -Depth 16
    Set-Content -Path $ProfilePath -Value $json -Encoding UTF8
}

if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
    throw "GameRoot directory does not exist: $GameRoot"
}
$resolvedGameRoot = (Resolve-Path $GameRoot).Path

$trimmedModId = $ModId.Trim()
if ([string]::IsNullOrWhiteSpace($trimmedModId)) {
    throw "ModId must not be empty."
}

$modDirectoryName = Normalize-ModDirectoryName -Value $trimmedModId
$modsRoot = Join-Path $resolvedGameRoot "mods"
$modRoot = Join-Path $modsRoot $modDirectoryName
$normalizedStartupScenePath = Normalize-RelativeAssetPath -Path $StartupScenePath
$normalizedUiOverridePath = Normalize-RelativeAssetPath -Path $UiOverridePath
$trimmedContentRoot = $ContentRoot.Trim()
if ([string]::IsNullOrWhiteSpace($trimmedContentRoot)) {
    throw "ContentRoot must not be empty."
}

if ((Test-Path -LiteralPath $modRoot -PathType Container) -and -not $Force) {
    throw "Mod directory already exists: $modRoot (use -Force to overwrite files)"
}
New-Item -ItemType Directory -Force -Path $modRoot | Out-Null

$dependencyEntries = @()
foreach ($dependencySpec in $DependsOn) {
    $parsed = Parse-DependencySpec -Spec $dependencySpec
    $dependencyEntries += $parsed
}

$depsJson = ""
if ($dependencyEntries.Count -gt 0) {
    $depLines = @()
    foreach ($dep in $dependencyEntries) {
        if ([string]::IsNullOrWhiteSpace($dep.version)) {
            $depLines += "    { `"`"id`"`": `"`"$($dep.id)`"`" }"
        } else {
            $depLines += "    { `"`"id`"`": `"`"$($dep.id)`"`", `"`"version`"`": `"`"$($dep.version)`"`" }"
        }
    }
    $depsJson = "`n  `"deps`": [`n$($depLines -join ",`n")`n  ]"
}

$manifestJson = @"
{
  "schemaVersion": 1,
  "id": "$trimmedModId",
  "version": "$Version",
  "engineApi": "$EngineApi",
  "gameApi": "$GameApi",
  "contentRoot": "$trimmedContentRoot"$depsJson
}
"@

$modReadmeText = @"
This mod was generated by tools/newmod.ps1.

Files:
- mod_manifest.json : mod metadata (id/version/deps/api compatibility)
- README.txt : this guide
"@

if ($Template -eq "sample") {
    $sampleSceneOverrideJson = @"
{
  "version": 1,
  "folders": [],
  "entities": [
    {
      "name": "SampleModEntity",
      "guid": 11001,
      "folderPath": "ModSample",
      "isEditorOnly": false,
      "active": true,
      "visible": true,
      "components": [
        {
          "type": "engine.Transform",
          "guid": 11002,
          "active": true,
          "data": {
            "position": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0, 1.0],
            "scale": [1.0, 1.0, 1.0],
            "parentEntityGuid": 0
          }
        }
      ]
    }
  ]
}
"@

    $sampleUiOverrideJson = @"
{
  "version": 2,
  "name": "SampleModUiOverride",
  "root": {
    "name": "Root",
    "visible": true,
    "enabled": true,
    "children": []
  }
}
"@

    $modReadmeText += @"
- $trimmedContentRoot/$normalizedStartupScenePath : sample scene override (contains 1 engine.Transform component)
- $trimmedContentRoot/$normalizedUiOverridePath : sample UI document override (version=2)
"@

    Write-TextFile -Path (Join-Path $modRoot "$trimmedContentRoot/$normalizedStartupScenePath") -Content $sampleSceneOverrideJson
    Write-TextFile -Path (Join-Path $modRoot "$trimmedContentRoot/$normalizedUiOverridePath") -Content $sampleUiOverrideJson
}

Write-TextFile -Path (Join-Path $modRoot "mod_manifest.json") -Content $manifestJson
Write-TextFile -Path (Join-Path $modRoot "README.txt") -Content $modReadmeText

if ($EnableInProfile) {
    $profilePath = Join-Path $resolvedGameRoot "config/game_profile.json"
    Update-GameProfileEnabledMods -ProfilePath $profilePath -TargetModId $trimmedModId
}

Write-Host "Created mod template:"
Write-Host "  GameRoot : $resolvedGameRoot"
Write-Host "  ModRoot  : $modRoot"
Write-Host "  ModId    : $trimmedModId"
Write-Host "  Template : $Template"
if ($EnableInProfile) {
    Write-Host "  Updated  : $(Join-Path $resolvedGameRoot 'config/game_profile.json') (mods.enabled)"
}
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1) Add or edit assets under '$modRoot/$trimmedContentRoot'."
if ($Template -eq "sample") {
    Write-Host "  2) If needed, adjust sample override paths:"
    Write-Host "     - $trimmedContentRoot/$normalizedStartupScenePath"
    Write-Host "     - $trimmedContentRoot/$normalizedUiOverridePath"
}
Write-Host "  2) Validate startup:"
Write-Host "     UnnamedEditorApp.exe --project=$resolvedGameRoot/config/game_profile.json --validate-startup-only"
