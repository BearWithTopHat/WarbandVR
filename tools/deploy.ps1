# SPDX-License-Identifier: GPL-3.0-only
# WarbandVR deploy script
# Copies the built proxy d3d9.dll + config next to mb_warband.exe.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -GameDir "C:\Steam\steamapps\common\MountBlade Warband"
#
param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [string]$BuildConfig = "Release",
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $GameDir "mb_warband.exe"
if (-not (Test-Path $exe)) {
    Write-Error "mb_warband.exe not found in '$GameDir'. Pass the folder that contains the game executable."
}

$targetDll = Join-Path $GameDir "d3d9.dll"
$targetIni = Join-Path $GameDir "WarbandVR.ini"

if ($Uninstall) {
    Remove-Item $targetDll -ErrorAction SilentlyContinue
    Remove-Item $targetIni -ErrorAction SilentlyContinue
    Write-Host "WarbandVR removed from $GameDir"
    exit 0
}

$built = Join-Path $PSScriptRoot "..\$BuildConfig\d3d9.dll"
if (-not (Test-Path $built)) {
    $built = Join-Path $PSScriptRoot "..\$BuildConfig\WarbandVR.dll"
}
if (-not (Test-Path $built)) {
    Write-Error "Built DLL not found. Build the solution first (msbuild WarbandVR.sln /p:Configuration=$BuildConfig /p:Platform=Win32)."
}

Copy-Item $built $targetDll -Force

# Only copy the ini if none exists, so we never clobber a tuned config.
if (-not (Test-Path $targetIni)) {
    Copy-Item (Join-Path $PSScriptRoot "..\WarbandVR.ini") $targetIni -Force
}

Write-Host "Deployed WarbandVR to $GameDir"
Write-Host "  d3d9.dll     -> proxy (stereo disabled by default; edit WarbandVR.ini)"
Write-Host "  WarbandVR.ini-> config"
Write-Host "Launch the game once and check WarbandVR_log.txt next to the exe."
