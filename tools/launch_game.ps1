# SPDX-License-Identifier: GPL-3.0-only
# tools/launch_game.ps1
# Launches mb_warband.exe and clicks through the Warband launcher dialog so
# automated test runs reach the actual game. The launcher is a #32770 dialog
# titled "Mount&Blade Warband"; its Play button is dialog control ID 1029
# (same technique as the WarbandX launcher).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\launch_game.ps1 -GameDir "<folder with mb_warband.exe>"
#
# The script returns as soon as Play has been clicked (the game keeps
# running). Use -TimeoutSec to bound the wait for the launcher window.

param(
    [Parameter(Mandatory = $true)]
    [string] $GameDir,

    [int] $TimeoutSec = 60
)

$exe = Join-Path $GameDir "mb_warband.exe"
if (-not (Test-Path $exe)) {
    Write-Error "mb_warband.exe not found at $exe"
    exit 1
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class WarbandLauncherInterop
{
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr SendMessageW(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

$process = Start-Process -FilePath $exe -WorkingDirectory $GameDir -PassThru
Write-Host "Started mb_warband.exe (pid $($process.Id)); waiting for launcher dialog..."

$WM_LBUTTONDOWN = 0x0201
$WM_LBUTTONUP   = 0x0202
$MK_LBUTTON     = [IntPtr]1
$IDC_PLAY       = 1029

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$playButton = [IntPtr]::Zero
while ((Get-Date) -lt $deadline) {
    $dialog = [WarbandLauncherInterop]::FindWindowW("#32770", "Mount&Blade Warband")
    if ($dialog -ne [IntPtr]::Zero) {
        $playButton = [WarbandLauncherInterop]::GetDlgItem($dialog, $IDC_PLAY)
        if ($playButton -ne [IntPtr]::Zero) { break }
    }
    if ($process.HasExited) {
        Write-Error "mb_warband.exe exited before the launcher appeared"
        exit 1
    }
    Start-Sleep -Milliseconds 100
}

if ($playButton -eq [IntPtr]::Zero) {
    Write-Error "Timed out waiting for the launcher Play button"
    exit 1
}

[WarbandLauncherInterop]::SendMessageW($playButton, $WM_LBUTTONDOWN, $MK_LBUTTON, [IntPtr]::Zero) | Out-Null
[WarbandLauncherInterop]::SendMessageW($playButton, $WM_LBUTTONUP,   $MK_LBUTTON, [IntPtr]::Zero) | Out-Null
Write-Host "Play clicked; game is starting (pid $($process.Id))"
