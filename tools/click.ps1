# SPDX-License-Identifier: GPL-3.0-only
# Clicks absolute virtual-screen coordinates (multi-monitor safe).
param(
    [Parameter(Mandatory = $true)][int] $X,
    [Parameter(Mandatory = $true)][int] $Y,
    [int] $DelayMs = 300
)
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class MouseSender
{
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
"@
[MouseSender]::SetCursorPos($X, $Y) | Out-Null
Start-Sleep -Milliseconds 200
[MouseSender]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)  # LEFTDOWN
Start-Sleep -Milliseconds 60
[MouseSender]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)  # LEFTUP
Write-Host "clicked ($X, $Y)"
Start-Sleep -Milliseconds $DelayMs
