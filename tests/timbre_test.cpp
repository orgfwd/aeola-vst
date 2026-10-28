// Timbre bank + engine v3 tests. Build: clang++ -std=c++17 -O2 timbre_test.cpp -o timbre_test
#include "../Source/DroneEngine.h"
#include <cstdio>
#include <vector>
#include <complex>
#include <chrono>
using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void renderSec(Engine& e, std::vector<float>& L, std::vector<float>& R, double sec, int sr = 44100)
{ int n = (int) (sec * sr); L.assign(n, 0); R.assign(n, 0); for (int i = 0; i < n; i += 512) e.render(&L[i], &R[i], std::min(512, n - i)); }
static double bandEnergy(const std::vector<float>& x, int sr, double f0, double f1)
{ // Goertzel-style scan on 2 s window
  int n = (int) x.size(); double tot = 0; for (double f = f0; f <= f1; f += 7.0) { std::complex<double> a(0,0); for (int i = 0; i < n; i += 2) { double w = 0.5 - 0.5*std::cos(2*M_PI*i/n); a += w*x[i]*std::polar(1.0, -2*M_PI*f*i/sr);} tot += std::norm(a);} return tot; }
static double centroid(const std::vector<float>& x, int sr)
{ int n = std::min<int>((int) x.size(), 16384); std::vector<float> s(x.end() - n, x.end()); double num = 0, den = 0;
  for (double f = 100; f < 8000; f += 25) { std::complex<double> a(0,0); for (int i = 0; i < n; i += 2) { double w = 0.5-0.5*std::cos(2*M_PI*i/n); a += w*s[i]*std::polar(1.0,-2*M_PI*f*i/sr);} double m = std::abs(a); num += f*m; den += m; } return num/std::max(den,1e-12); }
static double rms(const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += x[i]*x[i]; return std::sqrt(s/(b-a)); }

static Engine mk(int a, int b, float morph, int sr = 44100)
{ Engine e; e.params.srcA = a; e.params.srcB = b; e.params.morph = morph; e.params.morphMotion = 0; e.params.drift = 0; e.params.attack = 0.05f;
  e.params.life = 0; e.prepare(sr); return e; }

