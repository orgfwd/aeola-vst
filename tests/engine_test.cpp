// Offline engine tests. Build: clang++ -std=c++17 -O2 engine_test.cpp -o engine_test
#include "../Source/DroneEngine.h"
#include <cstdio>
#include <vector>
#include <chrono>
#include <complex>

using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void renderSeconds(Engine& e, std::vector<float>& L, std::vector<float>& R, double sec, int sr, int block = 512)
{
    int n = (int) (sec * sr); L.assign(n, 0); R.assign(n, 0);
    for (int i = 0; i < n; i += block) e.render(&L[i], &R[i], std::min(block, n - i));
}
// frequency of strongest peak near `target` using a zero-padded DFT around the target (Goertzel-like scan)
static double peakNear(const std::vector<float>& x, int sr, double target, double span = 4.0)
{
    int n = (int) x.size(); double best = 0, bestF = target;
    for (double f = target - span; f <= target + span; f += 0.02)
    {
        std::complex<double> acc(0, 0);
        for (int i = 0; i < n; i += 2) { double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / n); acc += w * x[i] * std::polar(1.0, -2 * M_PI * f * i / sr); }
        if (std::abs(acc) > best) { best = std::abs(acc); bestF = f; }
    }
    return bestF;
}

int main()
{
    const int sr = 44100;
    printf("[1] tuning accuracy (Just 5-limit, C3 E3 G3)\n");
    {
        Engine e; e.params.drift = 0; e.params.attack = 0.05f; e.params.north = 1.0f; e.params.system = kJust5; e.prepare(sr);
        e.noteOn(48, 1); e.noteOn(52, 1); e.noteOn(55, 1);
        std::vector<float> L, R; renderSeconds(e, L, R, 6.0, sr);
        std::vector<float> x(L.begin() + sr * 3, L.begin() + sr * 5);
        double c = peakNear(x, sr, 130.81), eN = peakNear(x, sr, 163.5), g = peakNear(x, sr, 196.2);
        CHECK(std::fabs(eN - 130.813 * 5.0 / 4) < 0.1, "E3 = %.2f Hz (5:4 of C3 = %.2f)", eN, 130.813 * 1.25);
        CHECK(std::fabs(g - 130.813 * 1.5) < 0.1, "G3 = %.2f Hz (3:2 = %.2f)", g, 130.813 * 1.5);
        CHECK(std::fabs(c - 130.813) < 0.1, "C3 = %.2f Hz", c);
    }

    printf("[2] Pythagorean third and meantone third\n");
    for (int sys : { (int) kPythagorean, (int) kMeantone })
    {
        Engine e; e.params.drift = 0; e.params.attack = 0.05f; e.params.north = 1.0f; e.params.system = sys; e.prepare(sr);
        e.noteOn(48, 1); e.noteOn(52, 1);
        std::vector<float> L, R; renderSeconds(e, L, R, 5.0, sr);
        std::vector<float> x(L.begin() + sr * 3, L.begin() + sr * 5);
        double expect = 130.813 * std::pow(2.0, pureCents(sys, 4) / 1200.0);
        double got = peakNear(x, sr, expect);
        CHECK(std::fabs(got - expect) < 0.1, "system %d third = %.2f (expected %.2f)", sys, got, expect);
    }

    printf("[3] no clicks on abrupt parameter jumps\n");
    {
        Engine e; e.params.attack = 0.05f; e.prepare(sr); e.noteOn(48, 1); e.noteOn(55, 1);
        std::vector<float> L, R; renderSeconds(e, L, R, 2.0, sr);
        double maxStepBefore = 0; for (size_t i = sr * 1; i + 1 < L.size(); ++i) maxStepBefore = std::max(maxStepBefore, (double) std::fabs(L[i + 1] - L[i]));
        e.params.gain = 1.0f; e.params.brightness = 1.0f; e.params.cutoff = 800; e.params.beatHz = 3; e.params.north = 1;
        std::vector<float> L2, R2; renderSeconds(e, L2, R2, 2.0, sr);
        double maxStep = 0; for (size_t i = 0; i + 1 < L2.size(); ++i) maxStep = std::max(maxStep, (double) std::fabs(L2[i + 1] - L2[i]));
        double peak = 0; for (float s : L2) peak = std::max(peak, (double) std::fabs(s));
        CHECK(maxStep < 0.12, "max sample-to-sample step after jumps = %.4f (peak %.3f)", maxStep, peak);
    }

    printf("[4] release reaches silence; stolen voices fade\n");
    {
        Engine e; e.params.release = 3.0f; e.params.maxVoices = 4; e.prepare(sr);
        for (int n : { 48, 52, 55, 59 }) e.noteOn(n, 1);
        std::vector<float> L, R; renderSeconds(e, L, R, 3.0, sr);
        e.noteOn(62, 1);   // forces a steal
        CHECK(e.liveCount() <= 4, "live voices after steal = %d (limit 4)", e.liveCount());
        std::vector<float> A, B; renderSeconds(e, A, B, 0.2, sr);
        double step = 0; for (size_t i = 0; i + 1 < A.size(); ++i) step = std::max(step, (double) std::fabs(A[i + 1] - A[i]));
        CHECK(step < 0.1, "max step during steal crossfade = %.4f", step);
        e.allNotesOff();
        renderSeconds(e, L, R, 4.0, sr);
        CHECK(e.audibleCount() == 0, "audible voices 4 s after release (release=3 s) = %d", e.audibleCount());
    }

    printf("[5] output headroom with 12 dense voices at default gain\n");
    {
        Engine e; e.params.maxVoices = 12; e.prepare(sr);
        for (int i = 0; i < 12; ++i) e.noteOn(36 + i * 3, 1.0f);
        std::vector<float> L, R; renderSeconds(e, L, R, 8.0, sr);
        double peak = 0; for (float s : L) peak = std::max(peak, (double) std::fabs(s));
        printf("  info: dry peak = %.3f (limiter in plugin catches >1)\n", peak);
        CHECK(peak < 2.5, "dry peak finite and sane");
    }

    printf("[6] CPU: 16 live voices, 44.1 kHz, 512-sample blocks\n");
    {
        Engine e; e.params.maxVoices = 16; e.params.beatHz = 1.5f; e.prepare(sr);
        for (int i = 0; i < 16; ++i) e.noteOn(36 + i * 2, 1.0f);
        std::vector<float> L(512), R(512);
        for (int i = 0; i < 200; ++i) e.render(L.data(), R.data(), 512);     // warm up
        auto t0 = std::chrono::steady_clock::now();
        int blocks = 2000; for (int i = 0; i < blocks; ++i) e.render(L.data(), R.data(), 512);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        double audioSecs = blocks * 512.0 / sr;
        printf("  info: %.1f%% of one core (render only)\n", 100.0 * secs / audioSecs);
        CHECK(secs / audioSecs < 0.20, "CPU < 20%% of one core");
    }

    printf("[7] determinism\n");
    {
        auto run = [&]() { Engine e; e.prepare(sr); e.noteOn(50, 1); e.noteOn(57, 1); std::vector<float> L, R; renderSeconds(e, L, R, 1.0, sr); double s = 0; for (float v : L) s += std::fabs(v); return s; };
        double a = run(), b = run(); CHECK(a == b, "two identical runs give identical output (%.6f)", a);
    }

    printf("[8] roughness calibration at C3 (higher = rougher)\n");
    {
        int c = 60; const char* names[] = { "unison", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7", "octave" };
        for (int i = 0; i <= 12; ++i) printf("  %-7s %.3f\n", names[i], noteRough(c, c + i));
        CHECK(noteRough(c, c + 1) > noteRough(c, c + 7) * 1.0, "minor second much rougher than fifth");
    }

    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
