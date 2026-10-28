// Aeola — Sky mapper: turns aircraft into musically safe voice orders.
// Musical Guard: key/scale lock, interval-class + roughness gate (Consonance), anchor drone,
// ghost voices for quiet skies, slow "evolve" register/density movement.
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include "Sky.h"

namespace aeola
{
class SkyMapper
{
public:
    void reset() { for (auto& s : st) s = State(); }

    // `now` = epoch seconds of the frame, `dtNow` = used for evolve phase
    void map(const Frame& frame, const SkySettings& cfg, double now, MapResult& out)
    {
        out = MapResult();
        const bool soloing = cfg.classes && cfg.soloMask != 0;
        const double radius = std::max(5.0, cfg.radiusNm);

        // ---- evolve: slow register / density / brightness movement (deterministic from seed + time) ----
        double period = std::max(60.0, (double) cfg.evolvePeriodMin * 60.0);
        double p1 = 6.2831853 * (now / period + hash01(cfg.seed, 11)), p2 = 6.2831853 * (now / (period * 0.618) + hash01(cfg.seed, 12));
        double ev1 = 0.55 * std::sin(p1) + 0.45 * std::sin(p1 * 0.37 + 1.3), ev2 = std::sin(p2);
        int regShiftDeg = (int) std::lround(ev1 * 2.0 * cfg.evolveDepth);
        float brightEv = (float) (ev2 * 0.18 * cfg.evolveDepth);
        float densityF = (float) (1.0 - 0.35 * cfg.evolveDepth * (0.5 - 0.5 * std::sin(p2 * 0.7 + 0.4)));
        out.regShiftDeg = regShiftDeg;
        const int anchors = (cfg.anchor && !(cfg.classes && cfg.soloMask != 0)) ? (cfg.anchorFifth ? 2 : 1) : 0;
        (void) densityF; (void) anchors;     // the Aircraft setting is exact: Evolve moves register and brightness, it no longer thins the sky
        const int effMax = std::max(1, std::min(cfg.maxAircraft, cfg.voiceCap));      // anchors and ghosts get their own voices on top (the processor adds them to the engine limit)

        const auto& sc = scales()[cfg.scale];
        auto shiftByDegrees = [&](int note, int deg) {          // move `deg` scale steps up/down
            int n = note;
            for (int i = 0; i < std::abs(deg); ++i)
            {
                int dir = deg > 0 ? 1 : -1, m = n + dir;
                while (!inScale(m, cfg.rootPc, cfg.scale)) m += dir;
                n = m;
            }
            return n;
        };
        const int lowLimit = cfg.baseNote - 12, highLimit = cfg.baseNote + (int) std::lround(cfg.rangeOct * 12.0f) + 14;
        (void) sc;

        const double allowed = 0.10 + (1.0 - cfg.consonance) * 0.95;
        int assigned[kMaxOrders]; int nAssigned = 0;
        ++gen;
        const bool measured = cfg.harmonicity < 0.6f && cfg.nPart > 0;
        auto pen = [&](int a, int b) -> double {
            if (a == b) return 0.0;
            if (!measured) return guardPenalty(a, b);
            if (a > b) std::swap(a, b);
            if (a < 0 || b > 127) return 1.0;
            if (memoGen[a][b] == gen) return memo[a][b];
            double base = guardPenalty(a, b), p;
            if (cfg.harmonicity < 0.4f && b - a < 5) p = 1.0;
            else
            {   // excess roughness over an octave, scaled so that a minor second counts as 1
                double R = specRough(a, b, cfg.partRatio, cfg.partAmp, cfg.nPart);
                double Roct = specRough(a, a + 12, cfg.partRatio, cfg.partAmp, cfg.nPart), Rm2 = specRough(a, a + 1, cfg.partRatio, cfg.partAmp, cfg.nPart);
                double den = std::max(0.004, Rm2 - Roct), ex = std::min(1.0, std::max(0.0, (R - Roct) / den));
                p = std::max(ex * 0.9, 0.5 * base);
            }
            memo[a][b] = (float) p; memoGen[a][b] = gen; return p;
        };
        auto penaltyVs = [&](int note) { double worst = 0; for (int i = 0; i < nAssigned; ++i) worst = std::max(worst, pen(note, assigned[i])); return worst; };
        // aircraft prefer their own note: a second aircraft on a note another aircraft already plays counts as a mild clash, so a busy sky
        // spreads into a chord instead of stacking in unison (at low Consonance unisons are allowed again)
        int acNotes[kMaxOrders]; int nAcNotes = 0;
        auto penaltyAc = [&](int note) { double w = penaltyVs(note); for (int i = 0; i < nAcNotes; ++i) if (acNotes[i] == note) w = std::max(w, 0.45); return w; };
        auto push = [&](uint32_t id, int note, float level, float pan, float bright, float breath, float vib, bool anchor, bool ghost) -> VoiceOrder* {
            if (out.n >= kMaxOrders) return nullptr;
            out.v[out.n] = VoiceOrder(); auto& o = out.v[out.n++];
            o.id = id; o.note = note; o.level = level; o.pan = pan; o.brightOff = bright; o.breath = breath; o.vib = vib; o.anchor = anchor; o.ghost = ghost;
            if (nAssigned < kMaxOrders) assigned[nAssigned++] = note;
            return &o;
        };

        // ---- anchor drone ----
        int rootNote = cfg.baseNote - 12;
        while ((rootNote % 12 + 12) % 12 != cfg.rootPc) ++rootNote;          // nearest root at/above base-12
        if (rootNote > cfg.baseNote) rootNote -= 12;
        if (cfg.anchor && !soloing)
        {
            push(5000, rootNote, 0.55f, 0.0f, -0.15f, 11.3f * cfg.loopStretch, 0.0f, true, false);
            if (cfg.anchorFifth && inScale(rootNote + 7, cfg.rootPc, cfg.scale)) push(5001, rootNote + 7, 0.38f, 0.05f, -0.1f, 13.1f * cfg.loopStretch, 0.0f, true, false);
        }

        // ---- order aircraft by distance (closest = most important) ----
        int idx[kMaxAc]; int n = frame.count;
        for (int i = 0; i < n; ++i) idx[i] = i;
        const bool K = cfg.classes;
        auto classAudible = [&](const Aircraft& a) { if (!K) return true; uint32_t bit = 1u << std::max(0, std::min(kNumClasses - 1, a.category)); if (cfg.muteMask & bit) return false; return !soloing || (cfg.soloMask & bit) != 0; };
        auto catOf = [&](const Aircraft& a) { return K ? a.category : a.legacyCat; };
        auto featured = [&](int c) { return K && (c == kCargo || c == kHeavy || c == kHeli || c == kMil); };   // the classes that must not vanish
        auto sortKey = [&](const Aircraft& a) { return a.distNm * (featured(a.category) ? 0.55f : 1.0f); };
        std::sort(idx, idx + n, [&](int a, int b) { return sortKey(frame.ac[a]) < sortKey(frame.ac[b]); });
        const float contrast = std::max(0.0f, std::min(1.0f, cfg.contrast));
        auto octaveOf = [&](const Aircraft& a) -> double {
            if (!K) { int c = a.legacyCat; return (c == kHeavy || c == kCargo) ? -12.0 : (c == kLight ? 12.0 : 0.0); }
            return contrast >= 0.34f ? 12.0 * cfg.cls[std::max(0, std::min(kNumClasses - 1, a.category))].octave : 0.0;
        };

        State next[kMaxAc];
        int used = 0;
        auto rec = [&](uint32_t id, int status, int want, int note, float swell = 0.0f) { if (out.nOut < kMaxAc) out.outcome[out.nOut++] = { id, (uint8_t) status, want, note, swell }; };
        int oi = 0;
        for (; oi < n && used < effMax; ++oi)
        {
            const Aircraft& a = frame.ac[idx[oi]];
            if (!classAudible(a)) { rec(a.id, 5, 0, 0); continue; }
            double f = altFraction(a.altFt);
            double octShift = octaveOf(a);
            double pitchReal = cfg.baseNote + cfg.rangeOct * 12.0 * f + octShift;
            int cand = nearestInScale(pitchReal, cfg.rootPc, cfg.scale);
            if (regShiftDeg) cand = shiftByDegrees(cand, regShiftDeg);

            // hysteresis: keep the previous pitch while the real altitude has not moved far from it
            State* prev = find(a.id);
            if (prev && prev->note > 0 && std::fabs((pitchReal + (cand - nearestInScale(pitchReal, cfg.rootPc, cfg.scale))) - prev->note) < 1.3) cand = prev->note;

            int chosen = cand; bool shifted = false;
            const bool feat = featured(a.category);
            const double allowedA = feat ? std::min(1.0, allowed * 1.25) : allowed;
            if (penaltyAc(cand) > allowedA)
            {   // look for the least clashing alternative: scale neighbours and octave displacements
                double best = 1e9; int bestNote = -1;
                for (int dd = -3; dd <= 3; ++dd)
                    for (int oct = -1; oct <= 1; ++oct)
                    {
                        if (!dd && !oct) continue;
                        int nn = shiftByDegrees(cand, dd) + 12 * oct;
                        if (nn < lowLimit || nn > highLimit) continue;
                        double pen = penaltyAc(nn);
                        double cost = pen + (feat ? 0.03 * std::abs(dd) + 0.01 * std::abs(oct) : 0.02 * std::abs(dd) + 0.03 * std::abs(oct));
                        if (pen <= allowedA && cost < best) { best = cost; bestNote = nn; }
                    }
                if (bestNote >= 0) { chosen = bestNote; shifted = penaltyVs(cand) > allowedA; }      // stepping aside from a unison is not a clash: not shown as "moved"
                else { ++out.rejected; rec(a.id, 3, cand, cand); continue; }
            }
            if (shifted) ++out.shifted;
            double distN = std::min(1.0, a.distNm / radius);
            double eastNm = (a.lon - cfg.apLon) * 60.0 * std::cos(cfg.apLat * M_PI / 180.0), northNm = (a.lat - cfg.apLat) * 60.0;
            static const float primes[8] = { 5.3f, 5.9f, 6.7f, 7.3f, 7.9f, 8.3f, 8.9f, 9.7f };
            VoiceOrder* vo = nullptr; float swellAmt = 0.0f;
            if (!K)
            {   // ---- plain voices (classes off: only the Guard tests use this) ----
                int lc = a.legacyCat;
                float catGain = lc == kHeli ? 0.8f : (lc == kHeavy ? 1.0f : 0.9f);
                float level = (float) std::max(0.3, 1.0 - 0.55 * distN) * catGain * 0.8f;
                float pan = (float) std::max(-1.0, std::min(1.0, eastNm / radius));
                float bright = (float) ((0.5 - distN) * 0.3 + (lc == kHeli ? 0.18 : 0.0) + brightEv);
                float breath = primes[mix64(a.id) % 8] * cfg.loopStretch * (lc == kCargo ? 1.4f : 1.0f);
                float vib = (float) std::min(1.0, a.gsKt / 500.0) * 0.5f;
                vo = push(kAcVoiceBit | a.id, chosen, level, pan, bright, breath, vib, false, false);
            }
            else
            {   // ---- Classes mapping (v1.3) ----
                const int c = std::max(0, std::min(kNumClasses - 1, a.category)); const ClassStyle& st = cfg.cls[c];
                if (cfg.swell)
                {   double swR = std::max(2.0, radius * 0.08); double x = std::max(0.0, 1.0 - a.distNm / swR); swellAmt = (float) (x * x * (3.0 - 2.0 * x)); }
                float levelDb = st.levelDb * contrast;
                float level = (float) std::max(0.12, std::pow(1.0 - distN, 1.4)) * 0.9f * 0.8f * std::pow(10.0f, levelDb / 20.0f) * (1.0f + 0.6f * swellAmt);     // near = loud, the rim about -18 dB
                float pan = (float) std::max(-1.0, std::min(1.0, eastNm / (radius * 0.7)));
                static const float classBright[kNumClasses] = { 0.08f, 0.02f, 0.0f, -0.08f, 0.18f, -0.10f, 0.12f, 0.0f, 0.05f };
                float bright = (float) ((0.5 - distN) * 0.5 - 0.10 * std::max(-1.0, std::min(1.0, northNm / radius)) + brightEv) + classBright[c] * contrast + 0.15f * swellAmt;
                static const float classBreath[kNumClasses] = { 0.8f, 1.0f, 1.0f, 1.4f, 0.6f, 1.8f, 0.9f, 0.8f, 1.0f };
                float breath = primes[mix64(a.id) % 8] * cfg.loopStretch * std::pow(classBreath[c], contrast);
                float vib = (float) std::min(1.0, a.gsKt / 500.0) * (c == kHeli ? 0.8f : 0.5f);
                vo = push(kAcVoiceBit | a.id, chosen, level, pan, bright, breath, vib, false, false);
                if (vo)
                {
                    vo->cls = c;
                    if (st.timbre >= 0 && contrast > 0.05f) { vo->timbre = st.timbre; vo->blend = 1.0f; }       // a class with its own timbre is that timbre, not a blend with the base sound
                    vo->attackMul = vo->releaseMul = std::pow(std::max(0.2f, st.attackMul), contrast);
                    vo->chopDepth = st.chop * contrast;
                    vo->chopRate = c == kHeli ? 2.6f + std::min(150.0f, a.gsKt) / 55.0f : (c == kProp ? 5.0f + (float) (mix64(a.id) % 100) * 0.012f : 4.0f);
                }
            }
            rec(a.id, shifted ? 2 : 1, cand, chosen, swellAmt);
            if (nAcNotes < kMaxOrders) acNotes[nAcNotes++] = chosen;
            next[used].id = a.id; next[used].note = chosen; ++used;
            ++out.accepted;
        }
        for (int i = 0; i < kMaxAc; ++i) st[i] = (i < used) ? next[i] : State();
        // aircraft beyond the voice limit: record the note their altitude would give
        for (; oi < n; ++oi)
        {
            const Aircraft& a = frame.ac[idx[oi]];
            if (!classAudible(a)) { rec(a.id, 5, 0, 0); continue; }
            double f = altFraction(a.altFt);
            int cand = nearestInScale(cfg.baseNote + cfg.rangeOct * 12.0 * f + octaveOf(a), cfg.rootPc, cfg.scale);
            if (regShiftDeg) cand = shiftByDegrees(cand, regShiftDeg);
            rec(a.id, 4, cand, cand);
        }

        // ---- ghost voices keep a quiet sky from being one lonely note ----
        int have = out.accepted;
        if (have < cfg.ghostMin && !soloing)
        {
            static const int pref[] = { 7, 0, 2, 4, 5, 9, 1, 3, 6, 8 };   // scale-degree indices to try, consonant first
            long long epochSlot = (long long) std::floor(now / 45.0);
            int want = cfg.ghostMin - have;
            for (int g = 0; g < 6 && want > 0; ++g)
            {
                int deg = pref[(g + (int) (mix64((uint64_t) epochSlot ^ cfg.seed) % 4)) % 10] % sc.n;
                int note = cfg.baseNote + 12 * (g % 2) + sc.deg[deg] - 12 * ((cfg.baseNote + sc.deg[deg]) % 12 > 6 ? 0 : 0);
                while ((note % 12 + 12) % 12 != (cfg.rootPc + sc.deg[deg]) % 12) ++note;
                if (note > highLimit) note -= 12;
                if (penaltyVs(note) > allowed) continue;
                push(6000 + (uint32_t) g, note, cfg.ghostLevel * 0.7f, (float) (hash01(cfg.seed, 40 + g) * 1.6 - 0.8), 0.0f, 9.7f * cfg.loopStretch, 0.0f, false, true);
                ++out.ghosts; --want;
            }
        }

        // ---- roughness meter (average penalty over all sounding pairs, 0..1) ----
        double sum = 0; int pairs = 0;
        for (int i = 0; i < out.n; ++i) for (int j = i + 1; j < out.n; ++j) { sum += pen(out.v[i].note, out.v[j].note); ++pairs; }
        out.roughness = pairs ? (float) std::min(1.0, sum / pairs * 2.0) : 0.0f;
    }

private:
    struct State { uint32_t id = 0; int note = 0; };
    float memo[128][128] = {}; uint32_t memoGen[128][128] = {}; uint32_t gen = 0;
    State st[kMaxAc];
    State* find(uint32_t id) { for (auto& s : st) if (s.id == id && s.note > 0) return &s; return nullptr; }
};
} // namespace aeola
