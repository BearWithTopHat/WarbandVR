# SPDX-License-Identifier: GPL-3.0-only
# tools/setup_openxr.ps1
# One-shot OpenXR build setup for headset testing (milestone M3).
#
# Does everything that can be scripted:
#   1. clones the pinned Khronos OpenXR-SDK source (matches CI's OPENXR_TAG)
#   2. builds the x86 openxr_loader with cmake (the one manual prerequisite)
#   3. stages external/openxr/{include,lib} so the Release-OpenXR config builds
#   4. builds Release-OpenXR|Win32
#   5. deploys d3d9.dll + WarbandVR.ini + x86 openxr_loader.dll to the game
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\setup_openxr.ps1 -GameDir "<folder with mb_warband.exe>"
#
# Prerequisite it cannot self-install: cmake (winget install Kitware.CMake).
# SteamVR still must be run once to register itself as the active OpenXR
# runtime, and Steam Link pairing to the Frame stays manual.

param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [string]$OpenXrTag = "release-1.1.63"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$srcDir   = Join-Path $repo "external\openxr-src"
$buildDir = Join-Path $repo "external\openxr-build"
$stageDir = Join-Path $repo "external\openxr"

# --- 1. cmake ----------------------------------------------------------------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "cmake not found; attempting 'winget install Kitware.CMake'..."
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        Write-Error "cmake and winget both unavailable. Install cmake manually (https://cmake.org/download) and re-run."
    }
    winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Error "cmake was installed but isn't on PATH for this shell. Open a NEW shell (or sign out/in) and re-run."
    }
}
Write-Host "cmake: $($cmake.Source)"

# --- 2. OpenXR SDK source ----------------------------------------------------
if (-not (Test-Path (Join-Path $srcDir "CMakeLists.txt"))) {
    Write-Host "Cloning OpenXR-SDK ($OpenXrTag)..."
    git clone --depth 1 --branch $OpenXrTag https://github.com/KhronosGroup/OpenXR-SDK.git $srcDir
} else {
    Write-Host "OpenXR-SDK source already present at $srcDir"
}

# --- 3. x86 loader via cmake --------------------------------------------------
$loaderLib = Join-Path $buildDir "src\loader\Release\openxr_loader.lib"
if (-not (Test-Path $loaderLib)) {
    Write-Host "Configuring + building x86 openxr_loader..."
    & cmake -S $srcDir -B $buildDir -A Win32 -DDYNAMIC_LOADER=ON
    if ($LASTEXITCODE) { Write-Error "cmake configure failed ($LASTEXITCODE)" }
    & cmake --build $buildDir --config Release --target openxr_loader --parallel
    if ($LASTEXITCODE) { Write-Error "cmake build failed ($LASTEXITCODE)" }
}
if (-not (Test-Path $loaderLib)) {
    # layout can vary by generator; hunt for it
    $found = Get-ChildItem $buildDir -Recurse -Filter openxr_loader.lib -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $loaderLib = $found.FullName }
}
if (-not (Test-Path $loaderLib)) { Write-Error "openxr_loader.lib was not produced under $buildDir" }
Write-Host "loader lib: $loaderLib"

# --- 4. stage external/openxr/{include,lib} -----------------------------------
New-Item -ItemType Directory -Force -Path (Join-Path $stageDir "include"), (Join-Path $stageDir "lib") | Out-Null
Copy-Item -Recurse -Force (Join-Path $srcDir "include\*") (Join-Path $stageDir "include\")
Copy-Item -Force $loaderLib (Join-Path $stageDir "lib\openxr_loader.lib")
Write-Host "staged SDK layout at $stageDir"

# --- 5. build Release-OpenXR|Win32 --------------------------------------------
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $found = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue |
             Where-Object { $_.FullName -like "*Current\Bin\MSBuild.exe" } | Select-Object -First 1
    if ($found) { $msbuild = $found.FullName }
}
if (-not (Test-Path $msbuild)) { Write-Error "MSBuild not found (need VS2022)" }
Write-Host "Building Release-OpenXR|Win32..."
& $msbuild (Join-Path $repo "WarbandVR.sln") -p:Configuration=Release-OpenXR -p:Platform=Win32 -m -v:m -nologo
if ($LASTEXITCODE) { Write-Error "MSBuild failed ($LASTEXITCODE)" }

# --- 6. deploy ----------------------------------------------------------------
$builtDll = Join-Path $repo "Release-OpenXR\d3d9.dll"
if (-not (Test-Path $builtDll)) { Write-Error "expected $builtDll after the build" }

# deploy.ps1 copies the proxy + ini; we additionally copy the x86 loader dll.
powershell -ExecutionPolicy Bypass -File (Join-Path $repo "tools\deploy.ps1") -GameDir $GameDir -BuildConfig Release-OpenXR
$loaderDll = Get-ChildItem $buildDir -Recurse -Filter openxr_loader.dll -ErrorAction SilentlyContinue | Select-Object -First 1
if ($loaderDll) {
    Copy-Item -Force $loaderDll.FullName (Join-Path $GameDir "openxr_loader.dll")
    Write-Host "copied openxr_loader.dll -> $GameDir"
} else {
    Write-Warning "openxr_loader.dll not found under $buildDir - copy the x86 loader dll to the game folder manually."
}

Write-Host ""
Write-Host "OpenXR setup complete. Remaining manual steps:"
Write-Host "  1. Run SteamVR once (it registers itself as the active OpenXR runtime)."
Write-Host "  2. In WarbandVR.ini: EnableStereo=1, OutputMode=1."
Write-Host "  3. Launch; check WarbandVR_log.txt for the xrGetD3D11GraphicsRequirementsKHR line."
Write-Host "  4. Then HeadTracking=1; SharedSurface=1 only if the slow path underperforms."
