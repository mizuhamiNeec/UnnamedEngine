param(
    [ValidateSet("create", "enable", "validate", "package", "all")]
    [string]$Action = "all",
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [Parameter(Mandatory = $true)]
    [string]$ModId,
    [ValidateSet("minimal", "sample")]
    [string]$Template = "sample",
    [ValidateSet("folder", "zip")]
    [string]$PackageFormat = "zip",
    [string]$PackageOutputRoot = "",
    [switch]$EnableOnCreate,
    [switch]$IncludeUnchanged,
    [switch]$KeepPackageFolderWhenZip,
    [switch]$SkipStartupValidation,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Resolve-ToolScriptPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptName
    )

    $path = Join-Path $PSScriptRoot $ScriptName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required script was not found: $path"
    }
    return (Resolve-Path $path).Path
}

function Invoke-ToolScript {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & powershell -ExecutionPolicy Bypass -File $ScriptPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Tool script failed: $ScriptPath"
    }
}

function Resolve-EditorExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $preferred = @(
        (Join-Path $RepoRoot "bin\\Debug-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe"),
        (Join-Path $RepoRoot "bin\\Release-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe")
    )

    foreach ($candidate in $preferred) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $searchRoot = Join-Path $RepoRoot "bin"
    if (-not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
        throw "Could not find editor executable. bin directory was not found: $searchRoot"
    }

    $found = Get-ChildItem -LiteralPath $searchRoot -Recurse -File -Filter "UnnamedEditorApp.exe" |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $found) {
        throw "Could not find UnnamedEditorApp.exe under: $searchRoot"
    }
    return $found.FullName
}

function Enable-ModInProfile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProfilePath,
        [Parameter(Mandatory = $true)]
        [string]$ModId
    )

    if (-not (Test-Path -LiteralPath $ProfilePath -PathType Leaf)) {
        throw "game_profile.json was not found: $ProfilePath"
    }

    $profileRaw = Get-Content -Path $ProfilePath -Raw
    $profile = $profileRaw | ConvertFrom-Json
    if ($null -eq $profile) {
        throw "Failed to parse game_profile.json: $ProfilePath"
    }

    if (-not $profile.PSObject.Properties.Match("mods") -or $null -eq $profile.mods) {
        $profile | Add-Member -MemberType NoteProperty -Name "mods" -Value ([PSCustomObject]@{}) -Force
    }
    if (-not ($profile.mods -is [PSCustomObject])) {
        throw "game_profile.json field 'mods' must be an object."
    }

    if (-not $profile.mods.PSObject.Properties.Match("enabled") -or $null -eq $profile.mods.enabled) {
        $profile.mods | Add-Member -MemberType NoteProperty -Name "enabled" -Value @() -Force
    }

    $enabled = @()
    foreach ($entry in $profile.mods.enabled) {
        if ($null -ne $entry) {
            $enabled += [string]$entry
        }
    }
    if (-not ($enabled -contains $ModId)) {
        $enabled += $ModId
    }
    $profile.mods.enabled = $enabled

    $profile | ConvertTo-Json -Depth 16 | Set-Content -Path $ProfilePath -Encoding UTF8
}

function Validate-Startup {
    param(
        [Parameter(Mandatory = $true)]
        [string]$EditorExe,
        [Parameter(Mandatory = $true)]
        [string]$ProjectProfilePath,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory
    )

    $stdoutPath = [System.IO.Path]::GetTempFileName()
    $stderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $process = Start-Process -FilePath $EditorExe -ArgumentList @("--project=$ProjectProfilePath", "--validate-startup-only") -WorkingDirectory $WorkingDirectory -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

        $stdout = Get-Content -Path $stdoutPath -Raw
        $stderr = Get-Content -Path $stderrPath -Raw
        $combined = (($stdout + [Environment]::NewLine + $stderr).Trim())
        if (-not [string]::IsNullOrWhiteSpace($combined)) {
            Write-Host $combined
        }

        if ($process.ExitCode -ne 0) {
            throw "Startup validation failed with exit code $($process.ExitCode)."
        }
        if ($combined -notmatch "status=passed") {
            throw "Startup validation failed (status=passed was not found in output)."
        }
    }
    finally {
        if (Test-Path -LiteralPath $stdoutPath) { Remove-Item -LiteralPath $stdoutPath -Force }
        if (Test-Path -LiteralPath $stderrPath) { Remove-Item -LiteralPath $stderrPath -Force }
    }
}

if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
    throw "GameRoot directory does not exist: $GameRoot"
}
$resolvedGameRoot = (Resolve-Path $GameRoot).Path
$trimmedModId = $ModId.Trim()
if ([string]::IsNullOrWhiteSpace($trimmedModId)) {
    throw "ModId must not be empty."
}

$profilePath = Join-Path $resolvedGameRoot "config\\game_profile.json"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$newModScript = Resolve-ToolScriptPath -ScriptName "newmod.ps1"
$packageModScript = Resolve-ToolScriptPath -ScriptName "package-mod.ps1"

$didCreate = $false
$didEnable = $false
$didValidate = $false
$didPackage = $false

if ($Action -in @("create", "all")) {
    $args = @(
        "-GameRoot", $resolvedGameRoot,
        "-ModId", $trimmedModId,
        "-Template", $Template
    )
    if ($EnableOnCreate -or $Action -eq "all") {
        $args += "-EnableInProfile"
    }
    if ($Force) {
        $args += "-Force"
    }

    Invoke-ToolScript -ScriptPath $newModScript -Arguments $args
    $didCreate = $true
    if ($EnableOnCreate -or $Action -eq "all") {
        $didEnable = $true
    }
}

if ($Action -eq "enable") {
    Enable-ModInProfile -ProfilePath $profilePath -ModId $trimmedModId
    $didEnable = $true
}

if ($Action -in @("validate", "all") -and -not $SkipStartupValidation) {
    $editorExe = Resolve-EditorExecutable -RepoRoot $repoRoot
    Validate-Startup -EditorExe $editorExe -ProjectProfilePath $profilePath -WorkingDirectory $repoRoot
    $didValidate = $true
}

if ($Action -in @("package", "all")) {
    $packageArgs = @(
        "-GameRoot", $resolvedGameRoot,
        "-ModId", $trimmedModId,
        "-Format", $PackageFormat
    )
    if (-not [string]::IsNullOrWhiteSpace($PackageOutputRoot)) {
        $packageArgs += @("-OutputRoot", $PackageOutputRoot)
    }
    if ($IncludeUnchanged) {
        $packageArgs += "-IncludeUnchanged"
    }
    if ($KeepPackageFolderWhenZip) {
        $packageArgs += "-KeepFolderWhenZip"
    }
    if ($Force) {
        $packageArgs += "-Force"
    }

    Invoke-ToolScript -ScriptPath $packageModScript -Arguments $packageArgs
    $didPackage = $true
}

Write-Host "Mod workflow completed:"
Write-Host "  Action      : $Action"
Write-Host "  GameRoot    : $resolvedGameRoot"
Write-Host "  ModId       : $trimmedModId"
Write-Host "  Created     : $didCreate"
Write-Host "  Enabled     : $didEnable"
Write-Host "  Validated   : $didValidate"
Write-Host "  Packaged    : $didPackage"
