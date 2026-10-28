// Aeola — timbre bank: 14 hand-designed spectra (original, procedural — no third-party data).
// A timbre is a list of partials (frequency ratio + amplitude), optional vowel/body formants (fixed in Hz),
// a breath-noise amount, and how lively the partials are. Two timbres can be morphed continuously.
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace aeola
{
constexpr int kPartials = 24;
constexpr int kBPartials = 8;

struct Timbre
{
    const char* name = ""; const char* tip = "";
    float ratio[kPartials]; float amp[kPartials];
    float noise = 0.0f;        // breath noise (0..1)
    float lifeDepth = 0.3f;    // how much each partial wanders in level/frequency
    float autoBeat = 0.0f;     // built-in chorus (Hz) on top of the Beat control
    int nForm = 0; float fF[3] = {}, fB[3] = {}, fG[3] = {};   // formants: centre Hz, bandwidth Hz, gain
    float formFloor = 1.0f;    // level between formants (1 = formants off)
};

inline float formantGain(const Timbre& t, double f)
{
    if (t.nForm == 0) return 1.0f;
    float g = t.formFloor;
    for (int i = 0; i < t.nForm; ++i) { double d = (f - t.fF[i]) / t.fB[i]; g += t.fG[i] * (float) std::exp(-0.5 * d * d); }
    return g;
}

inline std::vector<Timbre> buildTimbres()
{
    std::vector<Timbre> v;
    auto blank = [](const char* name, const char* tip) { Timbre t; t.name = name; t.tip = tip; for (int k = 0; k < kPartials; ++k) { t.ratio[k] = (float) (k + 1); t.amp[k] = 0; } return t; };
    auto harmonic = [&](const char* name, const char* tip, auto ampOfH) { Timbre t = blank(name, tip); for (int k = 0; k < kPartials; ++k) t.amp[k] = (float) ampOfH(k + 1); return t; };

    { auto t = harmonic("Glass", "Clean and bell-like; the original Aeola sound.", [](int h) { return std::pow((double) h, -1.6); }); t.lifeDepth = 0.35f; v.push_back(t); }
    { auto t = blank("Organ", "Drawbar organ: stable and hollow.");
      const float d[][2] = { {1,1.0f},{2,0.8f},{3,0.6f},{4,0.5f},{6,0.35f},{8,0.3f},{10,0.12f},{12,0.1f} };
      for (auto& e : d) t.amp[(int) e[0] - 1] = e[1]; t.lifeDepth = 0.12f; v.push_back(t); }
    auto choir = [&](const char* name, const char* tip, float f1, float f2, float f3) {
        auto t = harmonic(name, tip, [](int h) { return 1.0 / h; });
        t.nForm = 3; t.fF[0] = f1; t.fF[1] = f2; t.fF[2] = f3; t.fB[0] = 90; t.fB[1] = 130; t.fB[2] = 220; t.fG[0] = 1.0f; t.fG[1] = 0.7f; t.fG[2] = 0.35f; t.formFloor = 0.05f;
        t.noise = 0.10f; t.lifeDepth = 0.5f; t.autoBeat = 0.35f; return t; };
    v.push_back(choir("Choir Ah", "Open vowel 'ah'; voices that follow the pitch.", 730, 1090, 2440));
    v.push_back(choir("Choir Oh", "Rounded vowel 'oh'; darker and warmer.", 570, 840, 2410));
    v.push_back(choir("Choir Ee", "Bright vowel 'ee'; thin and shining.", 270, 2290, 3010));
    { auto t = harmonic("Bowed", "Bowed strings with a body resonance.", [](int h) { return 1.0 / h; });
      t.nForm = 3; t.fF[0] = 300; t.fF[1] = 1000; t.fF[2] = 2500; t.fB[0] = 150; t.fB[1] = 250; t.fB[2] = 500; t.fG[0] = 0.8f; t.fG[1] = 0.6f; t.fG[2] = 0.4f; t.formFloor = 0.25f;
      t.noise = 0.16f; t.lifeDepth = 0.35f; t.autoBeat = 0.2f; v.push_back(t); }
    { auto t = harmonic("Reed", "Clarinet-like: odd overtones, woody.", [](int h) { return (h & 1) ? 1.0 / h : 0.06 / h; }); t.noise = 0.06f; t.lifeDepth = 0.25f; v.push_back(t); }
    { auto t = harmonic("Flute", "Breathy and pure.", [](int h) { return std::pow((double) h, -2.4); }); t.noise = 0.5f; t.lifeDepth = 0.25f; v.push_back(t); }
    { auto t = harmonic("Brass", "Swelling overtones like a horn section.", [](int h) { return h * std::exp(-h / 3.5); }); t.lifeDepth = 0.2f; t.noise = 0.04f; v.push_back(t); }
    { auto t = blank("Bell", "Inharmonic bell with a low hum tone.");
      const float r[] = { 0.5f, 1.0f, 1.19f, 1.56f, 2.0f, 2.51f, 2.66f, 3.01f, 4.07f, 4.16f, 5.43f, 6.8f, 8.1f, 9.4f, 11.0f };
      const float a[] = { 0.6f, 1.0f, 0.7f, 0.5f, 0.45f, 0.35f, 0.3f, 0.28f, 0.2f, 0.2f, 0.15f, 0.12f, 0.1f, 0.08f, 0.06f };
      for (int k = 0; k < 15; ++k) { t.ratio[k] = r[k]; t.amp[k] = a[k]; } t.lifeDepth = 0.4f; v.push_back(t); }
    { auto t = blank("Bowl", "Singing bowl: pairs of modes that beat slowly.");
      const float r[] = { 1.0f, 1.004f, 2.71f, 2.73f, 5.15f, 5.18f, 8.4f, 8.45f, 12.1f };
      const float a[] = { 1.0f, 0.9f, 0.5f, 0.45f, 0.3f, 0.28f, 0.15f, 0.14f, 0.08f };
      for (int k = 0; k < 9; ++k) { t.ratio[k] = r[k]; t.amp[k] = a[k]; } t.lifeDepth = 0.6f; v.push_back(t); }
    { auto t = blank("Metal", "Dense, inharmonic and cold.");
      const float r[] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.21f, 3.89f, 4.45f, 5.2f, 6.1f, 7.3f };
      for (int k = 0; k < 10; ++k) { t.ratio[k] = r[k]; t.amp[k] = (float) std::pow(1.0 + k, -1.1); } t.lifeDepth = 0.3f; v.push_back(t); }
    { auto t = harmonic("Pad", "Wide, slowly chorusing pad.", [](int h) { return std::pow((double) h, -0.9) * ((h & 1) ? 1.0 : 0.55) * std::exp(-h / 14.0); }); t.autoBeat = 0.7f; t.lifeDepth = 0.4f; t.noise = 0.03f; v.push_back(t); }
    { auto t = blank("Breath", "Mostly air with a faint pitch.");
      const float a[] = { 0.12f, 0.22f, 0.2f, 0.15f, 0.1f, 0.07f, 0.04f, 0.03f }; for (int k = 0; k < 8; ++k) t.amp[k] = a[k]; t.noise = 1.0f; t.lifeDepth = 0.5f; v.push_back(t); }
    { auto t = harmonic("Saw", "Bright and buzzy: every overtone, falling slowly.", [](int h) { return 1.0 / h; }); t.lifeDepth = 0.3f; t.autoBeat = 0.25f; v.push_back(t); }
    { auto t = harmonic("Pulse", "Hollow, nasal pulse wave (25% duty); every 4th overtone missing.", [](int h) { return std::fabs(std::sin(3.141592653589793 * h / 4.0)) / h; }); t.lifeDepth = 0.35f; t.autoBeat = 0.2f; v.push_back(t); }
    { auto t = blank("Grit", "Saw with stretched, slightly out-of-tune overtones and a rasp of noise.");
      for (int k = 0; k < kPartials; ++k) { double h = k + 1; t.ratio[k] = (float) (h * std::sqrt(1.0 + 0.0009 * h * h)); t.amp[k] = (float) std::pow(h, -0.85); }
      t.noise = 0.4f; t.lifeDepth = 0.55f; t.autoBeat = 0.5f; v.push_back(t); }
    { auto t = blank("Rust", "Corroded metal: dense inharmonic partials, noisy and restless.");
      const float r[] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.21f, 3.89f, 4.45f, 5.2f, 6.1f, 7.3f, 8.7f, 10.2f, 11.9f, 13.9f, 16.4f, 18.1f };
      for (int k = 0; k < 16; ++k) { t.ratio[k] = r[k] * (1.0f + 0.012f * std::sin(1.7f * k)); t.amp[k] = (float) std::pow(1.0 + k, -0.8); }
      t.noise = 0.55f; t.lifeDepth = 0.7f; t.autoBeat = 0.4f; v.push_back(t); }
    return v;
}
inline const std::vector<Timbre>& timbres() { static const std::vector<Timbre> t = buildTimbres(); return t; }
// Share (0..1) of a timbre's overtone amplitude (strongest partial excluded, so a dominant fundamental cannot hide
// an inharmonic upper spectrum) that sits near whole-number multiples of the fundamental.
inline float timbreHarmonicity(const Timbre& t)
{
    int top = 0; for (int k = 1; k < kPartials; ++k) if (t.amp[k] > t.amp[top]) top = k;
    double tot = 0, har = 0;
    for (int k = 0; k < kPartials; ++k)
    {
        if (k == top || t.amp[k] <= 0) continue;
        double a = t.amp[k]; tot += a;
        double r = t.ratio[k], n = std::floor(r + 0.5), dev = std::fabs(r - n) / std::max(1.0, n);
        if (n >= 1 && dev < 0.015) har += a;
    }
    return tot > 1e-12 ? (float) (har / tot) : 1.0f;
}
inline int numTimbres() { return (int) timbres().size(); }

