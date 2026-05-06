param(
    [string]$AppExePath = "",
    [switch]$KeepArtifacts
)

$ErrorActionPreference = "Stop"

function Resolve-AppExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "App executable was not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $preferred = @(
        (Join-Path $RepoRoot "bin\\Debug-windows-x86_64\\UnnamedLauncher\\UnnamedLauncher.exe"),
        (Join-Path $RepoRoot "bin\\Debug-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe"),
        (Join-Path $RepoRoot "bin\\Release-windows-x86_64\\UnnamedLauncher\\UnnamedLauncher.exe"),
        (Join-Path $RepoRoot "bin\\Release-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe")
    )
    foreach ($candidate in $preferred) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $searchRoot = Join-Path $RepoRoot "bin"
    if (-not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
        throw "Could not find app executable. bin directory was not found: $searchRoot"
    }

    $found = Get-ChildItem -LiteralPath $searchRoot -Recurse -File -Filter "*App.exe" |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $found) {
        throw "Could not auto-detect app executable under: $searchRoot"
    }
    return $found.FullName
}

function Write-JsonFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [object]$Object
    )

    $dir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $Object | ConvertTo-Json -Depth 16 | Set-Content -Path $Path -Encoding UTF8
}

function Write-TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Text
    )

    $dir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Set-Content -Path $Path -Value $Text -Encoding UTF8
}

function Invoke-StartupValidation {
    param(
        [Parameter(Mandatory = $true)]
        [string]$AppPath,
        [Parameter(Mandatory = $true)]
        [string]$ProjectPath,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $AppPath -ArgumentList @("--project=$ProjectPath", "--validate-startup-only") -WorkingDirectory $WorkingDirectory -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
        $combined = ((Get-Content -Path $stdoutPath -Raw) + [Environment]::NewLine + (Get-Content -Path $stderrPath -Raw)).Trim()
        if (-not [string]::IsNullOrWhiteSpace($combined)) {
            Write-Host $combined
        }
        if ($process.ExitCode -ne 0) {
            throw "Startup validation failed in '$Label' with exit code $($process.ExitCode)."
        }
        if ($combined -notmatch "status=passed") {
            throw "Startup validation failed in '$Label' (status=passed was not found in output)."
        }
        return $combined
    }
    finally {
        if (Test-Path -LiteralPath $stdoutPath) { Remove-Item -LiteralPath $stdoutPath -Force }
        if (Test-Path -LiteralPath $stderrPath) { Remove-Item -LiteralPath $stderrPath -Force }
    }
}

function Parse-StartupResolution {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputText
    )

    $pattern = "startup scene mount resolution: relative='([^']*)' resolved='([^']*)' layer='([^']*)' root='([^']*)' exists=(true|false)"
    $match = [regex]::Match($OutputText, $pattern)
    if (-not $match.Success) {
        throw "Could not parse startup scene mount resolution line from output."
    }

    return [PSCustomObject]@{
        Relative = $match.Groups[1].Value
        Resolved = $match.Groups[2].Value
        Layer = $match.Groups[3].Value
        Root = $match.Groups[4].Value
        Exists = $match.Groups[5].Value
    }
}

