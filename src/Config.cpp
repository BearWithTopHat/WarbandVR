// SPDX-License-Identifier: GPL-3.0-only
#include "Config.h"
#include "Logger.h"
#include "ParseUtil.h"
#include "Comfort.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace
{
    Config g_cfg;

    void IniPath(char* out, size_t outSize)
    {
        HMODULE self = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&IniPath), &self);
        GetModuleFileNameA(self, out, static_cast<DWORD>(outSize));
        char* slash = strrchr(out, '\\');
        if (slash) slash[1] = '\0';
        strcat_s(out, outSize, "WarbandVR.ini");
    }

    bool GetBool(const char* path, const char* sec, const char* key, bool def)
    {
        return GetPrivateProfileIntA(sec, key, def ? 1 : 0, path) != 0;
    }

    float GetFloat(const char* path, const char* sec, const char* key, float def)
    {
        char buf[64], defBuf[64];
        snprintf(defBuf, sizeof(defBuf), "%g", def);
        GetPrivateProfileStringA(sec, key, defBuf, buf, sizeof(buf), path);
        return static_cast<float>(atof(buf));
    }

    // Parses decimal or 0x-prefixed hex.
    unsigned long GetULong(const char* path, const char* sec, const char* key, unsigned long def)
    {
        char buf[64], defBuf[64];
        snprintf(defBuf, sizeof(defBuf), "0x%lX", def);
        GetPrivateProfileStringA(sec, key, defBuf, buf, sizeof(buf), path);
        return Parse::ULong(buf, 0); // empty value parses as 0 (legacy behavior)
    }

    void GetStr(const char* path, const char* sec, const char* key,
                const char* def, char* out, size_t outSize)
    {
        GetPrivateProfileStringA(sec, key, def, out, static_cast<DWORD>(outSize), path);
    }

    // "W", "SPACE", "SHIFT", "F5" ... -> virtual-key code. Pure table first
    // (ParseUtil.h, unit-tested); VkKeyScanA remains the fallback for single
    // layout-dependent punctuation characters.
    int KeyFromString(const char* s)
    {
        int vk = Parse::KeyFromString(s);
        if (vk) return vk;
        if (s && s[0] && !s[1])
        {
            SHORT scan = VkKeyScanA(s[0]);
            return (scan == -1) ? 0 : (scan & 0xFF);
        }
        return 0;
    }

    int GetKey(const char* path, const char* key, int def)
    {
        char buf[32], defBuf[32] = "";
        if (def == VK_SPACE) strcpy_s(defBuf, "SPACE");
        else if (def >= 'A' && def <= 'Z') { defBuf[0] = (char)def; defBuf[1] = 0; }
        else if (def >= '0' && def <= '9') { defBuf[0] = (char)def; defBuf[1] = 0; }
        GetStr(path, "InputMap", key, defBuf, buf, sizeof(buf));
        int vk = KeyFromString(buf);
        return vk ? vk : def;
    }
}

Config& Config::Get() { return g_cfg; }

