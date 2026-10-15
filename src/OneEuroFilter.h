// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// OneEuroFilter: jitter-killing low-pass for noisy signals at fixed rate,
// with low lag during fast motion. Standard implementation (Casiez et al.)
// used for M4 head-pose smoothing.
//
// Usage: OneEuro f(72.0 /*update Hz*/, 1.0 /*minCutoff*/, 0.007 /*beta*/);
//        float smooth = f.Filter(rawYaw, dtSeconds);

#include <cmath>
#include <algorithm>

class OneEuro
{
public:
    OneEuro(double freq, double minCutoff = 1.0, double beta = 0.0, double dCutoff = 1.0)
        : m_freq(freq), m_minCutoff(minCutoff), m_beta(beta), m_dCutoff(dCutoff)
    {
    }

    float Filter(float x, double dt)
    {
        if (dt <= 0.0) dt = 1.0 / m_freq;

        double dx = m_hasLast ? (x - m_xPrev) / dt : 0.0;
        const double aDeriv = Alpha(m_dCutoff, dt);
        double edx = aDeriv * dx + (1.0 - aDeriv) * m_dxPrev;
        double cutoff = m_minCutoff + m_beta * std::fabs(edx);
        double a = Alpha(cutoff, dt);
        float result = static_cast<float>(a * x + (1.0 - a) * (m_hasLast ? m_xPrev : x));

        m_xPrev = result;
        m_dxPrev = static_cast<float>(edx);
        m_hasLast = true;
        return result;
    }

    void Reset() { m_hasLast = false; m_xPrev = 0.0f; m_dxPrev = 0.0f; }

private:
    double Alpha(double cutoff, double dt) const
    {
        double tau = 1.0 / (2.0 * 3.14159265358979323846 * cutoff);
        return 1.0 / (1.0 + tau / dt);
    }

    double m_freq, m_minCutoff, m_beta, m_dCutoff;
    float  m_xPrev = 0.0f, m_dxPrev = 0.0f;
    bool   m_hasLast = false;
};
