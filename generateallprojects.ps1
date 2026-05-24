Write-Host "Generating projects (default: Parkour-only --games=parkour)..."

$localPremakePath = Join-Path -Path $PSScriptRoot -ChildPath "premake5.exe"
$premakeCommand = $null

if (Test-Path -LiteralPath $localPremakePath -PathType Leaf) {
    $premakeCommand = $localPremakePath
    Write-Host "Using local premake5.exe: $premakeCommand"
} else {
    $premakeFromPath = Get-Command -Name "premake5.exe" -ErrorAction SilentlyContinue
    if ($premakeFromPath) {
        $premakeCommand = $premakeFromPath.Source
        Write-Host "Using premake5.exe from PATH: $premakeCommand"
    }
}

if (-not $premakeCommand) {
    Write-Host "premake5.exe was not found in this directory or in PATH."
    Read-Host "Press Enter to continue..."
    exit 1
}

try {
    & $premakeCommand --games=parkour vs2026

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Premake5 execution completed."
        $workspaceRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
        $projectRoot = Join-Path -Path $workspaceRoot -ChildPath "build/projects"
        $staleTeamGameDirs = @(
            (Join-Path -Path $projectRoot -ChildPath "TeamGameApp"),
            (Join-Path -Path $projectRoot -ChildPath "TeamGameRuntime"),
            (Join-Path -Path $projectRoot -ChildPath "TeamGameRuntimeDll")
        )

        foreach ($dir in $staleTeamGameDirs) {
            if (-not (Test-Path -LiteralPath $dir -PathType Container)) {
                continue
            }

            $resolvedDir = (Resolve-Path -LiteralPath $dir).Path
            if (-not $resolvedDir.StartsWith($workspaceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove stale TeamGame directory outside workspace: $resolvedDir"
            }

            Remove-Item -LiteralPath $resolvedDir -Recurse -Force
            Write-Host "Removed stale TeamGame project directory: $resolvedDir"
        }
    } else {
        Write-Host "An error occurred during Premake5 execution."
        Read-Host "Press Enter to continue..."
        exit 1
    }
} catch {
    Write-Host "Failed to execute premake5.exe. Please download the latest version of Premake and add it to your system PATH."
    Read-Host "Press Enter to continue..."
    exit 1
}
