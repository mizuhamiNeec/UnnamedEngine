param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot,
    [string]$ModId = "",
    [switch]$FailOnUnsigned,
    [switch]$AllowExecutableFiles
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

function Has-Property {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $Object.PSObject.Properties.Match($Name).Count -gt 0
}

function Resolve-ModDirectories {
    param(
        [Parameter(Mandatory = $true)]
        [string]$GameRoot,
        [string]$RequestedModId
    )

    $modsRoot = Join-Path $GameRoot "mods"
    if (-not (Test-Path -LiteralPath $modsRoot -PathType Container)) {
        throw "mods directory was not found: $modsRoot"
    }

    if (-not [string]::IsNullOrWhiteSpace($RequestedModId)) {
        $candidates = @(
            (Join-Path $modsRoot $RequestedModId),
            (Join-Path $modsRoot (Normalize-ModDirectoryName -Value $RequestedModId))
        )
        foreach ($candidate in $candidates) {
            if (Test-Path -LiteralPath $candidate -PathType Container) {
                return @((Resolve-Path $candidate).Path)
            }
        }

        foreach ($entry in Get-ChildItem -LiteralPath $modsRoot -Directory) {
            $manifestPath = Join-Path $entry.FullName "mod_manifest.json"
            $manifest = Try-LoadJson -Path $manifestPath
            if ($null -eq $manifest) {
                continue
            }
            if ((Has-Property -Object $manifest -Name "id") -and [string]$manifest.id -eq $RequestedModId) {
                return @($entry.FullName)
            }
        }

        throw "Mod '$RequestedModId' was not found under '$modsRoot'."
    }

    $resolved = @()
    foreach ($entry in Get-ChildItem -LiteralPath $modsRoot -Directory) {
        $manifestPath = Join-Path $entry.FullName "mod_manifest.json"
        if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
            $resolved += $entry.FullName
        }
    }
    return $resolved
}

function Resolve-SignatureState {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Manifest
    )

    if (-not (Has-Property -Object $Manifest -Name "signature")) {
        return [PSCustomObject]@{
            State = "unsigned"
            Detail = "signature metadata is not defined."
            IsError = $false
        }
    }

    $signature = $Manifest.signature
    if ($null -eq $signature -or -not ($signature -is [PSCustomObject])) {
        return [PSCustomObject]@{
            State = "invalid"
            Detail = "signature field must be an object when provided."
            IsError = $true
        }
    }

    if (-not (Has-Property -Object $signature -Name "type")) {
        return [PSCustomObject]@{
            State = "invalid"
            Detail = "signature.type is required when signature is provided."
            IsError = $true
        }
    }

    $signatureType = ([string]$signature.type).Trim().ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($signatureType) -or $signatureType -eq "unsigned") {
        return [PSCustomObject]@{
            State = "unsigned"
            Detail = "signature.type=unsigned."
            IsError = $false
        }
    }

    if (-not (Has-Property -Object $signature -Name "value") -or [string]::IsNullOrWhiteSpace([string]$signature.value)) {
        return [PSCustomObject]@{
            State = "invalid"
            Detail = "signature.value is required when signature.type is '$signatureType'."
            IsError = $true
        }
    }

    return [PSCustomObject]@{
        State = "signed"
        Detail = "signature.type=$signatureType."
        IsError = $false
    }
}

if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
    throw "GameRoot directory does not exist: $GameRoot"
}

$resolvedGameRoot = (Resolve-Path $GameRoot).Path
$targetModId = $ModId.Trim()
$modDirectories = Resolve-ModDirectories -GameRoot $resolvedGameRoot -RequestedModId $targetModId

if ($modDirectories.Count -eq 0) {
    Write-Host "No mod directories were found under '$resolvedGameRoot\\mods'."
    return
}

$dangerousExtensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($extension in @(".exe", ".dll", ".com", ".bat", ".cmd", ".ps1", ".psm1", ".vbs", ".js", ".jse", ".wsf", ".wsh", ".hta", ".msi", ".scr")) {
    $dangerousExtensions.Add($extension) | Out-Null
}

$errorCount = 0
$warningCount = 0
$unsignedCount = 0

foreach ($modRoot in $modDirectories) {
    $manifestPath = Join-Path $modRoot "mod_manifest.json"
    $manifest = Try-LoadJson -Path $manifestPath
    if ($null -eq $manifest) {
        Write-Host "ERROR: $(Split-Path -Leaf $modRoot) -> failed to parse mod_manifest.json."
        $errorCount++
        continue
    }

    $modName = Split-Path -Leaf $modRoot
    if ((Has-Property -Object $manifest -Name "id") -and -not [string]::IsNullOrWhiteSpace([string]$manifest.id)) {
        $modName = [string]$manifest.id
    }

    $signatureState = Resolve-SignatureState -Manifest $manifest
    if ($signatureState.IsError) {
        Write-Host "ERROR: $modName -> $($signatureState.Detail)"
        $errorCount++
    } elseif ($signatureState.State -eq "unsigned") {
        $unsignedCount++
        if ($FailOnUnsigned) {
            Write-Host "ERROR: $modName -> unsigned mod is not allowed (-FailOnUnsigned)."
            $errorCount++
        } else {
            Write-Host "WARN:  $modName -> unsigned mod ($($signatureState.Detail))"
            $warningCount++
        }
    }

    $dangerousFiles = @()
    foreach ($file in Get-ChildItem -LiteralPath $modRoot -Recurse -File) {
        $extension = [System.IO.Path]::GetExtension($file.FullName).ToLowerInvariant()
        if ($dangerousExtensions.Contains($extension)) {
            $relativePath = $file.FullName.Substring($modRoot.Length).TrimStart("\", "/")
            $dangerousFiles += $relativePath
        }
    }

    if ($dangerousFiles.Count -gt 0) {
        foreach ($relativePath in $dangerousFiles) {
            if ($AllowExecutableFiles) {
                Write-Host "WARN:  $modName -> executable/script file detected: $relativePath (allowed by -AllowExecutableFiles)"
                $warningCount++
            } else {
                Write-Host "ERROR: $modName -> executable/script file is not allowed: $relativePath"
                $errorCount++
            }
        }
    }

    Write-Host "Checked mod: $modName (signature=$($signatureState.State), dangerousFiles=$($dangerousFiles.Count))"
}

Write-Host ""
Write-Host "Security check summary:"
Write-Host "  GameRoot     : $resolvedGameRoot"
if (-not [string]::IsNullOrWhiteSpace($targetModId)) {
    Write-Host "  Target ModId : $targetModId"
}
Write-Host "  Checked Mods : $($modDirectories.Count)"
Write-Host "  Unsigned     : $unsignedCount"
Write-Host "  Warnings     : $warningCount"
Write-Host "  Errors       : $errorCount"

if ($errorCount -gt 0) {
    throw "Mod security check failed."
}
