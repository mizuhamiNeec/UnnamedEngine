param(
    [Parameter(Mandatory = $true)]
    [string]$PackagedRoot,
    [string]$AppExePath = "",
    [string]$ProjectPath = "",
    [string]$SandboxRoot = "",
    [switch]$KeepSandbox,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

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

function Get-RelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,
        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $baseFull.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $baseFull += [System.IO.Path]::DirectorySeparatorChar
    }
    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)

    $baseUri = New-Object System.Uri($baseFull)
    $targetUri = New-Object System.Uri($targetFull)
    $relativeUri = $baseUri.MakeRelativeUri($targetUri)
    $relative = [System.Uri]::UnescapeDataString($relativeUri.ToString())
    return ($relative -replace "/", "\")
}

function Resolve-AppExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedPackagedRoot,
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "App executable was not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $preferredOrder = @(
        "UnnamedEditorApp.exe",
        "UnnamedStandaloneApp.exe",
        "UnnamedLauncher.exe"
    )

    foreach ($name in $preferredOrder) {
        $candidate = Join-Path $ResolvedPackagedRoot $name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    $allCandidates = Get-ChildItem -LiteralPath $ResolvedPackagedRoot -File -Filter "*App.exe" | Sort-Object Name
    if ($allCandidates.Count -gt 0) {
        return $allCandidates[0].FullName
    }

    throw "Could not find executable under packaged root: $ResolvedPackagedRoot"
}

function Resolve-ProjectPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedPackagedRoot,
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "Project manifest was not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $defaultPath = Join-Path $ResolvedPackagedRoot "config\game_profile.json"
    if (-not (Test-Path -LiteralPath $defaultPath -PathType Leaf)) {
        throw "Default project manifest was not found: $defaultPath"
    }
    return $defaultPath
}

if (-not (Test-Path -LiteralPath $PackagedRoot -PathType Container)) {
    throw "Packaged root directory does not exist: $PackagedRoot"
}
$resolvedPackagedRoot = (Resolve-Path $PackagedRoot).Path

$resolvedAppExe = Resolve-AppExecutable -ResolvedPackagedRoot $resolvedPackagedRoot -ExplicitPath $AppExePath
$resolvedProject = Resolve-ProjectPath -ResolvedPackagedRoot $resolvedPackagedRoot -ExplicitPath $ProjectPath

if ([string]::IsNullOrWhiteSpace($SandboxRoot)) {
    $SandboxRoot = Join-Path $env:TEMP ("unnamed_isolated_validate_{0}" -f ([Guid]::NewGuid().ToString("N")))
}
$resolvedSandboxRoot = [System.IO.Path]::GetFullPath($SandboxRoot)

if (Test-Path -LiteralPath $resolvedSandboxRoot) {
    if (-not $Force) {
        throw "Sandbox root already exists: $resolvedSandboxRoot (use -Force to overwrite)"
    }
    Remove-Item -LiteralPath $resolvedSandboxRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $resolvedSandboxRoot | Out-Null

$isolationRoot = Join-Path $resolvedSandboxRoot "game"
Copy-DirectoryContents -SourceDir $resolvedPackagedRoot -DestinationDir $isolationRoot

$isolatedExe = Join-Path $isolationRoot (Split-Path -Leaf $resolvedAppExe)
$relativeProject = Get-RelativePath -BasePath $resolvedPackagedRoot -TargetPath $resolvedProject
$isolatedProject = Join-Path $isolationRoot $relativeProject

if (-not (Test-Path -LiteralPath $isolatedExe -PathType Leaf)) {
    throw "Isolated executable was not found: $isolatedExe"
}
if (-not (Test-Path -LiteralPath $isolatedProject -PathType Leaf)) {
    throw "Isolated project manifest was not found: $isolatedProject"
}

$stdoutPath = [System.IO.Path]::GetTempFileName()
$stderrPath = [System.IO.Path]::GetTempFileName()
try {
    $oldRepoRootEnv = $env:UNNAMED_REPO_ROOT
    $oldProjectsRootEnv = $env:UNNAMED_PROJECTS_ROOT
    $env:UNNAMED_REPO_ROOT = ""
    $env:UNNAMED_PROJECTS_ROOT = ""

    $process = $null
    try {
        $process = Start-Process -FilePath $isolatedExe -ArgumentList @("--project=$isolatedProject", "--validate-startup-only") -WorkingDirectory $isolationRoot -Wait -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    }
    finally {
        $env:UNNAMED_REPO_ROOT = $oldRepoRootEnv
        $env:UNNAMED_PROJECTS_ROOT = $oldProjectsRootEnv
    }

    $stdout = Get-Content -Path $stdoutPath -Raw
    $stderr = Get-Content -Path $stderrPath -Raw
    $combined = (($stdout + [Environment]::NewLine + $stderr).Trim())
    if (-not [string]::IsNullOrWhiteSpace($combined)) {
        Write-Host $combined
    }

    if ($null -eq $process) {
        throw "failed to start isolated validation process."
    }
    if ($process.ExitCode -ne 0) {
        throw "isolated startup validation failed with exit code $($process.ExitCode)"
    }
    $hasLegacyPassedStatus = $combined -match "status=passed"
    $hasSucceededMessage = $combined -match "validate-startup-only\s+succeeded"
    if (-not ($hasLegacyPassedStatus -or $hasSucceededMessage)) {
        throw "isolated startup validation failed (success marker was not found in output)."
    }
}
finally {
    if (Test-Path -LiteralPath $stdoutPath) { Remove-Item -LiteralPath $stdoutPath -Force }
    if (Test-Path -LiteralPath $stderrPath) { Remove-Item -LiteralPath $stderrPath -Force }

    if (-not $KeepSandbox -and (Test-Path -LiteralPath $resolvedSandboxRoot)) {
        Remove-Item -LiteralPath $resolvedSandboxRoot -Recurse -Force
    }
}

Write-Host "Isolated packaged validation passed:"
Write-Host "  PackagedRoot : $resolvedPackagedRoot"
Write-Host "  Executable   : $resolvedAppExe"
Write-Host "  Project      : $resolvedProject"
Write-Host "  SandboxRoot  : $resolvedSandboxRoot"
if ($KeepSandbox) {
    Write-Host "  Sandbox      : kept"
} else {
    Write-Host "  Sandbox      : removed"
}
