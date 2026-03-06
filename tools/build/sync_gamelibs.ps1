param(
    [string]$GameLibsRepo = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$openQ4Root = [System.IO.Path]::GetFullPath((Join-Path $scriptDir "..\.."))

if ([string]::IsNullOrWhiteSpace($GameLibsRepo)) {
    $GameLibsRepo = Join-Path $openQ4Root "..\OpenQ4-GameLibs"
}

$gameLibsRoot = [System.IO.Path]::GetFullPath($GameLibsRepo)
if (-not (Test-Path $gameLibsRoot)) {
    throw "OpenQ4-GameLibs repository not found at '$gameLibsRoot'. Set OPENQ4_GAMELIBS_REPO or pass -GameLibsRepo."
}

if (Test-Path (Join-Path $openQ4Root "src\game")) {
    Write-Warning "OpenQ4 now consumes game sources directly from OpenQ4-GameLibs. In-repo mirror directory 'src\\game' should be removed."
}

Write-Host "sync_gamelibs.ps1 is deprecated. No files were copied."
Write-Host "Using OpenQ4-GameLibs at: $gameLibsRoot"

$global:LASTEXITCODE = 0
exit 0
