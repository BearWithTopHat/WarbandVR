// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// ParseUtil: pure string-parsing helpers for Config. No Windows headers -
// host-compilable and unit-tested (tests/test_main.cpp). File I/O
// (GetPrivateProfile*) stays in Config.cpp behind this pure core.
//
// VK codes produced here follow the fixed VK_* numbering (0x30-0x39 digits,
// 0x41-0x5A letters, VK_F1=0x70 ...). VkKeyScanA agrees for alphanumerics on
// any Latin layout; punctuation VKs are layout-dependent, so Config falls
// back to VkKeyScanA for single non-alnum characters.

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>

namespace Parse
{
    // "W", "SPACE", "SHIFT", "F5" ... -> virtual-key code, 0 if unknown.
    inline int KeyFromString(const char* s)
    {
        if (!s || !s[0]) return 0;
        char up[32];
        size_t n = strlen(s);
        if (n >= sizeof(up)) n = sizeof(up) - 1;
        for (size_t i = 0; i < n; ++i) up[i] = (char)toupper((unsigned char)s[i]);
        up[n] = '\0';
        if (n == 1)
        {
            if (up[0] >= 'A' && up[0] <= 'Z') return up[0];
            if (up[0] >= '0' && up[0] <= '9') return up[0];
            return 0; // punctuation: layout-dependent, caller falls back
        }
        if (strcmp(up, "SPACE") == 0) return 0x20;   // VK_SPACE
        if (strcmp(up, "SHIFT") == 0) return 0x10;   // VK_SHIFT
        if (strcmp(up, "CTRL") == 0 || strcmp(up, "CONTROL") == 0) return 0x11; // VK_CONTROL
        if (strcmp(up, "ALT") == 0) return 0x12;     // VK_MENU
        if (strcmp(up, "TAB") == 0) return 0x09;     // VK_TAB
        if (strcmp(up, "ESC") == 0 || strcmp(up, "ESCAPE") == 0) return 0x1B; // VK_ESCAPE
        if (strcmp(up, "ENTER") == 0 || strcmp(up, "RETURN") == 0) return 0x0D; // VK_RETURN
        if (up[0] == 'F' && n >= 2) // F1..F12
        {
            int k = atoi(up + 1);
            if (k >= 1 && k <= 12) return 0x70 + (k - 1); // VK_F1
        }
        return 0;
    }

    // Parses decimal or 0x-prefixed hex.
    inline unsigned long ULong(const char* s, unsigned long def)
    {
        if (!s || !s[0]) return def;
        return strtoul(s, nullptr, 0);
    }

    // "x , y , z" -> true and fills out[3]; false if not three floats.
    inline bool Vec3(const char* s, float* out)
    {
        if (!s || !*s) return false;
        return sscanf(s, "%f , %f , %f", &out[0], &out[1], &out[2]) == 3;
    }
}
