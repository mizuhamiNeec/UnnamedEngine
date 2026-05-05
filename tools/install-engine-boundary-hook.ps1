$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$hooksPath = ".githooks"
$hookDir = Join-Path $repoRoot $hooksPath

if (-not (Test-Path -LiteralPath $hookDir -PathType Container)) {
    New-Item -ItemType Directory -Path $hookDir | Out-Null
}

git -c "safe.directory=$($repoRoot.Replace('\', '/'))" config core.hooksPath $hooksPath
if ($LASTEXITCODE -ne 0) {
    throw "Failed to set core.hooksPath to $hooksPath"
}

Write-Host "Installed git hooks path: $hooksPath"
Write-Host "pre-commit guard is now active for this repository."