function New-ModManifestObject {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Id
    )

    return [ordered]@{
        schemaVersion = 1
        id = $Id
        version = "1.0.0"
        engineApi = "1"
        gameApi = "1"
        contentRoot = "content"
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$appExe = Resolve-AppExecutable -RepoRoot $repoRoot -ExplicitPath $AppExePath
$workingDirectory = Split-Path -Parent $appExe

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("unnamed-mod-mount-override-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

try {
    $gameRoot = Join-Path $tempRoot "Game"
    $contentRoot = Join-Path $gameRoot "content"
    $configRoot = Join-Path $gameRoot "config"
    $modsRoot = Join-Path $gameRoot "mods"
    $startupRelativePath = "scenes/bootstrap.json"

    $baseStartupPath = Join-Path $contentRoot $startupRelativePath
    $modAStartupPath = Join-Path $modsRoot "mod_a/content/$startupRelativePath"
    $modBStartupPath = Join-Path $modsRoot "mod_b/content/$startupRelativePath"

    Write-TextFile -Path $baseStartupPath -Text '{"source":"base","version":1}'
    Write-TextFile -Path $modAStartupPath -Text '{"source":"mod_a","version":1}'
    Write-TextFile -Path $modBStartupPath -Text '{"source":"mod_b","version":1}'

    Write-JsonFile -Path (Join-Path $modsRoot "mod_a/mod_manifest.json") -Object (New-ModManifestObject -Id "mod_a")
    Write-JsonFile -Path (Join-Path $modsRoot "mod_b/mod_manifest.json") -Object (New-ModManifestObject -Id "mod_b")

    $profilePath = Join-Path $configRoot "game_profile.json"
    $profile = [ordered]@{
        schemaVersion = 1
        gameName = "MountOverrideValidationGame"
        aliases = @("MountOverrideValidationGame")
        gameRoot = ".."
        contentRoot = "../content"
        configRoot = "."
        defaultStartupScene = $startupRelativePath
        mods = [ordered]@{
            root = "mods"
            enabled = @("mod_a", "mod_b")
        }
    }
    Write-JsonFile -Path $profilePath -Object $profile

    $baseResultText = Invoke-StartupValidation -AppPath $appExe -ProjectPath $profilePath -WorkingDirectory $workingDirectory -Label "mod-order-a-b"
    $baseResolved = Parse-StartupResolution -OutputText $baseResultText

    if ($baseResolved.Layer -ne "mod") {
        throw "Expected mod layer resolution in first run, but got '$($baseResolved.Layer)'."
    }
    if ($baseResolved.Resolved -notlike "*mod_b*") {
        throw "Expected mod_b to win in first run, but resolved path was '$($baseResolved.Resolved)'."
    }
    if ($baseResolved.Resolved -like "*content/scenes/bootstrap.json" -and $baseResolved.Resolved -notlike "*mods*") {
        throw "Expected mod override to be selected instead of base content: '$($baseResolved.Resolved)'."
    }

    $modAWithDependency = [ordered]@{
        schemaVersion = 1
        id = "mod_a"
        version = "1.0.0"
        engineApi = "1"
        gameApi = "1"
        contentRoot = "content"
        deps = @(
            [ordered]@{
                id = "mod_b"
            }
        )
    }
    Write-JsonFile -Path (Join-Path $modsRoot "mod_a/mod_manifest.json") -Object $modAWithDependency

    $swappedResultText = Invoke-StartupValidation -AppPath $appExe -ProjectPath $profilePath -WorkingDirectory $workingDirectory -Label "dependency-driven-order"
    $swappedResolved = Parse-StartupResolution -OutputText $swappedResultText

    if ($swappedResolved.Layer -ne "mod") {
        throw "Expected mod layer resolution after swapping order, but got '$($swappedResolved.Layer)'."
    }
    if ($swappedResolved.Resolved -notlike "*mod_a*") {
        throw "Expected mod_a to win after dependency order change, but resolved path was '$($swappedResolved.Resolved)'."
    }
    if ($baseResolved.Resolved -eq $swappedResolved.Resolved) {
        throw "Expected resolved path to change when mount order changes, but both runs resolved '$($baseResolved.Resolved)'."
    }

    Write-Host "Mod mount override validation passed:"
    Write-Host "  App                 : $appExe"
    Write-Host "  Profile             : $profilePath"
    Write-Host "  Run#1 enabled mods  : mod_a, mod_b -> $($baseResolved.Resolved)"
    Write-Host "  Run#2 dependency    : mod_a depends on mod_b -> $($swappedResolved.Resolved)"
    Write-Host "  Result              : base untouched + mod override + mount order difference verified"
}
finally {
    if (-not $KeepArtifacts -and (Test-Path -LiteralPath $tempRoot -PathType Container)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
