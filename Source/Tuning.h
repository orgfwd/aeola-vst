// Aeola — tuning systems, scales and a roughness model (pure C++).
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later (derived from Aeola, https://github.com/orgfwd/aeola)
#pragma once
#include <cmath>
#include <algorithm>

namespace aeola
{
enum TuningSystem { kJust5 = 0, kPythagorean, kHarmonic, kMeantone, kNumSystems };

inline const char* const* tuningSystemNames()
{
    static const char* n[] = { "Just (5-limit)", "Pythagorean", "Harmonic series", "Meantone (1/4 comma)" };
    return n;
}

// Interval size in cents above the root for 0..11 semitones.
inline double pureCents(int system, int semis12)
{
    static const double just[12]  = { 0.0, 111.731, 203.910, 315.641, 386.314, 498.045, 590.224, 701.955, 813.686, 884.359, 1017.596, 1088.269 };
    static const double pyth[12]  = { 0.0, 90.225, 203.910, 294.135, 407.820, 498.045, 611.730, 701.955, 792.180, 905.865, 996.090, 1109.775 };
    static const double harm[12]  = { 0.0, 111.731, 203.910, 266.871, 386.314, 498.045, 582.512, 701.955, 813.686, 884.359, 968.826, 1088.269 };
    static const double mean[12]  = { 0.0, 76.049, 193.157, 310.265, 386.314, 503.422, 579.471, 696.578, 772.627, 889.735, 1006.843, 1082.892 };
    const double* t = system == kPythagorean ? pyth : system == kHarmonic ? harm : system == kMeantone ? mean : just;
    return t[((semis12 % 12) + 12) % 12];
}

// Deviation (cents) from equal temperament for the interval `semis` above the root.
inline double pureOffsetCents(int system, int semis)
{
    int r = ((semis % 12) + 12) % 12;
    return pureCents(system, r) - 100.0 * r;
}

struct ScaleDef { const char* name; int n; int deg[12]; };

inline const ScaleDef* scales()
{
    static const ScaleDef s[] = {
        { "Dorian",            7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Minor",             7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Major",             7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Lydian",            7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",        7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Phrygian",          7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Pentatonic minor",  5, { 0, 3, 5, 7, 10 } },
        { "Pentatonic major",  5, { 0, 2, 4, 7, 9 } },
        { "Hirajoshi",         5, { 0, 2, 3, 7, 8 } },
        { "Whole tone",        6, { 0, 2, 4, 6, 8, 10 } },
        { "Open fifths",       3, { 0, 7, 2 } },        // root, fifth, ninth
        { "Chromatic",        12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return s;
}
constexpr int kNumScales = 12;
inline const char* const* noteNames()
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline double midiToHz(double note) { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }

// Sethares' parameterisation of the Plomp–Levelt curve.
inline double pairRough(double f1, double f2, double a1, double a2)
{
    if (f1 > f2) std::swap(f1, f2);
    double s = 0.24 / (0.0207 * f1 + 18.96);
    double d = f2 - f1;
    return a1 * a2 * (std::exp(-3.5 * s * d) - std::exp(-5.75 * s * d));
}

// Roughness between two notes (MIDI note numbers), 6 harmonics at 1/h amplitude.
inline double noteRough(int noteA, int noteB)
{
    if (noteA == noteB) return 0.0;
    double fa = midiToHz(noteA), fb = midiToHz(noteB), r = 0.0;
    for (int i = 1; i <= 6; ++i)
        for (int j = 1; j <= 6; ++j)
            r += pairRough(fa * i, fb * j, 1.0 / i, 1.0 / j);
    return r;
}

// Plomp-Levelt roughness between two notes whose overtones are given explicitly (ratio, amplitude); strongest 8 partials only.
inline double specRough(int noteA, int noteB, const float* ratio, const float* amp, int n)
{
    int idx[24], m = 0; for (int i = 0; i < n && i < 24; ++i) if (amp[i] > 1e-4f) idx[m++] = i;
    std::sort(idx, idx + m, [&](int x, int y) { return amp[x] > amp[y]; }); m = std::min(m, 8);
    double fa = midiToHz(noteA), fb = midiToHz(noteB), r = 0;
    for (int i = 0; i < m; ++i) for (int j = 0; j < m; ++j) r += pairRough(fa * ratio[idx[i]], fb * ratio[idx[j]], amp[idx[i]], amp[idx[j]]);
    return r;
}

// Is `note` (absolute MIDI) in the scale rooted at pitch class `root`?
inline bool inScale(int note, int root, int scale)
{
    const auto& s = scales()[scale];
    int pc = (((note - root) % 12) + 12) % 12;
    for (int i = 0; i < s.n; ++i) if (s.deg[i] == pc) return true;
    return false;
}

// Nearest in-scale note to a real-valued pitch.
inline int nearestInScale(double pitch, int root, int scale)
{
    int c = (int) std::lround(pitch);
    for (int d = 0; d < 12; ++d)
    {
        double bestDist = 1e9; int best = c;
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            int n = c + sgn * d;
            if (inScale(n, root, scale) && std::abs(n - pitch) < bestDist) { bestDist = std::abs(n - pitch); best = n; }
        }
        if (bestDist < 1e8) return best;
    }
    return c;
}

// Musical safety net used by the Sky mapper: how "clashing" are two notes (0 = no problem, ~1 = harsh).
// Rule-based interval-class penalty (robust at every register) plus a little Plomp-Levelt roughness
// in the mid/high register where that model is reliable.
inline double guardPenalty(int a, int b)
{
    if (a == b) return 0.0;
    int d = std::abs(a - b), cls = d % 12;
    static const double pen[12] = { 0.00, 0.80, 0.42, 0.18, 0.12, 0.06, 0.55, 0.03, 0.12, 0.14, 0.38, 0.75 };
    double p = pen[cls];
    if (d >= 24) p *= 0.35; else if (d >= 12) p *= 0.6;       // wide spacing is gentler
    int lo = std::min(a, b);
    if (lo >= 55) p = std::min(1.0, p + 0.3 * noteRough(a, b));
    return p;
}
} // namespace aeola
