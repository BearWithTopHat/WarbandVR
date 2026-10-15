// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Comfort helpers: pure policy/math for VR comfort options. No Windows,
// D3D9, or OpenXR headers, so these are directly unit-tested on the host.

#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace Comfort
{
    inline float Clamp01(float v)
    {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    // Human-friendly WorldScalePreset values. They set Config::worldScale
    // unless an explicit numeric WorldScale key is present in the ini.
    inline bool WorldScaleFromPreset(const char* name, float* out)
    {
        if (!name || !out) return false;
        char lowered[32] = {};
        size_t n = strlen(name);
        if (n >= sizeof(lowered)) return false;
        for (size_t i = 0; i < n; ++i)
            lowered[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
        struct Entry { const char* name; float scale; };
        static const Entry kEntries[] = {
            { "compact",   0.85f }, // smaller-feeling world / less head motion
            { "realistic", 1.00f },
            { "large",     1.15f },
            { "giant",     1.35f },
        };
        for (const Entry& e : kEntries)
        {
            if (strcmp(e.name, lowered) == 0)
            {
                *out = e.scale;
                return true;
            }
        }
        return false;
    }

    // Radial vignette alpha for a texture texel in [0,1]^2. Alpha is zero in
    // the central clear region, smoothsteps to strength at corners, and is
    // capped to keep peripheral detail visible.
    inline unsigned VignetteAlpha(float u, float v, float strength)
    {
        strength = Clamp01(strength);
        const float dx = u - 0.5f;
        const float dy = v - 0.5f;
        const float radius = sqrtf(dx * dx + dy * dy) / 0.70710678f;
        const float t = Clamp01((radius - 0.55f) / 0.45f);
        const float smooth = t * t * (3.0f - 2.0f * t);
        // Bounded by construction: smooth and strength are both in [0,1], so
        // alpha lands in [0,176] with no clamp needed.
        return static_cast<unsigned>(smooth * strength * 176.0f + 0.5f);
    }

    inline uint32_t VignettePixel(float u, float v, float strength)
    {
        return (VignetteAlpha(u, v, strength) << 24); // opaque-black RGB=0
    }
}
