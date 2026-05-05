param(
    [string]$RepositoryFullName,
    [string]$Branch = "main",
    [int]$RequiredApprovals = 1,
    [string[]]$RequiredStatusChecks = @(
        "EngineBoundaryGuard / guard",
        "DebugBuild / build",
        "DevelopBuild / build",
        "ReleaseBuild / build"
    ),
    [bool]$RequireCodeOwnerReviews = $true,
    [bool]$DismissStaleReviews = $true,
    [bool]$RequireConversationResolution = $true,
    [bool]$RequireLinearHistory = $true,
    [bool]$EnforceAdmins = $true,
    [switch]$Apply
)

function Resolve-RepositoryFullName {
    param([string]$InputFullName)

    if ($InputFullName) {
        return $InputFullName
    }

    $originUrl = git config --get remote.origin.url 2>$null
    if (-not $originUrl) {
        $repoRoot = (Resolve-Path ".").Path.Replace("\", "/")
        $originUrl = git -c "safe.directory=$repoRoot" config --get remote.origin.url 2>$null
    }
    if (-not $originUrl) {
        throw "RepositoryFullName not provided and remote.origin.url was not found."
    }

    if ($originUrl -match "^https://github\.com/([^/]+)/([^/]+?)(?:\.git)?$") {
        return "$($matches[1])/$($matches[2])"
    }
    if ($originUrl -match "^git@github\.com:([^/]+)/([^/]+?)(?:\.git)?$") {
        return "$($matches[1])/$($matches[2])"
    }

    throw "Unsupported remote.origin.url format: $originUrl"
}

$resolvedRepository = Resolve-RepositoryFullName -InputFullName $RepositoryFullName

$payload = @{
    required_status_checks = @{
        strict   = $true
        contexts = $RequiredStatusChecks
    }
    enforce_admins                   = $EnforceAdmins
    required_pull_request_reviews    = @{
        dismiss_stale_reviews           = $DismissStaleReviews
        require_code_owner_reviews      = $RequireCodeOwnerReviews
        required_approving_review_count = $RequiredApprovals
    }
    restrictions                     = $null
    required_linear_history          = $RequireLinearHistory
    allow_force_pushes               = $false
    allow_deletions                  = $false
    block_creations                  = $false
    required_conversation_resolution = $RequireConversationResolution
    lock_branch                      = $false
    allow_fork_syncing               = $true
}

Write-Host "Target repository: $resolvedRepository"
Write-Host "Target branch    : $Branch"
Write-Host ""
Write-Host "Branch protection payload:"
$payload | ConvertTo-Json -Depth 10

if (-not $Apply) {
    Write-Host ""
    Write-Host "Dry-run mode. No remote changes were applied."
    Write-Host "To apply, re-run with -Apply and set GITHUB_TOKEN in your environment."
    exit 0
}

if (-not $env:GITHUB_TOKEN) {
    throw "GITHUB_TOKEN is not set. Export a token with repo administration permission."
}

$headers = @{
    Accept                 = "application/vnd.github+json"
    Authorization          = "Bearer $($env:GITHUB_TOKEN)"
    "X-GitHub-Api-Version" = "2022-11-28"
}

$uri = "https://api.github.com/repos/$resolvedRepository/branches/$Branch/protection"
Write-Host ""
Write-Host "Applying branch protection..."
$null = Invoke-RestMethod -Method Put -Uri $uri -Headers $headers -Body ($payload | ConvertTo-Json -Depth 10)
Write-Host "Branch protection applied successfully."
