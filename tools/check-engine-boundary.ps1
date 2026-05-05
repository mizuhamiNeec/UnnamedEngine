param(
    [string]$DiffRange,
    [switch]$Staged
)

if (-not $DiffRange -and -not $Staged) {
    throw "Specify either -DiffRange or -Staged."
}

if ($DiffRange -and $Staged) {
    throw "Use either -DiffRange or -Staged, not both."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoRootUnix = $repoRoot.Replace("\", "/")

$gitBaseArgs = @("-c", "safe.directory=$repoRootUnix", "-C", $repoRoot)

$changedFiles = @()
if ($Staged) {
    $changedFiles = @(git @gitBaseArgs diff --cached --name-only)
} else {
    $changedFiles = @(git @gitBaseArgs diff --name-only $DiffRange)
}

if ($changedFiles.Count -eq 0) {
    Write-Host "No changed files detected."
    exit 0
}

$blocked = @()
foreach ($path in $changedFiles) {
    if ($path -match "^projects/") {
        $blocked += $path
    }
}

if ($blocked.Count -gt 0) {
    Write-Host "The following game-specific paths are not allowed in UE engine-only changes:"
    $blocked | ForEach-Object { Write-Host " - $_" }
    throw "Engine/game boundary violation detected. Move these changes to the game repository."
}

Write-Host "Engine boundary check passed."
