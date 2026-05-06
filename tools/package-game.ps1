param(
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [string]$AppExePath = "",
    [string]$OutputRoot = "",
    [switch]$IncludeMods,
    [switch]$ValidateStartup,
    [switch]$ValidateIsolatedStartup,
    [string]$IsolatedValidatorPath = "",
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

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $Value
    }

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $Value))
}

function Load-JsonObject {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "JSON file was not found: $Path"
    }

    $raw = Get-Content -Path $Path -Raw
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

    foreach ($requiredField in @("schemaVersion", "gameName", "aliases", "gameRoot", "contentRoot", "configRoot", "defaultStartupScene")) {
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
        [string]$DestinationDir
    )

    if (-not (Test-Path -LiteralPath $SourceDir -PathType Container)) {
        throw "Source directory was not found: $SourceDir"
    }

    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $SourceDir -Force) {
        Copy-Item -LiteralPath $entry.FullName -Destination $DestinationDir -Recurse -Force
    }
}

function Resolve-AppExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Manifest,
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "App executable was not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
    $binRoot = Join-Path $repoRoot "bin"
    if (-not (Test-Path -LiteralPath $binRoot -PathType Container)) {
        throw "bin directory was not found for auto app detection: $binRoot"
    }

    $preferredName = "{0}App.exe" -f ([string]$Manifest.gameName)
    $candidates = Get-ChildItem -LiteralPath $binRoot -Recurse -File -Filter $preferredName | Sort-Object LastWriteTimeUtc -Descending
    if ($candidates.Count -gt 0) {
        return $candidates[0].FullName
    }

    throw "Could not auto-detect app executable '$preferredName'. Pass -AppExePath explicitly."
}

$resolvedProject = [System.IO.Path]::GetFullPath($Project)
$manifest = Load-JsonObject -Path $resolvedProject
Ensure-ManifestShape -Manifest $manifest -ManifestPath $resolvedProject

$manifestDir = Split-Path -Parent $resolvedProject
$resolvedGameRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.gameRoot)
$resolvedContentRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.contentRoot)
$resolvedConfigRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.configRoot)

if (-not (Test-Path -LiteralPath $resolvedGameRoot -PathType Container)) {
    throw "Resolved gameRoot does not exist: $resolvedGameRoot"
}
if (-not (Test-Path -LiteralPath $resolvedContentRoot -PathType Container)) {
    throw "Resolved contentRoot does not exist: $resolvedContentRoot"
}
if (-not (Test-Path -LiteralPath $resolvedConfigRoot -PathType Container)) {
    throw "Resolved configRoot does not exist: $resolvedConfigRoot"
}

$gameName = [string]$manifest.gameName
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $resolvedGameRoot ("packaged/game/{0}" -f $gameName)
}
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)

