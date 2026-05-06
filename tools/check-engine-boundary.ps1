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

$changedEntries = @()
if ($Staged) {
    $changedEntries = @(git @gitBaseArgs diff --cached --name-status)
} else {
    $changedEntries = @(git @gitBaseArgs diff --name-status $DiffRange)
}

if ($changedEntries.Count -eq 0) {
    Write-Host "No changed files detected."
    exit 0
}

$blocked = @()
foreach ($entry in $changedEntries) {
    if ([string]::IsNullOrWhiteSpace($entry)) {
        continue
    }

    $tokens = $entry -split "`t"
    if ($tokens.Count -lt 2) {
        continue
    }

    $status = $tokens[0]
    $path = $tokens[1]
    if ($path -match "^projects/") {
        $allowDeleteForSeparation =
            $status -like "D*" -and (
                $path -match "^projects/[^/]+/runtime/" -or
                $path -match "^projects/[^/]+/content/" -or
                $path -match "^projects/[^/]+/config/game_profile\.json$"
            )
        if (
            $allowDeleteForSeparation
        ) {
            continue
        }
        $blocked += $path
    }
}

if ($blocked.Count -gt 0) {
    Write-Host "The following game-specific paths are not allowed in UE engine-only changes:"
    $blocked | ForEach-Object { Write-Host " - $_" }
    throw "Engine/game boundary violation detected. Move these changes to the game repository."
}

Write-Host "Engine boundary check passed."
