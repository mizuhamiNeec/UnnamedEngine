# @brief エンジンと対象ゲームを、生成済み .slnx を含むソース配布フォルダへ出力します。

param(
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [string]$OutputRoot = "",
    [ValidateSet("folder", "zip")]
    [string]$Format = "folder",
    [switch]$KeepFolderWhenZip,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Resolve-PathAgainstBase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Value))
}

function Load-JsonObject {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "JSON file was not found: $Path"
    }

    $raw = Get-Content -LiteralPath $Path -Raw
    if ([string]::IsNullOrWhiteSpace($raw)) {
        throw "JSON file is empty: $Path"
    }
    return ($raw | ConvertFrom-Json)
}

function Ensure-ManifestShape {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Manifest,
        [Parameter(Mandatory = $true)]
        [string]$ManifestPath
    )

    foreach ($requiredField in @("schemaVersion", "gameName", "gameRoot", "contentRoot", "configRoot", "defaultStartupScene")) {
        if (-not $Manifest.PSObject.Properties.Match($requiredField)) {
            throw "game_profile.json is missing required field '$requiredField': $ManifestPath"
        }
    }
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDir,
        [string[]]$ExcludedFileNames = @()
    )

    if (-not (Test-Path -LiteralPath $SourceDir -PathType Container)) {
        throw "Source directory was not found: $SourceDir"
    }

    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $SourceDir -Force) {
        if (-not $entry.PSIsContainer -and $ExcludedFileNames -contains $entry.Name) {
            continue
        }
        Copy-Item -LiteralPath $entry.FullName -Destination $DestinationDir -Recurse -Force
    }
}

function Copy-RequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,
        [Parameter(Mandatory = $true)]
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "Required file was not found: $SourcePath"
    }

    $destinationDirectory = Split-Path -Parent $DestinationPath
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
}

function Copy-PremakeScripts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$DestinationDir
    )

    $premakeScripts = @(Get-ChildItem -LiteralPath $SourceDir -File -Filter "*.lua")
    if ($premakeScripts.Count -eq 0) {
        throw "No Premake helper scripts were found: $SourceDir"
    }

    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    foreach ($premakeScript in $premakeScripts) {
        Copy-Item -LiteralPath $premakeScript.FullName -Destination $DestinationDir -Force
    }
}

function Resolve-PremakeCommand {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $localPremake = Join-Path $RepoRoot "premake5.exe"
    if (Test-Path -LiteralPath $localPremake -PathType Leaf) {
        return $localPremake
    }

    $premakeFromPath = Get-Command -Name "premake5.exe" -ErrorAction SilentlyContinue
    if ($premakeFromPath) {
        return $premakeFromPath.Source
    }

    throw "premake5.exe was not found in the repository or PATH. It is required to create a portable .slnx."
}

function Assert-PackagedManifestPaths {
    param([Parameter(Mandatory = $true)][string]$ManifestPath)

    $manifest = Load-JsonObject -Path $ManifestPath
    $manifestDirectory = Split-Path -Parent $ManifestPath
    $contentRoot = Resolve-PathAgainstBase -BasePath $manifestDirectory -Value ([string]$manifest.contentRoot)
    $startupScene = Resolve-PathAgainstBase -BasePath $contentRoot -Value ([string]$manifest.defaultStartupScene)

    if (-not (Test-Path -LiteralPath $contentRoot -PathType Container)) {
        throw "Packaged contentRoot does not exist: $contentRoot"
    }
    if (-not (Test-Path -LiteralPath $startupScene -PathType Leaf)) {
        throw "Packaged defaultStartupScene does not exist: $startupScene"
    }
}

function Write-LaunchScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$ProjectSubPath,
        [Parameter(Mandatory = $true)]
        [string]$ExecutableProjectName
    )

    $scriptText = @"
