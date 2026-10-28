#include <set>
// Sky model tests. Build: clang++ -std=c++17 -O2 sky_test.cpp -o sky_test
#include "../Source/SkyMapper.h"
#include <cstdio>
using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main()
{
    int na; const Airport* aps = airports(na); const Airport& arn = aps[0];
    double t0 = 1792740000.0;   // some day in Oct 2026

    printf("[1] simulator determinism and daily curve\n");
    {
        Frame a, b; simulateSky(arn, 42, t0, 1.0, a); simulateSky(arn, 42, t0, 1.0, b);
        bool same = a.count == b.count; for (int i = 0; same && i < a.count; ++i) same = a.ac[i].id == b.ac[i].id && a.ac[i].altFt == b.ac[i].altFt && a.ac[i].lat == b.ac[i].lat;
        CHECK(same, "identical frames for identical (airport, seed, time): %d aircraft", a.count);
        Frame c; simulateSky(arn, 43, t0, 1.0, c);
        CHECK(c.count != a.count || c.ac[0].id != a.ac[0].id, "different seed gives a different sky");
        int night = 0, day = 0, samples = 0;
        for (int h = 0; h < 24; ++h) { Frame f; simulateSky(arn, 7, t0 - fmod(t0, 86400.0) + h * 3600.0 - 2 * 3600, 1.0, f); (h >= 8 && h <= 18 ? day : night) += f.count; ++samples; }
        printf("  info: aircraft summed over daytime hours=%d, other hours=%d\n", day, night);
        CHECK(day > night / 2, "daytime traffic not lighter than night");
        bool geo = true; Frame f; simulateSky(arn, 5, t0 + 36000, 1.0, f);
        for (int i = 0; i < f.count; ++i) geo = geo && f.ac[i].distNm <= 40.5 && f.ac[i].altFt >= 0 && f.ac[i].altFt < 20000;
        CHECK(geo, "all simulated aircraft inside the 40 nm circle, sane altitudes (%d aircraft)", f.count);
    }

    printf("[2] musical guard: no harsh intervals at default Consonance across 300 frames\n");
    {
        SkyMapper m; SkySettings cfg; cfg.apLat = arn.lat; cfg.apLon = arn.lon; cfg.seed = 9;
        int harsh = 0, total = 0, anchors = 0, maxN = 0; double roughSum = 0; int changes = 0, prevNotes[kMaxAc] = {};
        for (int i = 0; i < 300; ++i)
        {
            double now = t0 + i * 20.0; Frame f; simulateSky(arn, 9, now, 1.0, f);
            MapResult r; m.map(f, cfg, now, r);
            maxN = std::max(maxN, r.n);
            bool hasAnchor = false;
            for (int a = 0; a < r.n; ++a) { hasAnchor |= r.v[a].anchor; for (int b = a + 1; b < r.n; ++b) { ++total; double p = guardPenalty(r.v[a].note, r.v[b].note); if (p > 0.1 + (1 - cfg.consonance) * 0.95 + 1e-9) ++harsh; } }
            anchors += hasAnchor; roughSum += r.roughness;
            for (int a = 0; a < r.n && a < kMaxAc; ++a) { if (prevNotes[a] && prevNotes[a] != r.v[a].note) ++changes; prevNotes[a] = r.v[a].note; }
        }
        CHECK(harsh == 0, "pairs above the allowed penalty: %d of %d", harsh, total);
        CHECK(anchors == 300, "anchor drone present in every frame");
        printf("  info: max voices %d, mean roughness meter %.2f, note changes (slot-wise) %d / 300 frames\n", maxN, roughSum / 300, changes);
        CHECK(maxN <= 12, "max simultaneous orders = %d", maxN);
    }

    printf("[3] all notes belong to the chosen key/scale (Sky voices)\n");
    for (int scale : { 0, 6, 9 })
    {
        SkyMapper m; SkySettings cfg; cfg.scale = scale; cfg.rootPc = 9; cfg.apLat = arn.lat; cfg.apLon = arn.lon; cfg.evolveDepth = 0.0f;
        int outside = 0, count = 0;
        for (int i = 0; i < 100; ++i)
        {
            double now = t0 + i * 30.0; Frame f; simulateSky(arn, 3, now, 1.0, f); MapResult r; m.map(f, cfg, now, r);
            for (int a = 0; a < r.n; ++a) { ++count; if (!inScale(r.v[a].note, cfg.rootPc, cfg.scale)) ++outside; }
        }
        CHECK(outside == 0, "scale %d (%s): %d of %d notes outside the scale", scale, scales()[scale].name, outside, count);
    }

    printf("[4] consonance 0%% allows clusters, 100%% forbids them\n");
    {
        Frame f; simulateSky(arn, 21, t0 + 3600 * 8, 1.0, f);
        SkySettings lo, hi; lo.apLat = hi.apLat = arn.lat; lo.apLon = hi.apLon = arn.lon; lo.consonance = 0.0f; hi.consonance = 1.0f; lo.scale = hi.scale = 11; // chromatic
        SkyMapper m1, m2; MapResult a, b; m1.map(f, lo, t0, a); m2.map(f, hi, t0, b);
        printf("  info: aircraft=%d  roughness lo=%.2f hi=%.2f  voices lo=%d hi=%d  rejected hi=%d\n", f.count, a.roughness, b.roughness, a.n, b.n, b.rejected);
        CHECK(b.roughness <= a.roughness, "roughness meter lower when Consonance is high");
    }

    printf("[5] quiet sky is never one lonely note\n");
    {
        Frame empty; empty.count = 0; SkyMapper m; SkySettings cfg; MapResult r; m.map(empty, cfg, t0, r);
        CHECK(r.n >= 3, "empty sky yields %d voices (anchor + ghosts)", r.n);
    }

    printf("[6] per-aircraft outcomes (Profile view data)\n");
    {
        int fewer = 0, bad = 0, total = 0;
        for (int k = 0; k < 60; ++k)
        {
            Frame f; simulateSky(arn, 3, t0 + 36000 + k * 97, 1.6, f); SkyMapper m; SkySettings cfg; cfg.maxAircraft = 4; MapResult r; m.map(f, cfg, t0 + k * 97, r);
            int c[5] = {}; for (int i = 0; i < r.nOut; ++i) ++c[r.outcome[i].status];
            total += f.count;
            if (r.nOut != f.count) ++bad;
            if (c[1] + c[2] != r.accepted || c[2] != r.shifted || c[3] != r.rejected) ++bad;
            if (c[4] > 0) ++fewer;
        }
        CHECK(bad == 0, "every aircraft gets exactly one outcome; counts match accepted/shifted/rejected (%d aircraft checked)", total);
        CHECK(fewer > 0, "aircraft over the voice limit are reported (status 4) in %d of 60 frames", fewer);
    }

    printf("[7] classes mapping: class voices, featured classes, swell\n");
    {
        SkySettings cfg; cfg.classes = true; cfg.contrast = 1.0f; cfg.swell = true; cfg.apLat = arn.lat; cfg.apLon = arn.lon; cfg.maxAircraft = 5;
        cfg.cls[kCargo].timbre = 5; cfg.cls[kCargo].attackMul = 3.0f; cfg.cls[kHeli].chop = 0.5f; cfg.cls[kProp].chop = 0.45f; cfg.cls[kHeavy].octave = -1; cfg.cls[kLight].octave = 1;
        int cargoSeen = 0, cargoVoiced = 0, styled = 0, chopped = 0, framesWithClasses = 0; int otherDropped = 0, featDropped = 0;
        for (int k = 0; k < 400; ++k)
        {
            Frame f; simulatePlace(placeFromAirport(0, 40), 5, t0 + 20000 + k * 53, 1.8, f, true); SkyMapper m; MapResult r; m.map(f, cfg, t0 + k * 53, r);
            bool anyNonStd = false;
            for (int i = 0; i < f.count; ++i) if (f.ac[i].category != f.ac[i].legacyCat || f.ac[i].category == kBiz || f.ac[i].category == kProp) anyNonStd = true;
            if (anyNonStd) ++framesWithClasses;
            for (int i = 0; i < r.nOut; ++i)
            {
                int cat = -1; for (int j = 0; j < f.count; ++j) if (f.ac[j].id == r.outcome[i].id) cat = f.ac[j].category;
                bool feat = cat == kCargo || cat == kHeavy || cat == kHeli || cat == kMil;
                if (r.outcome[i].status == 3 || r.outcome[i].status == 4) { if (feat) ++featDropped; else ++otherDropped; }
                if (cat == kCargo) { ++cargoSeen; if (r.outcome[i].status <= 2) ++cargoVoiced; }
            }
            for (int v = 0; v < r.n; ++v) { if (r.v[v].timbre == 5 && r.v[v].attackMul > 2.5f) ++styled; if (r.v[v].chopDepth > 0.4f) ++chopped; }
        }
        CHECK(framesWithClasses > 60, "simulated skies contain business jets / turboprops / helicopters / military (%d of 400 frames)", framesWithClasses);
        CHECK(styled > 0, "freight voices carry their own timbre and slow attack (%d voices)", styled);
        CHECK(chopped > 0, "helicopter / propeller voices carry chop (%d voices)", chopped);
        CHECK(cargoSeen == 0 || cargoVoiced * 10 >= cargoSeen * 7, "freight is rarely dropped: %d of %d voiced", cargoVoiced, cargoSeen);
        printf("  info: dropped featured %d, dropped others %d\n", featDropped, otherDropped);
        // Classic mapping must not carry class styling
        SkySettings cl; cl.classes = false; Frame f; simulatePlace(placeFromAirport(0, 40), 5, t0 + 30000, 1.8, f, false); SkyMapper m; MapResult r; m.map(f, cl, t0, r);
        bool plain = true; for (int v = 0; v < r.n; ++v) plain = plain && r.v[v].timbre < 0 && r.v[v].chopDepth == 0.0f && r.v[v].attackMul == 1.0f;
        CHECK(plain, "Classic mapping produces plain voices");
        // swell: the same aircraft is louder when passing overhead than far away
        Frame one; one.count = 1; Aircraft& a = one.ac[0]; a.id = 7; a.category = kLarge; a.legacyCat = kLarge; a.altFt = 20000; a.gsKt = 400;
        SkySettings sw = cfg; sw.cls[kLarge] = ClassStyle(); sw.ghostMin = 0; sw.anchor = false; sw.evolveDepth = 0;
        auto levelAt = [&](double dn) { a.lat = arn.lat + dn / 60.0; a.lon = arn.lon; a.distNm = (float) dn; SkyMapper mm; MapResult rr; mm.map(one, sw, t0, rr); return rr.n ? rr.v[0].level : -1.0f; };
        float near0 = levelAt(0.5), far = levelAt(25.0); CHECK(near0 > far * 1.3f, "overhead swell: level %.2f overhead vs %.2f far away", near0, far);
        sw.swell = false; float noSw = levelAt(0.5); CHECK(noSw < near0 * 0.8f, "swell can be switched off (%.2f without, %.2f with)", noSw, near0);
    }

    printf("[8] places: any point on earth, radius, overflight simulator\n");
    {
        Place home; home.lat = 59.33; home.lon = 18.07; home.radiusNm = 60; home.isAirport = false; std::snprintf(home.name, sizeof home.name, "Stockholm");
        Frame a, b; simulatePlace(home, 11, t0 + 5000, 1.0, a); simulatePlace(home, 11, t0 + 5000, 1.0, b);
        bool same = a.count == b.count; for (int i = 0; same && i < a.count; ++i) same = a.ac[i].id == b.ac[i].id && a.ac[i].lat == b.ac[i].lat;
        CHECK(same && a.count > 0, "overflight sky is deterministic (%d aircraft)", a.count);
        int total = 0, inside = 0, hi = 0; int cats[kNumClasses] = {};
        for (int k = 0; k < 200; ++k)
        {
            Frame f; simulatePlace(home, 11, t0 + k * 600.0, 1.0, f);
            for (int i = 0; i < f.count; ++i)
            {
                ++total; double dN = (f.ac[i].lat - home.lat) * 60.0, dE = (f.ac[i].lon - home.lon) * 60.0 * std::cos(home.lat * M_PI / 180.0);
                if (std::sqrt(dN * dN + dE * dE) <= home.radiusNm * 1.02) ++inside; if (f.ac[i].altFt > 25000) ++hi; ++cats[f.ac[i].category];
            }
        }
        CHECK(total > 0 && inside == total, "all overflights inside the radius (%d of %d)", inside, total);
        CHECK(hi > total / 3, "mostly cruise altitudes (%d of %d above 25000 ft)", hi, total);
        printf("  info: classes in 200 skies:"); for (int c = 0; c < kNumClasses; ++c) printf(" %s=%d", className(c), cats[c]); printf("\n");
        Place small = home; small.radiusNm = 8; Place big = home; big.radiusNm = 250; int ns = 0, nb = 0;
        for (int k = 0; k < 60; ++k) { Frame f; simulatePlace(small, 3, t0 + k * 500.0, 1.0, f); ns += f.count; simulatePlace(big, 3, t0 + k * 500.0, 1.0, f); nb += f.count; }
        CHECK(nb > ns * 3, "a larger radius hears more aircraft (%d vs %d)", nb, ns);
        Place q = home; q.radiusNm = 61; CHECK(home.key() != q.key() && home.key() == home.key(), "place key changes with radius");
        Place q2 = home; q2.lat += 0.001; CHECK(home.key() == q2.key(), "place key is stable for the same spot");
        Place east = placeFromAirport(1, 40); CHECK(east.key() != placeFromAirport(0, 40).key(), "airports have different keys");
    }

    printf("[9] solo and mute per class\n");
    {
        SkySettings cfg; cfg.classes = true; cfg.contrast = 1.0f; cfg.apLat = arn.lat; cfg.apLon = arn.lon; cfg.maxAircraft = 8;
        int framesChecked = 0, bad = 0, soloVoices = 0, muteBad = 0, anyAnchor = 0;
        for (int k = 0; k < 200; ++k)
        {
            Frame f; simulatePlace(placeFromAirport(0, 40), 9, t0 + 20000 + k * 61, 1.8, f, true);
            SkySettings c = cfg; c.soloMask = 1u << kCargo | 1u << kHeli; SkyMapper m; MapResult r; m.map(f, c, t0 + k * 61, r); ++framesChecked;
            for (int v = 0; v < r.n; ++v)
            {
                if (r.v[v].anchor || r.v[v].ghost) { ++anyAnchor; continue; }
                if (r.v[v].cls != kCargo && r.v[v].cls != kHeli) ++bad; else ++soloVoices;
            }
            for (int i = 0; i < r.nOut; ++i) { int cat = -1; for (int j = 0; j < f.count; ++j) if (f.ac[j].id == r.outcome[i].id) cat = f.ac[j].category; if (cat != kCargo && cat != kHeli && r.outcome[i].status != 5) ++bad; }
            SkySettings d = cfg; d.muteMask = 1u << kHeavy; MapResult r2; SkyMapper m2; m2.map(f, d, t0 + k * 61, r2);
            for (int v = 0; v < r2.n; ++v) if (!r2.v[v].anchor && !r2.v[v].ghost && r2.v[v].cls == kHeavy) ++muteBad;
        }
        CHECK(bad == 0 && soloVoices > 0, "solo: only soloed classes sound, others are marked muted (%d voices over %d frames)", soloVoices, framesChecked);
        CHECK(anyAnchor == 0, "solo silences the anchor drone and quiet-sky voices");
        CHECK(muteBad == 0, "mute removes a class");
    }

    printf("[10] cruise traffic spreads over several notes (no single-note sky)\n");
    {
        Place home; home.lat = 59.33; home.lon = 18.07; home.radiusNm = 60; home.isAirport = false;
        SkySettings cfg; cfg.classes = true; cfg.contrast = 0.7f; cfg.apLat = home.lat; cfg.apLon = home.lon; cfg.radiusNm = 60; cfg.evolveDepth = 0;
        long snd = 0, distinct = 0; int frames = 0;
        for (int k = 0; k < 200; ++k)
        {
            Frame f; simulatePlace(home, 11, t0 + k * 600.0, 1.0, f); SkyMapper m; MapResult r; m.map(f, cfg, t0 + k * 600.0, r);
            std::set<int> notes; int n = 0; for (int v = 0; v < r.n; ++v) if (!r.v[v].anchor && !r.v[v].ghost) { notes.insert(r.v[v].note); ++n; }
            if (n >= 3) { snd += n; distinct += (long) notes.size(); ++frames; }
        }
        CHECK(frames > 20 && distinct * 10 >= snd * 6, "busy cruise skies use distinct notes (%ld notes for %ld voices over %d frames)", distinct, snd, frames);
        CHECK(std::fabs(altFraction(36000) - altFraction(30000)) > 0.08, "30,000 and 36,000 ft are apart (%.2f vs %.2f)", altFraction(30000), altFraction(36000));
    }

    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