int main()
{
    const int sr = 44100;
    printf("[1] bank integrity: %d timbres, unique names, finite, normalised, no partial above 0.45*fs\n", numTimbres());
    {
        bool ok = true; for (int i = 0; i < numTimbres(); ++i)
        { SpecIn s; s.a = s.b = i; s.f0 = 196; s.sr = sr; float r[kPartials], a[kPartials]; computeSpectrum(s, r, a);
          double e2 = 0; for (int k = 0; k < kPartials; ++k) { if (!std::isfinite(a[k]) || !std::isfinite(r[k])) ok = false; e2 += a[k]*a[k]; if (a[k] > 0 && s.f0 * r[k] > 0.45*sr) ok = false; }
          if (std::fabs(std::sqrt(e2) - 0.6) > 0.01) { ok = false; printf("    %s: norm %.3f\n", timbres()[i].name, std::sqrt(e2)); } }
        CHECK(ok, "all spectra valid");
        bool uniq = true; for (int i = 0; i < numTimbres(); ++i) for (int j = i+1; j < numTimbres(); ++j) if (!std::strcmp(timbres()[i].name, timbres()[j].name)) uniq = false;
        CHECK(uniq && numTimbres() >= 14, "%d unique names", numTimbres());
    }

    printf("[2] timbres are measurably different from each other (spectral distance)\n");
    {
        int close = 0; double minD = 1e9; int mi = 0, mj = 0;
        for (int i = 0; i < numTimbres(); ++i) for (int j = i+1; j < numTimbres(); ++j)
        { SpecIn a; a.a = a.b = i; a.f0 = 196; a.sr = sr; SpecIn b = a; b.a = b.b = j; float ra[kPartials], aa[kPartials], rb[kPartials], ab[kPartials]; computeSpectrum(a, ra, aa); computeSpectrum(b, rb, ab);
          // compare on a log-frequency grid of 40 bins
          double d = 0; for (int bin = 0; bin < 40; ++bin) { double lo = 80*std::pow(100.0, bin/40.0), hi = 80*std::pow(100.0,(bin+1)/40.0); double ea = 0, eb = 0;
              for (int k = 0; k < kPartials; ++k) { double fa = 196*ra[k], fb = 196*rb[k]; if (fa >= lo && fa < hi) ea += aa[k]*aa[k]; if (fb >= lo && fb < hi) eb += ab[k]*ab[k]; }
              d += std::pow(std::sqrt(ea) - std::sqrt(eb), 2); }
          d = std::sqrt(d); if (d < minD) { minD = d; mi = i; mj = j; } if (d < 0.08) ++close; }
        CHECK(close == 0, "closest pair: %s vs %s, distance %.3f (need >= 0.08)", timbres()[mi].name, timbres()[mj].name, minD);
    }

    printf("[3] every timbre renders cleanly through the engine (no NaN, bounded, audible)\n");
    {
        bool ok = true; for (int i = 0; i < numTimbres(); ++i)
        { Engine e = mk(i, i, 0); e.params.life = 0.8f; e.params.beatHz = 0.5f; e.noteOn(48, 1); e.noteOn(55, 1); e.noteOn(64, 1); e.noteOn(71, 1);
          std::vector<float> L, R; renderSec(e, L, R, 3.0); float pk = 0; for (size_t k = 0; k < L.size(); ++k) { if (!std::isfinite(L[k]) || !std::isfinite(R[k])) ok = false; pk = std::max(pk, std::max(std::fabs(L[k]), std::fabs(R[k]))); }
          double r = rms(L, sr*2, sr*3); if (r < 0.005 || pk > 1.2f) { ok = false; printf("    %s rms %.4f peak %.3f\n", timbres()[i].name, r, pk); } }
        CHECK(ok, "all %d timbres OK", numTimbres());
    }

    printf("[4] level consistency across timbres (loudness within 6 dB)\n");
    {
        double lo = 1e9, hi = 0; const char* nl = ""; const char* nh = "";
        for (int i = 0; i < numTimbres(); ++i) { Engine e = mk(i, i, 0); e.noteOn(55, 1); std::vector<float> L, R; renderSec(e, L, R, 9.0); double r = std::sqrt(rms(L, sr*3, sr*9)*rms(L, sr*3, sr*9) + rms(R, sr*3, sr*9)*rms(R, sr*3, sr*9));
          if (r < lo) { lo = r; nl = timbres()[i].name; } if (r > hi) { hi = r; nh = timbres()[i].name; } }
        double dB = 20*std::log10(hi/lo); CHECK(dB < 6.0, "quietest %s (%.4f) vs loudest %s (%.4f): %.1f dB (6 s window, covers one built-in chorus cycle)", nl, lo, nh, hi, dB);
    }

    printf("[5] partials sit at the right frequencies (Organ G3 at 196 Hz; Bell has a 0.5x hum tone)\n");
    {
        Engine e = mk(1, 1, 0); e.noteOn(55, 1); std::vector<float> L, R; renderSec(e, L, R, 3.0); std::vector<float> x(L.begin()+sr, L.begin()+3*sr);
        double f0 = 195.998; double on = bandEnergy(x, sr, f0-2, f0+2), off = bandEnergy(x, sr, f0*1.5-2, f0*1.5+2);
        CHECK(on > off * 50, "organ: fundamental dominates a non-partial at 1.5x (ratio %.0f)", on / std::max(off, 1e-12));
        double h3 = bandEnergy(x, sr, f0*3-2, f0*3+2); CHECK(h3 > off * 20, "organ: 3rd harmonic present");
        Engine b = mk(9, 9, 0); b.noteOn(55, 1); renderSec(b, L, R, 3.0); std::vector<float> y(L.begin()+sr, L.begin()+3*sr);
        double hum = bandEnergy(y, sr, f0*0.5-2, f0*0.5+2), none = bandEnergy(y, sr, f0*0.8-2, f0*0.8+2);
        CHECK(hum > none * 50, "bell: hum tone at 0.5x (%.1f Hz) present", f0*0.5);
        double inh = bandEnergy(y, sr, f0*1.19-2, f0*1.19+2); CHECK(inh > none * 50, "bell: inharmonic partial at 1.19x present");
    }

    printf("[6] Colour changes brightness on every timbre\n");
    {
        int bad = 0; for (int i = 0; i < numTimbres(); ++i)
        { double c[2]; for (int k = 0; k < 2; ++k) { Engine e = mk(i, i, 0); e.params.brightness = k ? 0.9f : 0.1f; e.noteOn(48, 1); std::vector<float> L, R; renderSec(e, L, R, 3.0); c[k] = centroid(L, sr); }
          if (c[1] < c[0] * 1.05) { ++bad; printf("    %s: %.0f -> %.0f Hz\n", timbres()[i].name, c[0], c[1]); } }
        CHECK(bad == 0, "centroid rises with Colour for all timbres (%d failures)", bad);
    }

    printf("[7] Morph blends continuously between A and B with no jump\n");
    {
        Engine e = mk(1, 9, 0); e.noteOn(48, 1); std::vector<float> L, R; renderSec(e, L, R, 1.0);
        double c0 = centroid(L, sr); e.params.morph = 1.0f; renderSec(e, L, R, 2.0); double c1 = centroid(L, sr);
        double step = 0; for (size_t i = 1; i < L.size(); ++i) step = std::max(step, (double) std::fabs(L[i] - L[i-1]));
        CHECK(std::fabs(c1 - c0) > 30, "centroid moved %.0f -> %.0f Hz", c0, c1);
        CHECK(step < 0.1, "no click while morphing (max sample step %.4f)", step);
        double prev = 0; bool mono = true; Engine m = mk(0, 2, 0); m.noteOn(48, 1);
        for (int s = 0; s <= 4; ++s) { m.params.morph = s * 0.25f; std::vector<float> a, b; renderSec(m, a, b, 1.2); double c = centroid(a, sr); if (s > 0 && std::fabs(c - prev) < 1.0) mono = false; prev = c; }
        CHECK(mono, "each morph step changes the sound");
    }

    printf("[8] Life: overtones wander over time; Life=0 is steady\n");
    {
        auto variation = [&](float life) { Engine e = mk(0, 0, 0); e.params.life = life; e.params.panSpread = 0; e.noteOn(48, 1); std::vector<float> L, R; renderSec(e, L, R, 16.0);
            std::vector<double> r; for (int s = 3; s < 16; ++s) { std::vector<float> w(L.begin()+s*sr, L.begin()+(s+1)*sr); double f = 130.813*3, acc = 0; for (double q = f-3; q <= f+3; q += 0.5) acc += bandEnergy(w, sr, q, q); r.push_back(acc); }
            double mn = 1e30, mx = 0; for (double v : r) { mn = std::min(mn, v); mx = std::max(mx, v); } return (mx - mn) / mx; };
        double v0 = variation(0.0f), v1 = variation(1.0f);
        CHECK(v0 < 0.05 && v1 > 0.1, "3rd-harmonic energy varies by %.1f%% with Life=1 vs %.1f%% with Life=0", v1*100, v0*100);
    }

    printf("[9] Air adds noise only to timbres that have breath\n");
    {
        auto noiseFloor = [&](int t, float air) { Engine e = mk(t, t, 0); e.params.air = air; e.noteOn(48, 1); std::vector<float> L, R; renderSec(e, L, R, 3.0); std::vector<float> x(L.begin()+sr, L.end());
            double f0 = 130.81; return bandEnergy(x, sr, f0*1.31, f0*1.37) + bandEnergy(x, sr, f0*5.4, f0*5.9); };
        double flute0 = noiseFloor(7, 0.0f), flute1 = noiseFloor(7, 1.0f), glass1 = noiseFloor(0, 1.0f), glass0 = noiseFloor(0, 0.0f);
        CHECK(flute1 > flute0 * 3, "flute noise rises with Air (%.0fx)", flute1 / std::max(flute0, 1e-12));
        CHECK(glass1 < glass0 * 3 + 1e-6, "glass has no breath (%.2fx)", glass1 / std::max(glass0, 1e-12));
    }

    printf("[10] tuning still exact with every timbre (E3 = 5:4)\n");
    {
        int bad = 0; for (int i = 0; i < numTimbres(); ++i)
        { Engine e = mk(i, i, 0); e.params.north = 1.0f; e.params.system = kJust5; e.noteOn(48, 1); e.noteOn(52, 1); std::vector<float> L, R; renderSec(e, L, R, 8.0);
          std::vector<float> x(L.begin()+2*sr, L.end()); double want = 130.813*1.25, best = 0, bf = 0;
          for (double f = want-3; f <= want+3; f += 0.05) { std::complex<double> a(0,0); for (size_t k = 0; k < x.size(); k += 2) { double w = 0.5-0.5*std::cos(2*M_PI*k/x.size()); a += w*x[k]*std::polar(1.0,-2*M_PI*f*k/sr);} if (std::abs(a) > best) { best = std::abs(a); bf = f; } }
          double tol = 0.12 + 0.6 * timbres()[i].autoBeat; if (std::fabs(bf - want) > tol) { ++bad; printf("    %s: %.2f\n", timbres()[i].name, bf); } }
        CHECK(bad == 0, "just-intonation third is exact for all timbres (tolerance covers each timbre's built-in chorus) (%d off)", bad);
    }

    printf("[11] release tails and silence; voice stealing is click-free\n");
    {
        Engine e = mk(3, 3, 0); e.params.release = 2.0f; e.params.maxVoices = 4; for (int n : {48, 52, 55, 59}) e.noteOn(n, 1);
        std::vector<float> L, R; renderSec(e, L, R, 1.0); for (int n : {50, 53, 57, 60, 62}) e.noteOn(n, 1);  // forces steals
        renderSec(e, L, R, 1.0); double step = 0; for (size_t i = 1; i < L.size(); ++i) step = std::max(step, (double) std::fabs(L[i]-L[i-1]));
        CHECK(step < 0.12, "voice stealing: max step %.4f", step);
        e.allNotesOff(); renderSec(e, L, R, 8.0); CHECK(rms(L, 6*sr, 8*sr) < 1e-4, "silent after release: %.6f", rms(L, 6*sr, 8*sr));
        CHECK(e.audibleCount() == 0, "all slots free again");
    }

    printf("[12] CPU: 16 voices x all timbres with Life + Beat (worst case)\n");
    {
        double worst = 0; const char* wn = "";
        for (int i = 0; i < numTimbres(); ++i)
        { Engine e = mk(i, (i+5) % numTimbres(), 0.5f); e.params.maxVoices = 16; e.params.life = 1; e.params.beatHz = 2; e.params.morphMotion = 1; e.params.air = 1; for (int n = 0; n < 16; ++n) e.noteOn(36 + n * 3, 1);
          std::vector<float> L, R; renderSec(e, L, R, 2.0); auto t0 = std::chrono::steady_clock::now(); renderSec(e, L, R, 6.0);
          double c = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 6.0; if (c > worst) { worst = c; wn = timbres()[i].name; } }
        printf("  info: worst %.1f%% of one core (%s)\n", worst * 100, wn);
        CHECK(worst < 0.30, "worst-case CPU below 30%% of a core");
    }

    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
