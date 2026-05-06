param(
    [Parameter(Mandatory = $true)]
    [string]$Project,
    [string]$AppExePath = "",
    [switch]$KeepArtifacts
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

function Resolve-AppExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$GameName,
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "App executable was not found: $ExplicitPath"
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    $preferredCandidates = @(
        (Join-Path $RepoRoot "bin\\Debug-windows-x86_64\\UnnamedLauncher\\UnnamedLauncher.exe"),
        (Join-Path $RepoRoot "bin\\Debug-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe"),
        (Join-Path $RepoRoot ("bin\\Debug-windows-x86_64\\{0}App\\{0}App.exe" -f $GameName)),
        (Join-Path $RepoRoot "bin\\Release-windows-x86_64\\UnnamedLauncher\\UnnamedLauncher.exe"),
        (Join-Path $RepoRoot "bin\\Release-windows-x86_64\\UnnamedEditorApp\\UnnamedEditorApp.exe"),
        (Join-Path $RepoRoot ("bin\\Release-windows-x86_64\\{0}App\\{0}App.exe" -f $GameName))
    )
    foreach ($candidate in $preferredCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $searchRoot = Join-Path $RepoRoot "bin"
    if (-not (Test-Path -LiteralPath $searchRoot -PathType Container)) {
        throw "Could not find app executable. bin directory was not found: $searchRoot"
    }

    $found = Get-ChildItem -LiteralPath $searchRoot -Recurse -File -Filter "*.exe" |
        Where-Object { $_.Name -like "*App.exe" } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $found) {
        throw "Could not auto-detect app executable under: $searchRoot"
    }
    return $found.FullName
}

function Ensure-BooleanField {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [bool]$Value
    )

    if ($Object.PSObject.Properties.Match($Name).Count -eq 0) {
        $Object | Add-Member -MemberType NoteProperty -Name $Name -Value $Value -Force
    } else {
        $Object.$Name = $Value
    }
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
    }
    finally {
        if (Test-Path -LiteralPath $stdoutPath) { Remove-Item -LiteralPath $stdoutPath -Force }
        if (Test-Path -LiteralPath $stderrPath) { Remove-Item -LiteralPath $stderrPath -Force }
    }
}

$resolvedProject = [System.IO.Path]::GetFullPath($Project)
$manifest = Load-JsonObject -Path $resolvedProject

if ($manifest.PSObject.Properties.Match("gameName").Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$manifest.gameName)) {
    throw "game_profile.json is missing required field 'gameName': $resolvedProject"
}
if ($manifest.PSObject.Properties.Match("gameRoot").Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$manifest.gameRoot)) {
    throw "game_profile.json is missing required field 'gameRoot': $resolvedProject"
}
if ($manifest.PSObject.Properties.Match("contentRoot").Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$manifest.contentRoot)) {
    throw "game_profile.json is missing required field 'contentRoot': $resolvedProject"
}
if ($manifest.PSObject.Properties.Match("configRoot").Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$manifest.configRoot)) {
    throw "game_profile.json is missing required field 'configRoot': $resolvedProject"
}
if ($manifest.PSObject.Properties.Match("runtimeBinary").Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$manifest.runtimeBinary)) {
    throw "game_profile.json is missing required field 'runtimeBinary' for runtime swap validation: $resolvedProject"
}

