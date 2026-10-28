// Modulation unit tests (LFOs, tempo sync, random walk, matrix). Build: clang++ -std=c++17 -O2 mod_test.cpp -o mod_test
#include "../Source/Modulation.h"
#include <cstdio>
#include <vector>
using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::vector<float> run(Lfo& l, double seconds, double dt, const HostTime& h) { std::vector<float> v; for (double t = 0; t < seconds; t += dt) v.push_back(l.tick(dt, h)); return v; }
static int upCrossings(const std::vector<float>& v) { int c = 0; for (size_t i = 1; i < v.size(); ++i) if (v[i - 1] < 0 && v[i] >= 0) ++c; return c; }

int main()
{
    HostTime ht;
    printf("[1] free-running LFO shapes: range, mean, rate\n");
    for (int sh = 0; sh < 4; ++sh)
    {
        Lfo l; l.shape = sh; l.rateHz = 2.0f; auto v = run(l, 10.0, 1.0 / 172.0, ht);
        float mn = 9, mx = -9; double mean = 0; for (float x : v) { mn = std::min(mn, x); mx = std::max(mx, x); mean += x; } mean /= v.size();
        CHECK(mn >= -1.001f && mx <= 1.001f && mx - mn > 1.8f && std::fabs(mean) < 0.05, "%s: range %.2f..%.2f, mean %.3f", lfoShapeNames()[sh], mn, mx, mean);
        if (sh != 3) CHECK(std::abs(upCrossings(v) - 20) <= 1, "%s: %d cycles in 10 s at 2 Hz", lfoShapeNames()[sh], upCrossings(v));
    }
    printf("[2] random shapes: deterministic, smooth vs stepped\n");
    {
        Lfo a, b, c; a.shape = b.shape = 4; c.shape = 5; a.seed = b.seed = c.seed = 7; a.rateHz = b.rateHz = c.rateHz = 1.0f;
        auto va = run(a, 12.0, 0.005, ht), vb = run(b, 12.0, 0.005, ht), vc = run(c, 12.0, 0.005, ht);
        CHECK(va == vb, "same seed -> identical output");
        double maxStep = 0; for (size_t i = 1; i < va.size(); ++i) maxStep = std::max(maxStep, (double) std::fabs(va[i] - va[i - 1]));
        CHECK(maxStep < 0.03, "smooth random is continuous (max step %.4f per 5 ms)", maxStep);
        int jumps = 0, changes = 0; for (size_t i = 1; i < vc.size(); ++i) { if (std::fabs(vc[i] - vc[i - 1]) > 1e-6f) ++changes; if (std::fabs(vc[i] - vc[i - 1]) > 0.05f) ++jumps; }
        CHECK(changes >= 10 && changes <= 14, "stepped random changes once per cycle (%d changes in 12 s at 1 Hz)", changes);
        Lfo d; d.shape = 4; d.seed = 99; d.rateHz = 1.0f; auto vd = run(d, 12.0, 0.005, ht); CHECK(vd != va, "different seed -> different output");
    }
    printf("[3] tempo sync\n");
    {
        for (double bpm : { 60.0, 120.0, 90.0 })
        {
            HostTime h; h.bpm = bpm; h.hasBpm = true;
            Lfo l; l.shape = 0; l.sync = true; l.div = 4;      // 1/4 note
            auto v = run(l, 16.0, 1.0 / 172.0, h);
            double expectCycles = 16.0 * bpm / 60.0;
            CHECK(std::fabs(upCrossings(v) - expectCycles) <= 1.01, "1/4 note at %.0f BPM: %d cycles in 16 s (expected %.0f)", bpm, upCrossings(v), expectCycles);
        }
        HostTime h; h.bpm = 120; h.hasBpm = true; h.playing = true; h.hasPpq = true;
        Lfo l; l.shape = 2; l.sync = true; l.div = 2;           // 1 bar = 4 beats, saw
        h.ppq = 0.0; float a = l.tick(0.01, h); h.ppq = 2.0; float b = l.tick(0.01, h); h.ppq = 4.0; float c = l.tick(0.01, h); h.ppq = 4.0 + 1.0; float d = l.tick(0.01, h);
        CHECK(std::fabs(a + 1.0f) < 0.01f && std::fabs(b) < 0.01f && std::fabs(c + 1.0f) < 0.01f && std::fabs(d + 0.5f) < 0.01f, "phase locks to host position: saw at 0 beats %.2f, 2 beats %.2f, 4 beats %.2f, 5 beats %.2f", a, b, c, d);
        h.ppq = 1000.0; float e1 = l.tick(0.01, h); h.ppq = 1000.0; float e2 = l.tick(0.01, h); CHECK(e1 == e2, "position-locked output is a pure function of the host position (jump/loop safe)");
        HostTime stopped; stopped.bpm = 100; stopped.hasBpm = true; stopped.playing = false; stopped.hasPpq = true; stopped.ppq = 8;
        Lfo f; f.shape = 0; f.sync = true; f.div = 4; auto vf = run(f, 12.0, 0.01, stopped); CHECK(std::abs(upCrossings(vf) - 20) <= 1, "stopped transport: free-runs at host tempo (%d cycles in 12 s at 100 BPM)", upCrossings(vf));
    }
    printf("[4] random walk\n");
    {
        Walk w1, w2; double mean = 0, var = 0; float mn = 9, mx = -9; std::vector<float> a;
        for (int i = 0; i < 172 * 600; ++i) { float v = w1.tick(1.0 / 172, 1.0f); a.push_back(v); mn = std::min(mn, v); mx = std::max(mx, v); mean += v; }
        mean /= a.size(); for (float v : a) var += (v - mean) * (v - mean); var /= a.size();
        CHECK(mn >= -1.0f && mx <= 1.0f && var > 0.02, "bounded %.2f..%.2f, spread (variance) %.3f over 10 minutes", mn, mx, var);
        bool same = true; for (size_t i = 0; i < 1000; ++i) if (w2.tick(1.0 / 172, 1.0f) != a[i]) same = false; CHECK(same, "deterministic");
        double maxStep = 0; for (size_t i = 1; i < a.size(); ++i) maxStep = std::max(maxStep, (double) std::fabs(a[i] - a[i - 1])); CHECK(maxStep < 0.05, "no sudden jumps (max step %.4f per control tick)", maxStep);
    }
    printf("[5] routing matrix\n");
    {
        float src[kNumSrc] = {}; for (int i = 1; i < kNumSrc; ++i) src[i] = 0.1f * i;
        ModSlot s[kModSlots]; float dst[kNumDst];
        s[0] = { SrcLfo1, DstCutoff, 0.5f }; evaluateMatrix(src, s, dst);
        CHECK(std::fabs(dst[DstCutoff] - 0.05f) < 1e-6f && dst[DstColour] == 0.0f, "one slot: LFO1 x 50%% -> Cutoff %.3f", dst[DstCutoff]);
        s[1] = { SrcLfo2, DstCutoff, -1.0f }; evaluateMatrix(src, s, dst); CHECK(std::fabs(dst[DstCutoff] - (0.05f - 0.2f)) < 1e-6f, "two slots to one target add: %.3f", dst[DstCutoff]);
        s[2] = { SrcNone, DstMorph, 1.0f }; s[3] = { SrcWalk, DstNone, 1.0f }; evaluateMatrix(src, s, dst);
        CHECK(dst[DstMorph] == 0.0f, "slots with source or target 'None' do nothing");
        bool finite = true; for (int a = 0; a < kNumSrc; ++a) for (int b = 0; b < kNumDst; ++b) { s[0] = { a, b, 1.0f }; evaluateMatrix(src, s, dst); for (int d = 0; d < kNumDst; ++d) if (!std::isfinite(dst[d])) finite = false; }
        CHECK(finite, "all %d x %d source/target combinations evaluate", kNumSrc, kNumDst);
        bool named = true; for (int i = 0; i < kNumSrc; ++i) if (!modSrcNames()[i] || !modSrcNames()[i][0]) named = false; for (int i = 0; i < kNumDst; ++i) if (!modDstNames()[i] || !modDstNames()[i][0]) named = false;
        CHECK(named, "every source and target has a name");
    }
    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