if (Test-Path -LiteralPath $resolvedOutputRoot) {
    if (-not $Force) {
        throw "Output directory already exists: $resolvedOutputRoot (use -Force to overwrite)"
    }
    Remove-Item -LiteralPath $resolvedOutputRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedOutputRoot | Out-Null

$targetContentRoot = Join-Path $resolvedOutputRoot "content"
$targetConfigRoot = Join-Path $resolvedOutputRoot "config"
Copy-DirectoryContents -SourceDir $resolvedContentRoot -DestinationDir $targetContentRoot
Copy-DirectoryContents -SourceDir $resolvedConfigRoot -DestinationDir $targetConfigRoot

if ($IncludeMods) {
    $sourceModsRoot = Join-Path $resolvedGameRoot "mods"
    if (Test-Path -LiteralPath $sourceModsRoot -PathType Container) {
        Copy-DirectoryContents -SourceDir $sourceModsRoot -DestinationDir (Join-Path $resolvedOutputRoot "mods")
    }
}

$runtimeBinaryPackagedPath = ""
if ($manifest.PSObject.Properties.Match("runtimeBinary") -and -not [string]::IsNullOrWhiteSpace([string]$manifest.runtimeBinary)) {
    $resolvedRuntimeBinary = Resolve-PathAgainstBase -BasePath $resolvedGameRoot -Value ([string]$manifest.runtimeBinary)
    if (-not (Test-Path -LiteralPath $resolvedRuntimeBinary -PathType Leaf)) {
        if ($manifest.PSObject.Properties.Match("requireRuntimeBinary") -and $manifest.requireRuntimeBinary) {
            throw "runtimeBinary is required but was not found: $resolvedRuntimeBinary"
        }
        Write-Warning "runtimeBinary was not found and will not be packaged: $resolvedRuntimeBinary"
    } else {
        $runtimeDir = Join-Path $resolvedOutputRoot "runtime"
        New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null
        $runtimeFileName = Split-Path -Leaf $resolvedRuntimeBinary
        Copy-Item -LiteralPath $resolvedRuntimeBinary -Destination (Join-Path $runtimeDir $runtimeFileName) -Force
        $runtimeBinaryPackagedPath = "./runtime/$runtimeFileName"
    }
}

$resolvedAppExe = Resolve-AppExecutable -Manifest $manifest -ExplicitPath $AppExePath
$appSourceDir = Split-Path -Parent $resolvedAppExe
Copy-DirectoryContents -SourceDir $appSourceDir -DestinationDir $resolvedOutputRoot

$appParentDir = Split-Path -Parent $appSourceDir
foreach ($supportDirName in @("DirectXTex", "Lua")) {
    $supportDir = Join-Path $appParentDir $supportDirName
    if (Test-Path -LiteralPath $supportDir -PathType Container) {
        Copy-DirectoryContents -SourceDir $supportDir -DestinationDir (Join-Path $resolvedOutputRoot $supportDirName)
    }
}

$packagedManifest = [ordered]@{}
$packagedManifest.schemaVersion = [int]$manifest.schemaVersion
$packagedManifest.gameName = $gameName
$packagedManifest.aliases = @($manifest.aliases)
$packagedManifest.gameRoot = ".."
$packagedManifest.contentRoot = "../content"
$packagedManifest.configRoot = "."
$packagedManifest.defaultStartupScene = [string]$manifest.defaultStartupScene

if ($manifest.PSObject.Properties.Match("requireRuntimeBinary")) {
    $packagedManifest.requireRuntimeBinary = [bool]$manifest.requireRuntimeBinary
}
if ($manifest.PSObject.Properties.Match("preferRuntimeBinary")) {
    $packagedManifest.preferRuntimeBinary = [bool]$manifest.preferRuntimeBinary
}
if (-not [string]::IsNullOrWhiteSpace($runtimeBinaryPackagedPath)) {
    $packagedManifest.runtimeBinary = $runtimeBinaryPackagedPath
}

if ($manifest.PSObject.Properties.Match("mounts") -and $null -ne $manifest.mounts) {
    $packagedManifest.mounts = $manifest.mounts
}
if ($manifest.PSObject.Properties.Match("mods") -and $null -ne $manifest.mods) {
    $packagedManifest.mods = $manifest.mods
    if ($IncludeMods) {
        if (-not $packagedManifest.mods.PSObject.Properties.Match("root")) {
            $packagedManifest.mods | Add-Member -MemberType NoteProperty -Name "root" -Value "../mods" -Force
        } else {
            $packagedManifest.mods.root = "../mods"
        }
    }
}

$packagedManifestPath = Join-Path $targetConfigRoot "game_profile.json"
$packagedManifest | ConvertTo-Json -Depth 16 | Set-Content -Path $packagedManifestPath -Encoding UTF8

$packagedExePath = Join-Path $resolvedOutputRoot (Split-Path -Leaf $resolvedAppExe)
if ($ValidateStartup) {
    if (-not (Test-Path -LiteralPath $packagedExePath -PathType Leaf)) {
        throw "Packaged executable was not found for validation: $packagedExePath"
    }

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $packagedExePath -ArgumentList @("--project=$packagedManifestPath", "--validate-startup-only") -WorkingDirectory $resolvedOutputRoot -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $validationText = ((Get-Content -Path $stdoutPath -Raw) + [Environment]::NewLine + (Get-Content -Path $stderrPath -Raw)).Trim()
        if (-not [string]::IsNullOrWhiteSpace($validationText)) {
            Write-Host $validationText
        }
        if ($process.ExitCode -ne 0) {
            throw "startup validation failed with exit code $($process.ExitCode)"
        }
    } finally {
        if (Test-Path -LiteralPath $stdoutPath) { Remove-Item -LiteralPath $stdoutPath -Force }
        if (Test-Path -LiteralPath $stderrPath) { Remove-Item -LiteralPath $stderrPath -Force }
    }
    if ($validationText -notmatch "status=passed") {
        throw "startup validation failed (status=passed was not found in output)."
    }
}

if ($ValidateIsolatedStartup) {
    if ([string]::IsNullOrWhiteSpace($IsolatedValidatorPath)) {
        $IsolatedValidatorPath = Join-Path $PSScriptRoot "validate-packaged-game.ps1"
    }
    if (-not (Test-Path -LiteralPath $IsolatedValidatorPath -PathType Leaf)) {
        throw "isolated validator script was not found: $IsolatedValidatorPath"
    }

    & powershell -ExecutionPolicy Bypass -File $IsolatedValidatorPath -PackagedRoot $resolvedOutputRoot -AppExePath $packagedExePath -ProjectPath $packagedManifestPath -Force
    if ($LASTEXITCODE -ne 0) {
        throw "isolated startup validation script failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Packaged game:"
Write-Host "  GameName        : $gameName"
Write-Host "  Project         : $resolvedProject"
Write-Host "  App             : $resolvedAppExe"
Write-Host "  Output          : $resolvedOutputRoot"
Write-Host "  Content         : $targetContentRoot"
Write-Host "  Config          : $targetConfigRoot"
if ($IncludeMods) {
    Write-Host "  Mods            : included"
} else {
    Write-Host "  Mods            : excluded"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeBinaryPackagedPath)) {
    Write-Host "  RuntimeBinary   : $runtimeBinaryPackagedPath"
}
if ($ValidateStartup) {
    Write-Host "  Validation      : passed"
}
if ($ValidateIsolatedStartup) {
    Write-Host "  Isolated Check  : passed"
}
