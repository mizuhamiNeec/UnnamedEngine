Write-Host "Generating TeamGame-only projects (--games=teamgame)..."

$localPremakePath = Join-Path -Path $PSScriptRoot -ChildPath "..\\premake5.exe"
$premakeCommand = $null

if (Test-Path -LiteralPath $localPremakePath -PathType Leaf) {
    $premakeCommand = (Resolve-Path $localPremakePath).Path
    Write-Host "Using local premake5.exe: $premakeCommand"
} else {
    $premakeFromPath = Get-Command -Name "premake5.exe" -ErrorAction SilentlyContinue
    if ($premakeFromPath) {
        $premakeCommand = $premakeFromPath.Source
        Write-Host "Using premake5.exe from PATH: $premakeCommand"
    }
}

if (-not $premakeCommand) {
    Write-Host "premake5.exe was not found in repo root or PATH."
    exit 1
}

& $premakeCommand --games=teamgame vs2026
if ($LASTEXITCODE -ne 0) {
    Write-Host "Premake generation failed."
    exit $LASTEXITCODE
}

Write-Host "TeamGame-only project generation completed."
