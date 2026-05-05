param(
    [string]$ProjectsRoot = ""
)

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

$premakeArgs = @("--games=teamgame")
$resolvedProjectsRoot = ""
if (-not [string]::IsNullOrWhiteSpace($ProjectsRoot)) {
    $resolvedProjectsRoot = $ProjectsRoot
} elseif (-not [string]::IsNullOrWhiteSpace($env:UNNAMED_GAME_PROJECTS_ROOT)) {
    $resolvedProjectsRoot = $env:UNNAMED_GAME_PROJECTS_ROOT
}

if ([string]::IsNullOrWhiteSpace($resolvedProjectsRoot)) {
    Write-Host "Projects root was not specified."
    Write-Host "Pass -ProjectsRoot <path> or set UNNAMED_GAME_PROJECTS_ROOT."
    exit 1
}

$teamGameRuntimeDir = Join-Path $resolvedProjectsRoot "TeamGame/runtime"
if (-not (Test-Path -LiteralPath $teamGameRuntimeDir -PathType Container)) {
    Write-Host "TeamGame runtime was not found: $teamGameRuntimeDir"
    Write-Host "Point -ProjectsRoot to the game repository's projects directory."
    exit 1
}

$premakeArgs += "--projects-root=$resolvedProjectsRoot"
Write-Host "Using external projects root: $resolvedProjectsRoot"
$premakeArgs += "vs2026"

& $premakeCommand @premakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "Premake generation failed."
    exit $LASTEXITCODE
}

Write-Host "TeamGame-only project generation completed."
