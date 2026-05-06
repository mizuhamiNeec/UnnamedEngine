param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [Parameter(Mandatory = $true)]
    [string]$ModId,
    [string]$OutputRoot = "",
    [ValidateSet("folder", "zip")]
    [string]$Format = "folder",
    [switch]$IncludeUnchanged,
    [switch]$KeepFolderWhenZip,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

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

function Try-LoadJson {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $raw = Get-Content -Path $Path -Raw
    if ([string]::IsNullOrWhiteSpace($raw)) {
        return $null
    }

    return ($raw | ConvertFrom-Json)
}

function Resolve-ModRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$GameRoot,
        [Parameter(Mandatory = $true)]
        [string]$RequestedModId
    )

    $modsRoot = Join-Path $GameRoot "mods"
    if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
        throw "mods directory was not found: $modsRoot"
    }

    $candidates = @(
        (Join-Path $modsRoot $RequestedModId),
        (Join-Path $modsRoot (Normalize-ModDirectoryName -Value $RequestedModId))
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            return (Resolve-Path $candidate).Path
        }
    }

    foreach ($entry in Get-ChildItem -LiteralPath $modsRoot -Directory) {
        $manifestPath = Join-Path $entry.FullName "mod_manifest.json"
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
            continue
        }

        $manifest = Try-LoadJson -Path $manifestPath
        if ($null -eq $manifest) {
            continue
        }

        if ($manifest.PSObject.Properties.Match("id") -and [string]$manifest.id -eq $RequestedModId) {
            return $entry.FullName
        }
    }

    throw "Mod '$RequestedModId' was not found under '$modsRoot'."
}

function Resolve-BaseContentRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$GameRoot
    )

    $profilePath = Join-Path $GameRoot "config/game_profile.json"
    $profile = Try-LoadJson -Path $profilePath
    if ($null -eq $profile) {
        return (Resolve-PathAgainstBase -BasePath $GameRoot -Value "content")
    }

    if (-not $profile.PSObject.Properties.Match("contentRoot")) {
        return (Resolve-PathAgainstBase -BasePath $GameRoot -Value "content")
    }

    $profileDir = Split-Path -Parent $profilePath
    return (Resolve-PathAgainstBase -BasePath $profileDir -Value ([string]$profile.contentRoot))
}

function Ensure-ManifestShape {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Manifest,
        [Parameter(Mandatory = $true)]
        [string]$ManifestPath
    )

    foreach ($requiredField in @("schemaVersion", "id", "version", "engineApi", "gameApi")) {
        if (-not $Manifest.PSObject.Properties.Match($requiredField)) {
            throw "mod_manifest.json is missing required field '$requiredField': $ManifestPath"
        }
    }

    if (-not $Manifest.PSObject.Properties.Match("contentRoot") -or [string]::IsNullOrWhiteSpace([string]$Manifest.contentRoot)) {
        $Manifest | Add-Member -MemberType NoteProperty -Name "contentRoot" -Value "content" -Force
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

$resolvedModRoot = Resolve-ModRoot -GameRoot $resolvedGameRoot -RequestedModId $trimmedModId
$manifestPath = Join-Path $resolvedModRoot "mod_manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "mod_manifest.json was not found: $manifestPath"
}
$manifest = Try-LoadJson -Path $manifestPath
if ($null -eq $manifest) {
    throw "Failed to parse mod_manifest.json: $manifestPath"
}
Ensure-ManifestShape -Manifest $manifest -ManifestPath $manifestPath

$modPackageId = [string]$manifest.id
$modVersion = [string]$manifest.version
$modInstallDirName = Split-Path -Leaf $resolvedModRoot
if ([string]::IsNullOrWhiteSpace($modInstallDirName)) {
    $modInstallDirName = Normalize-ModDirectoryName -Value $modPackageId
}
$modContentRootRelative = ([string]$manifest.contentRoot).Trim()
$resolvedModContentRoot = Resolve-PathAgainstBase -BasePath $resolvedModRoot -Value $modContentRootRelative
if (-not (Test-Path -LiteralPath $resolvedModContentRoot -PathType Container)) {
    throw "mod content root was not found: $resolvedModContentRoot"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $resolvedGameRoot "packaged/mods"
}
$resolvedOutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $resolvedOutputRoot | Out-Null

$safeVersion = $modVersion -replace "[^A-Za-z0-9._-]", "-"
$packageFolderName = "$modPackageId-$safeVersion"
$stagingDir = Join-Path $resolvedOutputRoot $packageFolderName
if (Test-Path -LiteralPath $stagingDir) {
    if (-not $Force) {
        throw "package output already exists: $stagingDir (use -Force to overwrite)"
    }
    Remove-Item -LiteralPath $stagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

$baseContentRoot = Resolve-BaseContentRoot -GameRoot $resolvedGameRoot

$modInstallRoot = Join-Path $stagingDir $modInstallDirName
New-Item -ItemType Directory -Force -Path $modInstallRoot | Out-Null
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $modInstallRoot "mod_manifest.json") -Force

$targetContentRoot = Join-Path $modInstallRoot $modContentRootRelative
New-Item -ItemType Directory -Force -Path $targetContentRoot | Out-Null

$copiedCount = 0
$skippedUnchangedCount = 0
$modFiles = Get-ChildItem -LiteralPath $resolvedModContentRoot -Recurse -File
foreach ($file in $modFiles) {
    $relativePath = $file.FullName.Substring($resolvedModContentRoot.Length).TrimStart("\", "/")
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
        continue
    }

    if (-not $IncludeUnchanged) {
        $baseFilePath = Join-Path $baseContentRoot $relativePath
        if (Test-Path -LiteralPath $baseFilePath -PathType Leaf) {
            $modHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
            $baseHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $baseFilePath).Hash
            if ($modHash -eq $baseHash) {
                $skippedUnchangedCount++
                continue
            }
        }
    }

    $destPath = Join-Path $targetContentRoot $relativePath
    $destDir = Split-Path -Parent $destPath
    if (-not [string]::IsNullOrWhiteSpace($destDir)) {
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    }

    Copy-Item -LiteralPath $file.FullName -Destination $destPath -Force
    $copiedCount++
}

$zipPath = ""
if ($Format -eq "zip") {
    $zipPath = Join-Path $resolvedOutputRoot ("$packageFolderName.zip")
    if (Test-Path -LiteralPath $zipPath) {
        if (-not $Force) {
            throw "zip output already exists: $zipPath (use -Force to overwrite)"
        }
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

    if (-not $KeepFolderWhenZip) {
        Remove-Item -LiteralPath $stagingDir -Recurse -Force
    }
}

Write-Host "Packaged mod:"
Write-Host "  GameRoot           : $resolvedGameRoot"
Write-Host "  ModRoot            : $resolvedModRoot"
Write-Host "  PackageId          : $modPackageId"
Write-Host "  Version            : $modVersion"
Write-Host "  Install Dir        : $modInstallDirName"
Write-Host "  Format             : $Format"
Write-Host "  Included assets    : $copiedCount"
Write-Host "  Skipped unchanged  : $skippedUnchangedCount"
if ($Format -eq "zip") {
    Write-Host "  Zip                : $zipPath"
} else {
    Write-Host "  Folder             : $stagingDir"
}
Write-Host ""
Write-Host "Notes:"
Write-Host "  - package root contains '$modInstallDirName/' (copy or extract into '<GameRoot>/mods')."
Write-Host "  - mod_manifest.json is always included."
if ($IncludeUnchanged) {
    Write-Host "  - unchanged files against base content are included."
} else {
    Write-Host "  - unchanged files against base content are excluded."
}
