# SPDX-License-Identifier: GPL-3.0-only
# tools/game_input.ps1
# Sends mouse input to the running fullscreen game via SendInput (more
# reliable than mouse_event for DirectInput games) after forcing the game
# window to the foreground. Coordinates are absolute desktop pixels.
#
# Usage:
#   -Action move  -X 1920 -Y 1080
#   -Action click -X 1920 -Y 1080          (move + left click)
#   -Action click -X 1 -Y 1 -Right         (right click)
#
# Exit code 0 = sent, 2 = game window not found.

param(
    [Parameter(Mandatory = $true)][ValidateSet('move', 'click')] [string] $Action,
    [Parameter(Mandatory = $true)][int] $X,
    [Parameter(Mandatory = $true)][int] $Y,
    [switch] $Right,
    [int] $SettleMs = 250
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class GameInput
{
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT
    {
        public int dx, dy;
        public uint mouseData, dwFlags, time;
        public IntPtr dwExtraInfo;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT
    {
        public uint type;
        public MOUSEINPUT mi;
    }
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int idx);
}
"@

$proc = Get-Process -Name "mb_warband" -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Host "mb_warband window not found"; exit 2 }
$hwnd = $proc.MainWindowHandle

# Foreground enforcement: restore if minimized, then claim foreground.
[GameInput]::ShowWindow($hwnd, 9) | Out-Null   # SW_RESTORE
[GameInput]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 150

# Normalized absolute coordinates for SendInput (0..65535 over the virtual
# screen). Primary-only assumption: the game runs fullscreen on the primary.
$vw = [GameInput]::GetSystemMetrics(78)  # SM_CXVIRTUALSCREEN
$vh = [GameInput]::GetSystemMetrics(79)  # SM_CYVIRTUALSCREEN
$vx = [GameInput]::GetSystemMetrics(76)  # SM_XVIRTUALSCREEN
$vy = [GameInput]::GetSystemMetrics(77)  # SM_YVIRTUALSCREEN
$nx = [int](($X - $vx) * 65535 / ($vw - 1))
$ny = [int](($Y - $vy) * 65535 / ($vh - 1))

$inputs = @()
$move = New-Object GameInput+INPUT
$move.type = 0
$move.mi.dx = $nx; $move.mi.dy = $ny
$move.mi.dwFlags = 0x8001  # MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
$inputs += $move

if ($Action -eq 'click') {
    $downFlags = 0x0002; $upFlags = 0x0004   # left
    if ($Right) { $downFlags = 0x0008; $upFlags = 0x0010 }
    $dn = New-Object GameInput+INPUT
    $dn.type = 0; $dn.mi.dwFlags = $downFlags
    $up = New-Object GameInput+INPUT
    $up.type = 0; $up.mi.dwFlags = $upFlags
    $inputs += $dn; $inputs += $up
}

# SetCursorPos as well: some games poll the cursor position directly.
[GameInput]::SetCursorPos($X, $Y) | Out-Null
$sent = [GameInput]::SendInput($inputs.Length, $inputs, [Runtime.InteropServices.Marshal]::SizeOf([type][GameInput+INPUT]))
if ($sent -ne $inputs.Length) { Write-Host "SendInput sent $sent of $($inputs.Length)"; exit 1 }
Write-Host "$Action at ($X, $Y) -> pid $($proc.Id)"
Start-Sleep -Milliseconds $SettleMs
exit 0
