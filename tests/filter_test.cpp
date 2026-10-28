// Filter tests for engine v1.2 (Clean / Ladder / Ladder 12, drive, key track, mix). Build: clang++ -std=c++17 -O2 filter_test.cpp -o filter_test
#include "../Source/DroneEngine.h"
#include <cstdio>
#include <vector>
#include <complex>
#include <chrono>
using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const int SR = 44100;

static void renderSec(Engine& e, std::vector<float>& L, std::vector<float>& R, double sec)
{ int n = (int) (sec * SR); L.assign(n, 0); R.assign(n, 0); for (int i = 0; i < n; i += 512) e.render(&L[i], &R[i], std::min(512, n - i)); }
static double mag(const std::vector<float>& x, size_t a, size_t b, double f)
{ std::complex<double> acc(0, 0); size_t n = b - a; for (size_t i = 0; i < n; ++i) { double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / n); acc += w * x[a + i] * std::polar(1.0, -2 * M_PI * f * (a + i) / SR); } return std::abs(acc) / n; }
static double rms(const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += x[i] * x[i]; return std::sqrt(s / (b - a)); }
static double dB(double v) { return 20 * std::log10(std::max(v, 1e-12)); }

static Engine mk(int timbre, int ftype, float cutoff, float reso, float drive = 0, float fmix = 1, float track = 0)
{
    Engine e; e.params.srcA = e.params.srcB = timbre; e.params.morph = 0; e.params.morphMotion = 0; e.params.drift = 0; e.params.life = 0; e.params.attack = 0.02f;
    e.params.air = 0; e.params.panSpread = 0; e.params.brightness = 1.0f; e.params.lowcut = 10; e.params.ftype = ftype; e.params.cutoff = cutoff; e.params.resonance = reso;
    e.params.drive = drive; e.params.fmix = fmix; e.params.ftrack = track; e.prepare(SR); return e;
}
// |H(f)| of the filter at harmonic h of note 36 (65.406 Hz), measured as filtered/unfiltered partial magnitude
static double response(int ftype, float cutoff, float reso, int h)
{
    const double f = 65.4064 * h;
    Engine ref = mk(5, 0, 18000, 0.0f); ref.params.ftype = 0; ref.noteOn(36, 1);
    Engine flt = mk(5, ftype, cutoff, reso); flt.noteOn(36, 1);
    std::vector<float> L, R, L2, R2; renderSec(ref, L, R, 3.0); renderSec(flt, L2, R2, 3.0);
    return mag(L2, SR, 3 * SR, f) / std::max(1e-12, mag(L, SR, 3 * SR, f));
}