struct SpecIn
{
    int a = 0, b = 0; float morph = 0, colour = 0.5f, oddEven = 0, inharm = 0, life = 0.4f;
    double f0 = 220.0, sr = 44100.0;
    const float* lifePh = nullptr;   // per-partial phase 0..1 (nullptr = no movement, used for previews)
};

// Blend two timbres, apply colour tilt, odd/even, inharmonicity, formants and life; returns RMS-normalised amplitudes.
inline void computeSpectrum(const SpecIn& in, float* ratio, float* amp)
{
    const Timbre& A = timbres()[(size_t) std::min(in.a, numTimbres() - 1)];
    const Timbre& B = timbres()[(size_t) std::min(in.b, numTimbres() - 1)];
    const float m = in.morph, tilt = (in.colour - 0.5f) * 3.2f;
    double sum = 0;
    for (int k = 0; k < kPartials; ++k)
    {
        float r = A.ratio[k] + (B.ratio[k] - A.ratio[k]) * m;
        r *= std::sqrt(1.0f + in.inharm * 0.02f * r * r);
        float life = 1.0f, fm = 1.0f;
        if (in.lifePh)
        {
            float depth = (A.lifeDepth + (B.lifeDepth - A.lifeDepth) * m) * in.life;
            float s = std::sin(6.2831853f * in.lifePh[k]);
            life = std::max(0.0f, 1.0f + depth * 0.7f * s);
            fm = 1.0f + 0.0007f * depth * std::cos(6.2831853f * in.lifePh[k] * 1.37f);
        }
        r *= fm;
        double fk = in.f0 * r;
        if (fk > in.sr * 0.45 || fk < 16.0 || r <= 0.0f) { ratio[k] = r; amp[k] = 0; continue; }
        float aa = A.amp[k] * formantGain(A, fk), bb = B.amp[k] * formantGain(B, fk);
        float a = (aa + (bb - aa) * m) * std::pow(std::max(r, 0.5f), tilt) * life;
        float oe = (k & 1) ? (1.0f - in.oddEven) : (1.0f + in.oddEven);        // index even == odd harmonic
        a *= std::max(0.0f, oe);
        ratio[k] = r; amp[k] = a; sum += (double) a * a;
    }
    float g = sum > 1e-12 ? (float) (0.6 / std::sqrt(sum)) : 0.0f;
    for (int k = 0; k < kPartials; ++k) amp[k] *= g;
}
} // namespace aeola
