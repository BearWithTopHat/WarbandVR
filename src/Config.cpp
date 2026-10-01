#include "Config.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        return strtoul(buf, nullptr, 0);
    }

    void GetStr(const char* path, const char* sec, const char* key,
                const char* def, char* out, size_t outSize)
    {
        GetPrivateProfileStringA(sec, key, def, out, static_cast<DWORD>(outSize), path);
    }
}

Config& Config::Get() { return g_cfg; }

void Config::Load()
{
    char path[MAX_PATH];
    IniPath(path, sizeof(path));

    enableStereo         = GetBool(path, "Stereo", "EnableStereo", enableStereo);
    ipd                  = GetFloat(path, "Stereo", "IPD", ipd);
    worldScale           = GetFloat(path, "Stereo", "WorldScale", worldScale);
    viewSource           = GetPrivateProfileIntA("Stereo", "ViewSource", viewSource, path);
    vsViewRegister       = GetPrivateProfileIntA("Stereo", "VSViewRegister", vsViewRegister, path);
    vsMatrixIsViewProj   = GetBool(path, "Stereo", "VSMatrixIsViewProj", vsMatrixIsViewProj);
    asymmetricProjection = GetBool(path, "Stereo", "AsymmetricProjection", asymmetricProjection);
    sbsMonoUI            = GetBool(path, "Stereo", "SBSMonoUI", sbsMonoUI);
    uiMaxTriangles       = GetPrivateProfileIntA("Stereo", "UIMaxTriangles", uiMaxTriangles, path);
    offscreenPasses      = GetBool(path, "Stereo", "OffscreenPasses", offscreenPasses);

    outputMode           = GetPrivateProfileIntA("Output", "OutputMode", outputMode, path);

    headTracking         = GetBool(path, "Tracking", "HeadTracking", headTracking);
    trackingSmoothing    = GetBool(path, "Tracking", "Smoothing", trackingSmoothing);
    smoothingMinCutoff   = GetFloat(path, "Tracking", "SmoothingMinCutoff", smoothingMinCutoff);
    smoothingBeta        = GetFloat(path, "Tracking", "SmoothingBeta", smoothingBeta);
    recenterKey          = static_cast<int>(GetULong(path, "Tracking", "RecenterKey",
                                                     static_cast<unsigned long>(recenterKey)));
    lockInMenus          = GetBool(path, "Tracking", "LockInMenus", lockInMenus);

    GetStr(path, "Camera", "GameBuild", "", gameBuild, sizeof(gameBuild));

    logLevel             = GetPrivateProfileIntA("Debug", "LogLevel", logLevel, path);
    logViewMatrices      = GetPrivateProfileIntA("Debug", "LogViewMatrices", logViewMatrices, path);
    captureMode          = GetPrivateProfileIntA("Debug", "CaptureMode", captureMode, path);
    dumpFrames           = GetPrivateProfileIntA("Debug", "DumpFrames", dumpFrames, path);
    cameraScan           = GetBool(path, "Debug", "CameraScan", cameraScan);
    scanBase             = GetULong(path, "Debug", "ScanBase", 0);
    scanSize             = GetULong(path, "Debug", "ScanSize", 0);

    char posBuf[128];
    GetStr(path, "Debug", "ExpectedPos", "", posBuf, sizeof(posBuf));
    if (posBuf[0] &&
        sscanf_s(posBuf, "%f , %f , %f", &expectedPos[0], &expectedPos[1], &expectedPos[2]) == 3)
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
}