int main()
{
    printf("[1] filter slopes (note 36 'Saw', cutoff 150 Hz, no resonance)\n");
    {
        double h4a = response(1, 150, 0.0f, 9), h4b = response(1, 150, 0.0f, 18), h4c = response(1, 150, 0.0f, 3);
        double s24 = dB(h4b) - dB(h4a);
        CHECK(std::fabs(s24 + 24.0) < 3.0, "Ladder 24: %.1f dB/oct between 589 and 1177 Hz (target -24 +-3)", s24);
        CHECK(h4c > h4a, "response falls with frequency (%.3f at 196 Hz vs %.4f at 589 Hz)", h4c, h4a);
        double a = response(2, 150, 0.0f, 9), b = response(2, 150, 0.0f, 18); double s12 = dB(b) - dB(a);
        CHECK(std::fabs(s12 + 12.0) < 3.0, "Ladder 12: %.1f dB/oct (target -12 +-3)", s12);
        double ca = response(0, 150, 0.0f, 9), cb = response(0, 150, 0.0f, 18); double sc = dB(cb) - dB(ca);
        CHECK(std::fabs(sc + 12.0) < 3.0, "Clean: %.1f dB/oct (target -12 +-3)", sc);
    }

    printf("[2] resonance: bass loss and self-oscillation\n");
    {
        double lo0 = response(1, 800, 0.0f, 2), lo1 = response(1, 800, 0.9f, 2);
        printf("  info: passband (131 Hz) response with cutoff 800 Hz: Reso 0 = %.1f dB, Reso 0.9 = %.1f dB (bass loss %.1f dB)\n", dB(lo0), dB(lo1), dB(lo0) - dB(lo1));
        CHECK(dB(lo0) - dB(lo1) > 3.0, "ladder loses bass as resonance rises (%.1f dB)", dB(lo0) - dB(lo1));
        double peak0 = response(1, 800, 0.0f, 12), peak1 = response(1, 800, 0.9f, 12);   // 785 Hz ~ the cutoff
        CHECK(peak1 > peak0 * 2.0, "resonant peak at the cutoff grows %.1fx", peak1 / peak0);
        // self-oscillation: play, release, listen to what remains
        Engine e = mk(0, 1, 800, 1.0f); e.params.release = 0.05f; e.noteOn(36, 1); e.noteOn(43, 1);
        std::vector<float> L, R; renderSec(e, L, R, 1.0); e.allNotesOff(); renderSec(e, L, R, 6.0);
        double r = rms(L, 4 * SR, 6 * SR);
        double best = 0, bf = 0; for (double f = 500; f <= 1100; f += 4) { double m = mag(L, 3 * SR, 6 * SR, f); if (m > best) { best = m; bf = f; } }
        CHECK(r > 0.003, "resonance 1.0 keeps ringing after the notes end (rms %.4f)", r);
        CHECK(bf > 800 * 0.88 && bf < 800 * 1.12, "self-oscillation at %.0f Hz (cutoff 800 Hz, +-12%%)", bf);
        double peakAbs = 0; for (float v : L) peakAbs = std::max(peakAbs, (double) std::fabs(v)); CHECK(peakAbs < 1.0, "self-oscillation stays bounded (peak %.3f)", peakAbs);
    }

    printf("[3] no NaN/Inf at extreme settings for every model\n");
    {
        bool ok = true; for (int t = 0; t < 3; ++t) for (float c : { 60.0f, 400.0f, 18000.0f }) for (float rs : { 0.0f, 0.5f, 1.0f }) for (float dr : { 0.0f, 24.0f })
        { Engine e = mk(3, t, c, rs, dr); e.noteOn(36, 1); e.noteOn(60, 1); e.noteOn(84, 1); std::vector<float> L, R; renderSec(e, L, R, 1.5);
          for (size_t i = 0; i < L.size(); ++i) if (!std::isfinite(L[i]) || !std::isfinite(R[i]) || std::fabs(L[i]) > 4.0f) ok = false; }
        CHECK(ok, "3 models x 3 cutoffs x 3 resonances x 2 drives: all finite and bounded");
    }

    printf("[4] drive adds harmonics, keeps loudness sane, and aliasing stays low\n");
    {
        // pure-ish tone: Glass at note 96 (2093 Hz), Colour 0, full output level (stress case for aliasing)
        auto run = [&](float drive, int ftype) { Engine e = mk(0, ftype, 18000, 0.0f, drive); e.params.brightness = 0.0f; e.params.gain = 1.0f; e.prepare(SR); e.noteOn(96, 1); std::vector<float> L, R; renderSec(e, L, R, 3.0); return L; };
        auto l0 = run(0, 0), l24 = run(24, 0);
        double f0 = 2093.005, h1a = mag(l0, SR, 3 * SR, f0), h3a = mag(l0, SR, 3 * SR, 3 * f0), h1b = mag(l24, SR, 3 * SR, f0), h3b = mag(l24, SR, 3 * SR, 3 * f0), h5b = mag(l24, SR, 3 * SR, 5 * f0);
        CHECK(dB(h3b / h1b) > dB(h3a / h1a) + 12.0, "3rd harmonic relative level: %.1f dB (drive 0) -> %.1f dB (24 dB)", dB(h3a / h1a), dB(h3b / h1b));
        CHECK(h5b > 0, "5th harmonic present (%.1f dB re fundamental)", dB(h5b / h1b));
        double r0 = rms(l0, SR, 3 * SR), r24 = rms(l24, SR, 3 * SR);
        CHECK(std::fabs(dB(r24) - dB(r0)) < 9.0, "loudness change with 24 dB drive: %.1f dB", dB(r24) - dB(r0));
        // aliasing: energy in bins that are not harmonics of f0 (above 100 Hz) relative to the fundamental
        auto alias = [&](const std::vector<float>& x) {
            double tot = 0; for (double f = 120; f < 20000; f += 31.0) { double d = std::fmod(f, f0); d = std::min(d, f0 - d); if (d < 45.0) continue; double m = mag(x, SR, 3 * SR, f); tot += m * m; }
            return tot; };
        double a24 = alias(l24), a0 = alias(l0), fund = h1b * h1b;
        printf("  info: non-harmonic energy: drive 0 = %.1f dB, drive 24 = %.1f dB (re fundamental)\n", 10 * std::log10(std::max(a0, 1e-20) / (h1a * h1a)), 10 * std::log10(a24 / fund));
        CHECK(10 * std::log10(a24 / fund) < -50.0, "aliasing below -50 dB re fundamental at 24 dB drive (%.1f dB)", 10 * std::log10(a24 / fund));
    }

    printf("[5] filter model / drive / cutoff jumps do not click\n");
    {
        Engine e = mk(0, 0, 9000, 0.1f); e.noteOn(48, 1); e.noteOn(55, 1); e.noteOn(64, 1); std::vector<float> L, R; renderSec(e, L, R, 2.0);
        double maxStep = 0; std::vector<float> all;
        auto go = [&](double sec) { renderSec(e, L, R, sec); for (size_t i = 0; i < L.size(); ++i) all.push_back(L[i]); };
        e.params.ftype = 1; go(0.5); e.params.drive = 18; go(0.5); e.params.cutoff = 300; go(0.5); e.params.ftype = 2; go(0.5); e.params.cutoff = 12000; e.params.drive = 0; go(0.5); e.params.ftype = 0; go(0.5);
        for (size_t i = 1; i < all.size(); ++i) maxStep = std::max(maxStep, (double) std::fabs(all[i] - all[i - 1]));
        double refStep = 0; { Engine q = mk(0, 0, 9000, 0.1f); q.noteOn(48, 1); q.noteOn(55, 1); q.noteOn(64, 1); renderSec(q, L, R, 2.0); renderSec(q, L, R, 1.0); for (size_t i = 1; i < L.size(); ++i) refStep = std::max(refStep, (double) std::fabs(L[i] - L[i - 1])); }
        CHECK(maxStep < refStep * 1.5, "max sample step across model/drive/cutoff changes: %.4f (steady-state reference %.4f, limit 1.5x)", maxStep, refStep);
    }

    printf("[6] filter mix and key track\n");
    {
        Engine dry = mk(5, 0, 18000, 0.0f); dry.noteOn(36, 1); Engine mx = mk(5, 1, 100, 0.0f, 0.0f, 0.0f); mx.noteOn(36, 1);
        std::vector<float> L, R, L2, R2; renderSec(dry, L, R, 3.0); renderSec(mx, L2, R2, 3.0);
        double worst = 0; for (int h : { 2, 5, 9, 14 }) worst = std::max(worst, std::fabs(dB(mag(L2, SR, 3 * SR, 65.4064 * h)) - dB(mag(L, SR, 3 * SR, 65.4064 * h))));
        CHECK(worst < 0.5, "Mix = 0%% passes the unfiltered sound unchanged (max level difference %.2f dB)", worst);
        Engine a = mk(0, 1, 1200, 0.2f, 0, 1, 1.0f); a.noteOn(72, 1); Engine b = mk(0, 1, 2400, 0.2f, 0, 1, 0.0f); b.noteOn(72, 1);
        renderSec(a, L, R, 5.0); renderSec(b, L2, R2, 5.0);
        double ra = rms(L, 4 * SR, 5 * SR), rb = rms(L2, 4 * SR, 5 * SR);
        CHECK(std::fabs(dB(ra) - dB(rb)) < 0.5, "key track 100%% on note 72 with cutoff 1200 equals cutoff 2400 without (%.2f dB apart)", dB(ra) - dB(rb));
    }

    printf("[7] CPU of the new filter path (16 voices, Ladder + 24 dB drive)\n");
    {
        double res[3];
        for (int mode = 0; mode < 3; ++mode)
        { Engine e = mk(0, mode == 0 ? 0 : 1, 3000, mode == 2 ? 0.7f : 0.2f, mode == 0 ? 0.0f : 24.0f); e.params.maxVoices = 16; e.params.life = 1; e.params.beatHz = 2; e.params.morphMotion = 1; e.params.air = 1;
          for (int n = 0; n < 16; ++n) e.noteOn(36 + n * 3, 1); std::vector<float> L, R; renderSec(e, L, R, 2.0);
          auto t0 = std::chrono::steady_clock::now(); renderSec(e, L, R, 6.0); res[mode] = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 6.0; }
        printf("  info: Clean %.1f%%, Ladder+drive %.1f%%, Ladder+drive+reso %.1f%% of one core\n", res[0] * 100, res[1] * 100, res[2] * 100);
        CHECK(res[1] < 0.22 && res[2] < 0.22, "engine worst case below 22%% of one core");
    }

    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