$manifestDir = Split-Path -Parent $resolvedProject
$resolvedGameRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.gameRoot)
$resolvedContentRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.contentRoot)
$resolvedConfigRoot = Resolve-PathAgainstBase -BasePath $manifestDir -Value ([string]$manifest.configRoot)
$resolvedRuntimeBinary = Resolve-PathAgainstBase -BasePath $resolvedGameRoot -Value ([string]$manifest.runtimeBinary)
if (-not (Test-Path -LiteralPath $resolvedRuntimeBinary -PathType Leaf)) {
    throw "runtimeBinary file was not found: $resolvedRuntimeBinary"
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedAppExe = Resolve-AppExecutable -RepoRoot $repoRoot -GameName ([string]$manifest.gameName) -ExplicitPath $AppExePath
$workingDirectory = Split-Path -Parent $resolvedAppExe

$runtimeInfoBefore = Get-Item -LiteralPath $resolvedRuntimeBinary
$runtimeHashBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedRuntimeBinary).Hash

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("unnamed-runtime-swap-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
$tempProjectPath = Join-Path $tempRoot "game_profile.runtime-swap.json"

Ensure-BooleanField -Object $manifest -Name "requireRuntimeBinary" -Value $true
Ensure-BooleanField -Object $manifest -Name "preferRuntimeBinary" -Value $true
$manifest.gameRoot = $resolvedGameRoot.Replace("\", "/")
$manifest.contentRoot = $resolvedContentRoot.Replace("\", "/")
$manifest.configRoot = $resolvedConfigRoot.Replace("\", "/")
$manifest | ConvertTo-Json -Depth 16 | Set-Content -Path $tempProjectPath -Encoding UTF8

$swapCandidatePath = Join-Path $tempRoot (Split-Path -Leaf $resolvedRuntimeBinary)
$runtimeBackupPath = "$resolvedRuntimeBinary.swapbak"

if (Test-Path -LiteralPath $runtimeBackupPath -PathType Leaf) {
    throw "Backup file already exists. Remove it and retry: $runtimeBackupPath"
}

$restored = $false
try {
    Invoke-StartupValidation -AppPath $resolvedAppExe -ProjectPath $tempProjectPath -WorkingDirectory $workingDirectory -Label "before-swap"

    Copy-Item -LiteralPath $resolvedRuntimeBinary -Destination $swapCandidatePath -Force
    (Get-Item -LiteralPath $swapCandidatePath).LastWriteTimeUtc = [DateTime]::UtcNow.AddSeconds(1)

    Copy-Item -LiteralPath $resolvedRuntimeBinary -Destination $runtimeBackupPath -Force
    Copy-Item -LiteralPath $swapCandidatePath -Destination $resolvedRuntimeBinary -Force
    (Get-Item -LiteralPath $resolvedRuntimeBinary).LastWriteTimeUtc = [DateTime]::UtcNow.AddSeconds(2)

    $runtimeInfoAfterSwap = Get-Item -LiteralPath $resolvedRuntimeBinary
    $runtimeHashAfterSwap = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedRuntimeBinary).Hash

    Invoke-StartupValidation -AppPath $resolvedAppExe -ProjectPath $tempProjectPath -WorkingDirectory $workingDirectory -Label "after-swap"

    Copy-Item -LiteralPath $runtimeBackupPath -Destination $resolvedRuntimeBinary -Force
    Remove-Item -LiteralPath $runtimeBackupPath -Force
    $restored = $true

    Write-Host "Runtime swap validation passed:"
    Write-Host "  Project                : $resolvedProject"
    Write-Host "  App                    : $resolvedAppExe"
    Write-Host "  RuntimeBinary          : $resolvedRuntimeBinary"
    Write-Host "  Runtime Size (bytes)   : $($runtimeInfoBefore.Length)"
    Write-Host "  Runtime WriteTime UTC  : $($runtimeInfoBefore.LastWriteTimeUtc.ToString('o')) -> $($runtimeInfoAfterSwap.LastWriteTimeUtc.ToString('o'))"
    Write-Host "  Runtime Hash (SHA256)  : $runtimeHashBefore -> $runtimeHashAfterSwap"
    Write-Host "  Result                 : status=passed (before-swap / after-swap)"
}
finally {
    if (-not $restored -and (Test-Path -LiteralPath $runtimeBackupPath -PathType Leaf)) {
        Copy-Item -LiteralPath $runtimeBackupPath -Destination $resolvedRuntimeBinary -Force
        Remove-Item -LiteralPath $runtimeBackupPath -Force
    }
    if (-not $KeepArtifacts -and (Test-Path -LiteralPath $tempRoot -PathType Container)) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
