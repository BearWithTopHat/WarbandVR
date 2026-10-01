#include "CameraScan.h"
#include "Config.h"
#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    struct Candidate
    {
        unsigned char* addr;
        float last[3];
        int   score;
        int   changes;
    };

    std::vector<Candidate> g_candidates;
    unsigned long long g_lastScanTick = 0;
    bool  g_seeded = false;

    const float kExpectedEps = 0.5f;    // match tolerance vs ExpectedPos
    const float kMoveEps     = 0.01f;   // "the value moved" threshold
    const size_t kMaxCandidates = 4096;

    // Resolve the exe's .data section as the default scan range.
    bool GetExeDataSection(unsigned char*& base, size_t& size)
    {
        unsigned char* exe = reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
        if (!exe) return false;

        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        IMAGE_NT_HEADERS32* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(exe + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
        {
            if (memcmp(sec->Name, ".data", 5) == 0)
            {
                base = exe + sec->VirtualAddress;
                size = sec->Misc.VirtualSize;
                return true;
            }
        }
        return false;
    }

    bool TripleMatches(const float* f, const float* expected)
    {
        return std::fabs(f[0] - expected[0]) < kExpectedEps &&
               std::fabs(f[1] - expected[1]) < kExpectedEps &&
               std::fabs(f[2] - expected[2]) < kExpectedEps;
    }

    // Iterate committed, readable pages in [base, base+size) and seed/prune.
    void ScanPass(unsigned char* base, size_t size, const float* expected)
    {
        unsigned char* addr = base;
        unsigned char* end  = base + size;

        while (addr < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) break;

            bool readable = (mbi.State == MEM_COMMIT) &&
                            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                                            PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                            PAGE_EXECUTE_READWRITE)) &&
                            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));

            unsigned char* regionEnd = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
            if (regionEnd > end) regionEnd = end;

            if (readable)
            {
                // Float-align scan; triples, so step 4 bytes.
                for (unsigned char* p = addr; p + 12 <= regionEnd; p += 4)
                {
                    const float* f = reinterpret_cast<const float*>(p);
                    if (!TripleMatches(f, expected)) continue;
                    if (g_candidates.size() >= kMaxCandidates)
                    {
                        LOG_WARN("[CAMERASCAN] hit candidate cap (%zu) - narrow ExpectedPos or ScanSize",
                                 kMaxCandidates);
                        return;
                    }
                    Candidate c{};
                    c.addr = p;
                    memcpy(c.last, f, sizeof(c.last));
                    c.score = 0; c.changes = 0;
                    g_candidates.push_back(c);
                }
            }
            addr = regionEnd;
        }
    }

    void RescorePass()
    {
        for (size_t i = 0; i < g_candidates.size();)
        {
            Candidate& c = g_candidates[i];

            // Revalidate the page before dereferencing.
            MEMORY_BASIC_INFORMATION mbi{};
            bool readable = VirtualQuery(c.addr, &mbi, sizeof(mbi)) != 0 &&
                            mbi.State == MEM_COMMIT &&
                            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
            if (!readable) { g_candidates.erase(g_candidates.begin() + i); continue; }

            float now[3];
            memcpy(now, c.addr, sizeof(now));

            bool finite = std::isfinite(now[0]) && std::isfinite(now[1]) && std::isfinite(now[2]);
            if (!finite) { g_candidates.erase(g_candidates.begin() + i); continue; }

            bool moved = std::fabs(now[0] - c.last[0]) > kMoveEps ||
                         std::fabs(now[1] - c.last[1]) > kMoveEps ||
                         std::fabs(now[2] - c.last[2]) > kMoveEps;
            if (moved) { c.score += 2; ++c.changes; }
            else       { c.score -= 1; }

            memcpy(c.last, now, sizeof(now));

            if (c.score < -4) { g_candidates.erase(g_candidates.begin() + i); continue; }
            ++i;
        }
    }

    void Report()
    {
        // Log the 20 best-scoring candidates.
        std::vector<size_t> idx(g_candidates.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [](size_t a, size_t b) {
            return g_candidates[a].score > g_candidates[b].score;
        });

        LOG_INFO("[CAMERASCAN] %zu candidates alive; top:", g_candidates.size());
        for (size_t i = 0; i < idx.size() && i < 20; ++i)
        {
            const Candidate& c = g_candidates[idx[i]];
            LOG_INFO("  %p  score=%-4d changes=%-3d  (%.2f, %.2f, %.2f)",
                     c.addr, c.score, c.changes, c.last[0], c.last[1], c.last[2]);
        }
    }
}

void CameraScan::Init()
{
    // Nothing to do eagerly; range resolution happens lazily on first frame.
}

void CameraScan::OnFrame()
{
    const Config& cfg = Config::Get();
    if (!cfg.cameraScan || !cfg.hasExpectedPos) return;

    unsigned long long now = GetTickCount64();
    if (now - g_lastScanTick < 500) return;
    g_lastScanTick = now;

    unsigned char* base = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(cfg.scanBase));
    size_t size = cfg.scanSize;
    if (!base || size == 0)
    {
        if (!GetExeDataSection(base, size))
        {
            LOG_ERROR("[CAMERASCAN] could not resolve exe .data section; set ScanBase/ScanSize");
            return;
        }
    }

    if (!g_seeded)
    {
        LOG_INFO("[CAMERASCAN] seeding: scanning %p..%p for (%.2f, %.2f, %.2f)",
                 base, base + size, cfg.expectedPos[0], cfg.expectedPos[1], cfg.expectedPos[2]);
        ScanPass(base, size, cfg.expectedPos);
        g_seeded = true;
        LOG_INFO("[CAMERASCAN] seeded %zu candidates - now WALK AROUND in-game", g_candidates.size());
        return;
    }

    RescorePass();
    Report();

    if (g_candidates.size() <= 20)
        LOG_INFO("[CAMERASCAN] converged to %zu candidates - copy these into docs/04 and confirm in Cheat Engine",
                 g_candidates.size());
}

void CameraScan::Shutdown()
{
    g_candidates.clear();
    g_seeded = false;
}