void Config::Load()
{
    char path[MAX_PATH];
    IniPath(path, sizeof(path));

    // Comfort presets load first; an explicit numeric [Stereo] WorldScale key
    // below remains the expert override.
    char presetDef[sizeof(worldScalePreset)];
    strcpy_s(presetDef, worldScalePreset);
    GetStr(path, "Comfort", "WorldScalePreset", presetDef, worldScalePreset, sizeof(worldScalePreset));
    float presetScale = 0.0f;
    if (Comfort::WorldScaleFromPreset(worldScalePreset, &presetScale))
        worldScale = presetScale;
    else if (worldScalePreset[0])
        LOG_WARN("Unknown WorldScalePreset='%s' (keeping %.3f)", worldScalePreset, worldScale);
    comfortVignette      = GetBool(path, "Comfort", "Vignette", comfortVignette);
    vignetteStrength     = Comfort::Clamp01(GetFloat(path, "Comfort", "VignetteStrength", vignetteStrength));

    enableStereo         = GetBool(path, "Stereo", "EnableStereo", enableStereo);
    ipd                  = GetFloat(path, "Stereo", "IPD", ipd);
    worldScale           = GetFloat(path, "Stereo", "WorldScale", worldScale);
    viewSource           = GetPrivateProfileIntA("Stereo", "ViewSource", viewSource, path);
    vsViewRegister       = GetPrivateProfileIntA("Stereo", "VSViewRegister", vsViewRegister, path);
    asymmetricProjection = GetBool(path, "Stereo", "AsymmetricProjection", asymmetricProjection);
    sbsMonoUI            = GetBool(path, "Stereo", "SBSMonoUI", sbsMonoUI);
    uiMaxTriangles       = GetPrivateProfileIntA("Stereo", "UIMaxTriangles", uiMaxTriangles, path);
    uiZScaleMax          = GetFloat(path, "Stereo", "UIZScaleMax", uiZScaleMax);
    uiWScaleEps          = GetFloat(path, "Stereo", "UIWScaleEps", uiWScaleEps);
    uiRowNormMax         = GetFloat(path, "Stereo", "UIRowNormMax", uiRowNormMax);
    offscreenPasses      = GetBool(path, "Stereo", "OffscreenPasses", offscreenPasses);

    outputMode           = GetPrivateProfileIntA("Output", "OutputMode", outputMode, path);
    sharedSurface        = GetBool(path, "Output", "SharedSurface", sharedSurface);
    mirrorOnDesktop      = GetBool(path, "Output", "MirrorOnDesktop", mirrorOnDesktop);
    sbsAspectCorrect     = GetBool(path, "Output", "SBSAspectCorrect", sbsAspectCorrect);
    sbsMouseRemap        = GetBool(path, "Output", "SBSMouseRemap", sbsMouseRemap);
    forceNoVsync         = GetBool(path, "Output", "ForceNoVsync", forceNoVsync);
    cinemaMode           = GetBool(path, "Output", "CinemaMode", cinemaMode);
    framePacing          = GetPrivateProfileIntA("Output", "FramePacing", framePacing, path);
    sharedSyncMode       = GetPrivateProfileIntA("Output", "SharedSyncMode", sharedSyncMode, path);

    emulateControllers   = GetBool(path, "Input", "EmulateControllers", emulateControllers);
    motionMelee          = GetBool(path, "Input", "MotionMelee", motionMelee);
    swingThreshold       = GetFloat(path, "Input", "SwingThreshold", swingThreshold);
    blockHoldTime        = GetFloat(path, "Input", "BlockHoldTime", blockHoldTime);
    blockForearm         = GetBool(path, "Input", "BlockForearm", blockForearm);
    haptics              = GetBool(path, "Input", "Haptics", haptics);
    attackCooldownMs     = GetPrivateProfileIntA("Input", "AttackCooldownMs", attackCooldownMs, path);
    mouseSpeed           = GetFloat(path, "Input", "MouseSpeed", mouseSpeed);
    stickDeadzone        = GetFloat(path, "Input", "StickDeadzone", stickDeadzone);
    motionSteer          = GetBool(path, "Input", "MotionSteer", motionSteer);
    steerScale           = GetFloat(path, "Input", "SteerScale", steerScale);
    steerMaxDeltaPx      = GetPrivateProfileIntA("Input", "SteerMaxDeltaPx", steerMaxDeltaPx, path);
    steerMaxHoldMs       = GetPrivateProfileIntA("Input", "SteerMaxHoldMs", steerMaxHoldMs, path);
    gestureGrab          = GetBool(path, "Input", "GestureGrab", gestureGrab);
    grabRefractoryMs     = GetPrivateProfileIntA("Input", "GrabRefractoryMs", grabRefractoryMs, path);
    attackHoldTrigger    = GetBool(path, "Input", "AttackHoldTrigger", attackHoldTrigger);
    snapTurnDegrees      = GetFloat(path, "Input", "SnapTurnDegrees", snapTurnDegrees);
    recenterHoldSeconds  = GetFloat(path, "Input", "RecenterHoldSeconds", recenterHoldSeconds);
    {
        char presetDef[sizeof(controllerPreset)];
        strcpy_s(presetDef, controllerPreset);
        GetStr(path, "Input", "ControllerPreset", presetDef, controllerPreset, sizeof(controllerPreset));
        if (controllerPreset[0])
        {
            // A preset section only overrides the keys it actually sets; the
            // base [Input] values above remain for everything else.
            char section[40];
            snprintf(section, sizeof(section), "Preset.%s", controllerPreset);
            swingThreshold = GetFloat(path, section, "SwingThreshold", swingThreshold);
            steerScale     = GetFloat(path, section, "SteerScale", steerScale);
            blockHoldTime  = GetFloat(path, section, "BlockHoldTime", blockHoldTime);
            LOG_INFO("Controller preset '%s' applied (section [%s])", controllerPreset, section);
        }
    }

    keyForward           = GetKey(path, "Forward", keyForward);
    keyBack              = GetKey(path, "Back", keyBack);
    keyLeft              = GetKey(path, "Left", keyLeft);
    keyRight             = GetKey(path, "Right", keyRight);
    keyUse               = GetKey(path, "Use", keyUse);
    keyJump              = GetKey(path, "Jump", keyJump);
    keyGrabHipL          = GetKey(path, "GrabHipLeft", keyGrabHipL);
    keyGrabHipR          = GetKey(path, "GrabHipRight", keyGrabHipR);
    keyGrabShoulder      = GetKey(path, "GrabShoulder", keyGrabShoulder);
    keyGrabQuiver        = GetKey(path, "GrabQuiver", keyGrabQuiver);
    keyTwoHand           = GetKey(path, "TwoHand", keyTwoHand);
    keyHolster           = GetKey(path, "Holster", keyHolster);

    headTracking         = GetBool(path, "Tracking", "HeadTracking", headTracking);
    trackingSmoothing    = GetBool(path, "Tracking", "Smoothing", trackingSmoothing);
    smoothingMinCutoff   = GetFloat(path, "Tracking", "SmoothingMinCutoff", smoothingMinCutoff);
    smoothingBeta        = GetFloat(path, "Tracking", "SmoothingBeta", smoothingBeta);
    recenterKey          = static_cast<int>(GetULong(path, "Tracking", "RecenterKey",
                                                     static_cast<unsigned long>(recenterKey)));
    lockInMenus          = GetBool(path, "Tracking", "LockInMenus", lockInMenus);
    invertYaw            = GetBool(path, "Tracking", "InvertYaw", invertYaw);
    invertPitch          = GetBool(path, "Tracking", "InvertPitch", invertPitch);
    invertRoll           = GetBool(path, "Tracking", "InvertRoll", invertRoll);
    invertPosX           = GetBool(path, "Tracking", "InvertPosX", invertPosX);
    invertPosZ           = GetBool(path, "Tracking", "InvertPosZ", invertPosZ);

    GetStr(path, "Camera", "GameBuild", "", gameBuild, sizeof(gameBuild));

    logLevel             = GetPrivateProfileIntA("Debug", "LogLevel", logLevel, path);
    logViewMatrices      = GetPrivateProfileIntA("Debug", "LogViewMatrices", logViewMatrices, path);
    captureMode          = GetPrivateProfileIntA("Debug", "CaptureMode", captureMode, path);
    dumpFrames           = GetPrivateProfileIntA("Debug", "DumpFrames", dumpFrames, path);
    noZTest              = GetBool(path, "Debug", "NoZTest", noZTest);
    cameraScan           = GetBool(path, "Debug", "CameraScan", cameraScan);
    scanBase             = GetULong(path, "Debug", "ScanBase", 0);
    scanSize             = GetULong(path, "Debug", "ScanSize", 0);

    char posBuf[128];
    GetStr(path, "Debug", "ExpectedPos", "", posBuf, sizeof(posBuf));
    if (Parse::Vec3(posBuf, expectedPos))
    {
        hasExpectedPos = true;
    }

    Log::SetLevel(logLevel);
    LOG_INFO("Config loaded from %s", path);
    LOG_INFO("  EnableStereo=%d IPD=%.3f WorldScale=%.2f ViewSource=%d VSReg=%d AsymProj=%d OutputMode=%d Offscreen=%d",
             enableStereo ? 1 : 0, ipd, worldScale, viewSource, vsViewRegister,
             asymmetricProjection ? 1 : 0, outputMode, offscreenPasses ? 1 : 0);
    LOG_INFO("  HeadTracking=%d CaptureMode=%d CameraScan=%d GameBuild='%s'",
             headTracking ? 1 : 0, captureMode, cameraScan ? 1 : 0, gameBuild);
    LOG_INFO("  Comfort: Vignette=%d Strength=%.2f WorldScalePreset=%s",
             comfortVignette ? 1 : 0, vignetteStrength, worldScalePreset);
}
