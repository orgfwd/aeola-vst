// Guard tests for inharmonic timbres. Build: clang++ -std=c++17 -O2 guard_test.cpp -o guard_test
#include "../Source/DroneEngine.h"
#include "../Source/SkyMapper.h"
#include <cstdio>
#include <string>
using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setSpectrum(SkySettings& cfg, int a, int b, float morph)
{
    float h = timbreHarmonicity(timbres()[(size_t) a]) * (1 - morph) + timbreHarmonicity(timbres()[(size_t) b]) * morph;
    cfg.harmonicity = h;
    SpecIn si; si.a = a; si.b = b; si.morph = morph; si.f0 = 196; si.sr = 44100; computeSpectrum(si, cfg.partRatio, cfg.partAmp);
    cfg.nPart = h < 0.6f ? kPartials : 0;
}
// independent roughness measure: mean raw Plomp-Levelt roughness of all sounding pairs (not the penalty function the Guard uses)
static double meanPairRough(const MapResult& r, const SkySettings& cfg)
{
    double s = 0; int n = 0; for (int i = 0; i < r.n; ++i) for (int j = i + 1; j < r.n; ++j) { s += specRough(r.v[i].note, r.v[j].note, cfg.partRatio, cfg.partAmp, kPartials); ++n; }
    return n ? s / n : 0;
}

int main()
{
    printf("[1] harmonicity classification\n");
    {
        for (const char* nm : { "Glass", "Organ", "Saw", "Pulse", "Bell", "Bowl", "Metal", "Rust" }) { for (int i = 0; i < numTimbres(); ++i) if (!std::strcmp(timbres()[i].name, nm)) printf("  info: %-6s harmonicity %.2f\n", nm, timbreHarmonicity(timbres()[i])); }
        bool ok = true; for (int i = 0; i < numTimbres(); ++i) { const char* n = timbres()[i].name; float h = timbreHarmonicity(timbres()[i]);
            bool inh = !std::strcmp(n, "Bell") || !std::strcmp(n, "Bowl") || !std::strcmp(n, "Metal") || !std::strcmp(n, "Rust");
            if (inh != (h < 0.6f)) { ok = false; printf("    %s misclassified (%.2f)\\n", n, h); } }
        CHECK(ok, "Bell, Bowl, Metal, Rust count as inharmonic (< 0.6); all other timbres as harmonic");
    }

    printf("[2] harmonic timbres: Guard output is unchanged (same notes as before)\n");
    {
        int same = 0, total = 0; int nap; Airport ap = airports(nap)[0];
        for (int seed = 1; seed <= 20; ++seed)
        {
            Frame f; simulateSky(ap, seed, 1.7928e9 + seed * 137, 1.0, f, false);
            SkySettings a; a.apLat = ap.lat; a.apLon = ap.lon; a.radiusNm = ap.radiusNm; a.consonance = 0.7f; a.scale = 0; a.rootPc = 2; a.seed = seed;
            SkySettings b = a; setSpectrum(b, 0, 14, 0.4f);     // Glass / Saw
            SkyMapper m1, m2; MapResult r1, r2; m1.map(f, a, f.epoch, r1); m2.map(f, b, f.epoch, r2);
            bool eq = r1.n == r2.n; for (int i = 0; eq && i < r1.n; ++i) eq = r1.v[i].note == r2.v[i].note; same += eq; ++total;
        }
        CHECK(same == total, "%d of %d skies map to identical notes with a harmonic source", same, total);
    }

    printf("[3] inharmonic timbres: chosen notes are measurably less rough than unguarded picks\n");
    {
        struct Case { int a, b; float m; const char* name; };
        Case cases[] = { { 9, 9, 0, "Bell" }, { 10, 10, 0, "Bowl" }, { 11, 11, 0, "Metal" }, { 17, 17, 0, "Rust" }, { 9, 11, 0.5f, "Bell/Metal" } };
        int ap_n; const Airport* aps = airports(ap_n);
        for (auto& c : cases)
        {
            double sumG = 0, sumU = 0, minSpacing = 99; int nG = 0, nU = 0, voicesG = 0, voicesU = 0;
            for (int seed = 1; seed <= 40; ++seed)
            {
                const Airport& ap = aps[seed % ap_n]; Frame f; simulateSky(ap, seed, 1.7928e9 + seed * 211, 1.0, f, false);
                SkySettings g; g.apLat = ap.lat; g.apLon = ap.lon; g.radiusNm = ap.radiusNm; g.consonance = 0.7f; g.scale = 6 /* pentatonic minor */ ; g.rootPc = 9; g.seed = seed; g.maxAircraft = 8;
                setSpectrum(g, c.a, c.b, c.m);
                SkySettings u = g; u.consonance = 0.0f;          // guard effectively off (still in key)
                SkyMapper m1, m2; MapResult rg, ru; m1.map(f, g, f.epoch, rg); m2.map(f, u, f.epoch, ru);
                if (rg.n >= 2) { sumG += meanPairRough(rg, g); ++nG; } if (ru.n >= 2) { sumU += meanPairRough(ru, u); ++nU; }
                voicesG += rg.n; voicesU += ru.n;
                if (g.harmonicity < 0.4f) for (int i = 0; i < rg.n; ++i) for (int j = i + 1; j < rg.n; ++j) if (rg.v[i].note != rg.v[j].note) minSpacing = std::min(minSpacing, (double) std::abs(rg.v[i].note - rg.v[j].note));
            }
            double mg = sumG / std::max(1, nG), mu = sumU / std::max(1, nU);
            printf("  info: %-10s harmonicity %.2f | avg voices %.1f guarded vs %.1f unguarded | mean pair roughness %.4f vs %.4f | smallest spacing %s\n", c.name, (timbreHarmonicity(timbres()[c.a]) * (1 - c.m) + timbreHarmonicity(timbres()[c.b]) * c.m), voicesG / 40.0, voicesU / 40.0, mg, mu, minSpacing > 90 ? "n/a" : (std::to_string((int) minSpacing) + " st").c_str());
            CHECK(mg <= 0.7 * mu, "%s: guarded roughness is %.0f%% of unguarded (need <= 70%%)", c.name, 100.0 * mg / mu);
            CHECK(voicesG / 40.0 >= 3.0, "%s: still at least 3 voices on average (%.1f) - the sky is not silenced", c.name, voicesG / 40.0);
            if (minSpacing < 90) CHECK(minSpacing >= 5, "%s: no two different notes closer than 5 semitones (smallest %d)", c.name, (int) minSpacing);
        }
    }
    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
