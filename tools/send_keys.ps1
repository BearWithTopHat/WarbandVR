# SPDX-License-Identifier: GPL-3.0-only
# Sends keystrokes to the Warband window via SendInput with scan codes
# (hardware-level injection that DirectInput games accept).
param(
    [Parameter(Mandatory = $true)][string] $Keys,
    [int] $DelayMs = 400
)
# Keys: comma-separated list: ESC, RETURN, UP, DOWN, LEFT, RIGHT, SPACE, TAB, F8...
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class ScanSender
{
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public INPUTUNION u; }
    [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION { [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public UIntPtr dwExtraInfo; }
    [DllImport("user32.dll")] public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    [DllImport("user32.dll")] public static extern ushort MapVirtualKeyW(uint uCode, uint uMapType);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);

    public static void Tap(ushort vk)
    {
        ushort scan = MapVirtualKeyW(vk, 0);
        INPUT[] down = new INPUT[1];
        down[0].type = 1; // INPUT_KEYBOARD
        down[0].u.ki.wVk = 0;
        down[0].u.ki.wScan = scan;
        down[0].u.ki.dwFlags = 0x0008; // KEYEVENTF_SCANCODE
        INPUT[] up = new INPUT[1];
        up[0].type = 1;
        up[0].u.ki.wVk = 0;
        up[0].u.ki.wScan = scan;
        up[0].u.ki.dwFlags = 0x0008 | 0x0002; // SCANCODE | KEYUP
        SendInput(1, down, Marshal.SizeOf(typeof(INPUT)));
        System.Threading.Thread.Sleep(60);
        SendInput(1, up, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@
$vkMap = @{ ESC=0x1B; RETURN=0x0D; SPACE=0x20; UP=0x26; DOWN=0x28; LEFT=0x25; RIGHT=0x27; TAB=0x09; F8=0x77 }
$proc = Get-Process mb_warband -ErrorAction SilentlyContinue
if ($proc) {
    $ws = New-Object -ComObject WScript.Shell
    $ws.AppActivate($proc.Id) | Out-Null
    Start-Sleep -Milliseconds 800
    [ScanSender]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null
}
Start-Sleep -Milliseconds 500
foreach ($k in $Keys.Split(',')) {
    $k = $k.Trim()
    $vk = if ($vkMap.ContainsKey($k)) { $vkMap[$k] } else { [Convert]::ToInt32($k, 16) }
    [ScanSender]::Tap([uint16]$vk)
    Write-Host "sent $k (0x$('{0:X2}' -f $vk))"
    Start-Sleep -Milliseconds $DelayMs
}