param(
    [Parameter(ValueFromRemainingArguments = `$true)]
    [string[]]`$ExtraArgs
)

`$ErrorActionPreference = "Stop"
`$root = Split-Path -Parent `$PSCommandPath
`$projectPath = Join-Path `$root "$ProjectSubPath"
`$candidateExecutables = @(
    "bin/Release-windows-x86_64/$ExecutableProjectName/$ExecutableProjectName.exe",
    "bin/Develop-windows-x86_64/$ExecutableProjectName/$ExecutableProjectName.exe",
    "bin/Debug-windows-x86_64/$ExecutableProjectName/$ExecutableProjectName.exe"
)

`$executablePath = `$null
foreach (`$candidateSubPath in `$candidateExecutables) {
    `$candidatePath = Join-Path `$root `$candidateSubPath
    if (Test-Path -LiteralPath `$candidatePath -PathType Leaf) {
        `$executablePath = `$candidatePath
        break
    }
}

if (`$null -eq `$executablePath) {
    throw "$ExecutableProjectName.exe was not found. Build Unnamed.slnx first."
}

Push-Location `$root
try {
    & `$executablePath "--project=`$projectPath" @ExtraArgs
    exit `$LASTEXITCODE
} finally {
    Pop-Location
}
"@

    Set-Content -LiteralPath $Path -Value $scriptText -Encoding UTF8
}

$resolvedProject = [System.IO.Path]::GetFullPath($Project)
$manifest = Load-JsonObject -Path $resolvedProject
Ensure-ManifestShape -Manifest $manifest -ManifestPath $resolvedProject

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$manifestDirectory = Split-Path -Parent $resolvedProject
$resolvedGameRoot = Resolve-PathAgainstBase -BasePath $manifestDirectory -Value ([string]$manifest.gameRoot)
$resolvedGameContentRoot = Resolve-PathAgainstBase -BasePath $manifestDirectory -Value ([string]$manifest.contentRoot)
$resolvedConfigRoot = Resolve-PathAgainstBase -BasePath $manifestDirectory -Value ([string]$manifest.configRoot)
$resolvedRuntimeRoot = Join-Path $resolvedGameRoot "runtime"
$engineSourceRoot = Join-Path $repoRoot "src"
$engineContentRoot = Join-Path $repoRoot "content"
$premakeScriptsRoot = Join-Path $repoRoot "build/premake"

foreach ($requiredDirectory in @(
    $resolvedGameRoot,
    $resolvedGameContentRoot,
    $resolvedConfigRoot,
    $resolvedRuntimeRoot,
    $engineSourceRoot,
    $engineContentRoot,
    $premakeScriptsRoot
)) {
    if (-not (Test-Path -LiteralPath $requiredDirectory -PathType Container)) {
        throw "Required source directory was not found: $requiredDirectory"
    }
}

$gameName = [string]$manifest.gameName
$gameDirectoryName = Split-Path -Leaf $resolvedGameRoot
if ([string]::IsNullOrWhiteSpace($gameDirectoryName)) {
    throw "Failed to resolve the game directory name from: $resolvedGameRoot"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $resolvedGameRoot "packaged/source"
}
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$packageFolderName = "$gameDirectoryName-source"
$stagingDirectory = Join-Path $resolvedOutputRoot $packageFolderName

if (Test-Path -LiteralPath $stagingDirectory) {
    if (-not $Force) {
        throw "Package output already exists: $stagingDirectory (use -Force to overwrite)"
    }
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingDirectory | Out-Null

$targetGameRoot = Join-Path $stagingDirectory "projects/$gameDirectoryName"
$targetConfigRoot = Join-Path $targetGameRoot "config"
$targetMergedContentRoot = Join-Path $stagingDirectory "content"

Copy-DirectoryContents -SourceDir $engineSourceRoot -DestinationDir (Join-Path $stagingDirectory "src")
Copy-PremakeScripts -SourceDir $premakeScriptsRoot -DestinationDir (Join-Path $stagingDirectory "build/premake")
Copy-DirectoryContents -SourceDir $resolvedRuntimeRoot -DestinationDir (Join-Path $targetGameRoot "runtime")
Copy-DirectoryContents -SourceDir $resolvedConfigRoot -DestinationDir $targetConfigRoot -ExcludedFileNames @("game_profile.json", "user.cfg")
Copy-DirectoryContents -SourceDir $engineContentRoot -DestinationDir $targetMergedContentRoot
Copy-DirectoryContents -SourceDir $resolvedGameContentRoot -DestinationDir $targetMergedContentRoot

Copy-RequiredFile -SourcePath (Join-Path $repoRoot "premake5.lua") -DestinationPath (Join-Path $stagingDirectory "premake5.lua")
Copy-RequiredFile -SourcePath (Join-Path $repoRoot "generateallprojects.ps1") -DestinationPath (Join-Path $stagingDirectory "generateallprojects.ps1")

$portableManifest = [ordered]@{
    schemaVersion = [int]$manifest.schemaVersion
    gameName = $gameName
    aliases = @($manifest.aliases)
    gameRoot = ".."
    contentRoot = "../../../content"
    configRoot = "."
    defaultStartupScene = [string]$manifest.defaultStartupScene
}
if ($manifest.PSObject.Properties.Match("runtimeModule").Count -gt 0) {
    $portableManifest.runtimeModule = [string]$manifest.runtimeModule
}

$portableManifestPath = Join-Path $targetConfigRoot "game_profile.json"
$portableManifest | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $portableManifestPath -Encoding UTF8
Assert-PackagedManifestPaths -ManifestPath $portableManifestPath

$projectSubPath = "projects/$gameDirectoryName/config/game_profile.json"
Write-LaunchScript -Path (Join-Path $stagingDirectory "run-game.ps1") -ProjectSubPath $projectSubPath -ExecutableProjectName "UnnamedLauncher"
Write-LaunchScript -Path (Join-Path $stagingDirectory "run-editor.ps1") -ProjectSubPath $projectSubPath -ExecutableProjectName "UnnamedEditorApp"

$premakeCommand = Resolve-PremakeCommand -RepoRoot $repoRoot
Push-Location $stagingDirectory
try {
    & $premakeCommand "vs2026"
    if ($LASTEXITCODE -ne 0) {
        throw "Premake project generation failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$solutionPath = Join-Path $stagingDirectory "Unnamed.slnx"
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
    throw "Premake completed without creating Unnamed.slnx: $solutionPath"
}

$generatedUserFiles = Get-ChildItem -LiteralPath (Join-Path $stagingDirectory "build/projects") -Recurse -File -Filter "*.vcxproj.user"
foreach ($generatedUserFile in $generatedUserFiles) {
    Remove-Item -LiteralPath $generatedUserFile.FullName -Force
}

$sourceReadme = @"
This folder contains the Unnamed engine and $gameName game source.

Build:
  1. Open Unnamed.slnx in Visual Studio or Rider.
  2. Select x64 and Debug, Develop, or Release.
  3. Build the solution or UnnamedLauncher project.

Command-line example:
  msbuild Unnamed.slnx /m /p:Configuration=Release /p:Platform=x64

Run after building:
  .\run-game.ps1
  .\run-editor.ps1   (Debug only)

Regenerate projects after changing Premake files:
  .\generateallprojects.ps1

The package intentionally excludes Git metadata, existing bin/intermediate outputs,
debug symbols, IDE user settings, local user.cfg, documentation, and unrelated games.
"@
Set-Content -LiteralPath (Join-Path $stagingDirectory "README.txt") -Value $sourceReadme -Encoding UTF8

$zipPath = ""
if ($Format -eq "zip") {
    $zipPath = Join-Path $resolvedOutputRoot "$packageFolderName.zip"
    if (Test-Path -LiteralPath $zipPath) {
        if (-not $Force) {
            throw "Zip output already exists: $zipPath (use -Force to overwrite)"
        }
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $stagingDirectory "*") -DestinationPath $zipPath -CompressionLevel Optimal
    if (-not $KeepFolderWhenZip) {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
}

Write-Host "Packaged game source:"
Write-Host "  GameName      : $gameName"
Write-Host "  Project       : $resolvedProject"
Write-Host "  Solution      : $solutionPath"
Write-Host "  Output        : $stagingDirectory"
Write-Host "  Format        : $Format"
if ($Format -eq "zip") {
    Write-Host "  Zip           : $zipPath"
}
