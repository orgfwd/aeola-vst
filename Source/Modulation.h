// Aeola — modulation: two LFOs (free or tempo-synced), a slow random walk and a 4-slot routing matrix.
// Pure C++ (no JUCE) so it can be unit-tested. Evaluated at control rate by the processor.
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>
#include <string>
#include "Sky.h"

namespace aeola
{
// New entries are only ever APPENDED: projects store the list index, so the order of existing entries must never change.
enum ModSrc { SrcNone = 0, SrcLfo1, SrcLfo2, SrcWalk, SrcAircraft, SrcAltitude, SrcProximity, SrcSpeed, SrcModWheel, SrcVelocity,
              SrcAftertouch, SrcPitchBend, SrcTension, SrcLight, SrcMotion, SrcSpace,
              SrcClassCount,                                   // 1.11: per aircraft class, how many are in range (0 = none, 1 = four or more)
              SrcClassAlt = SrcClassCount + kNumClasses,       // 1.11: per class, average altitude (1 = 40,000 ft)
              kNumSrc = SrcClassAlt + kNumClasses };
enum ModDst { DstNone = 0, DstColour, DstMorph, DstMorphMotion, DstLife, DstAir, DstBeat, DstDrift, DstCutoff, DstReso, DstDrive,
              DstPanSpread, DstWidth, DstReverbMix, DstOutput,
              DstClassLevel,                                   // 1.11: per class level (100 % = the whole -12..+6 dB range)
              DstClassOctave = DstClassLevel + kNumClasses,    // 1.11: per class octave (100 % = the whole -2..+2 range, so 25 % = one octave)
              DstInharm = DstClassOctave + kNumClasses, DstOddEven, DstBreath, DstNorth, DstConsonance, DstEvolve,   // 1.12
              DstAttack, DstRelease, DstGlide, DstDriftRate, DstLowCut, DstWarmth, DstFilterMix, DstKeyTrack, DstRevSize, DstRevDamp,   // 1.13
              DstRange, DstGhostLevel, DstStretch, DstLfo1Rate, DstLfo2Rate,
              DstClassSwell,                                   // 1.13: per class swell time (attack/release multiplier, 100 % = 0.25..4)
              DstClassChop = DstClassSwell + kNumClasses,      // 1.13: per class chop
              kNumDst = DstClassChop + kNumClasses };
constexpr int kModSlots = 8;        // 1.12: was 4

inline const char* const* modSrcNames()
{
    static const std::array<std::string, kNumSrc> s = [] {
        std::array<std::string, kNumSrc> a;
        const char* base[SrcClassCount] = { "None", "LFO 1", "LFO 2", "Random walk", "Aircraft count", "Aircraft altitude", "Aircraft proximity",
                                            "Aircraft speed", "Mod wheel", "Velocity", "Aftertouch", "Pitch bend", "Tension", "Light", "Motion", "Space" };
        for (int i = 0; i < SrcClassCount; ++i) a[(size_t) i] = base[i];
        for (int c = 0; c < kNumClasses; ++c) { a[(size_t) (SrcClassCount + c)] = std::string(className(c)) + " count"; a[(size_t) (SrcClassAlt + c)] = std::string(className(c)) + " altitude"; }
        return a; }();
    static const std::array<const char*, kNumSrc> p = [] { std::array<const char*, kNumSrc> a; for (size_t i = 0; i < a.size(); ++i) a[i] = s[i].c_str(); return a; }();
    return p.data();
}
inline const char* const* modDstNames()
{
    static const std::array<std::string, kNumDst> s = [] {
        std::array<std::string, kNumDst> a;
        const char* base[DstClassLevel] = { "None", "Colour", "Morph", "Morph motion", "Life", "Air", "Beat", "Drift", "Cutoff", "Resonance", "Drive",
                                            "Pan spread", "Width", "Reverb mix", "Output" };
        for (int i = 0; i < DstClassLevel; ++i) a[(size_t) i] = base[i];
        for (int c = 0; c < kNumClasses; ++c) { a[(size_t) (DstClassLevel + c)] = std::string(className(c)) + " gain"; a[(size_t) (DstClassOctave + c)] = std::string(className(c)) + " octave"; }
        const char* more[] = { "Inharmonic", "Odd/Even", "Breath", "North", "Consonance", "Evolve" };
        for (int i = 0; i < 6; ++i) a[(size_t) (DstInharm + i)] = more[i];
        const char* more2[] = { "Attack", "Release", "Glide", "Drift rate", "Low cut", "Warmth", "Filter mix", "Key track", "Reverb size", "Reverb damp",
                                "Range", "Ghost level", "Loop stretch", "LFO 1 rate", "LFO 2 rate" };
        for (int i = 0; i < 15; ++i) a[(size_t) (DstAttack + i)] = more2[i];
        for (int c = 0; c < kNumClasses; ++c) { a[(size_t) (DstClassSwell + c)] = std::string(className(c)) + " swell time"; a[(size_t) (DstClassChop + c)] = std::string(className(c)) + " chop"; }
        return a; }();
    static const std::array<const char*, kNumDst> p = [] { std::array<const char*, kNumDst> a; for (size_t i = 0; i < a.size(); ++i) a[i] = s[i].c_str(); return a; }();
    return p.data();
}
inline const char* const* lfoShapeNames()
{
    static const char* n[6] = { "Sine", "Triangle", "Saw", "Square", "Smooth random", "Stepped random" };
    return n;
}
inline const char* const* lfoDivNames()
{
    static const char* n[7] = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
    return n;
}
inline double lfoDivBeats(int div) { static const double b[7] = { 16, 8, 4, 2, 1, 0.5, 0.25 }; return b[std::max(0, std::min(6, div))]; }

inline float hash01(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (float) (x >> 8) * (1.0f / 16777216.0f);
}

struct HostTime { double bpm = 120.0; bool hasBpm = false; double ppq = 0.0; bool hasPpq = false; bool playing = false; };

struct Lfo
{
    int shape = 0; float rateHz = 0.5f; bool sync = false; int div = 4; uint32_t seed = 1;
    double phase = 0.0; uint32_t cycle = 0; float value = 0.0f;

    // advance by dt seconds and return the bipolar value (-1..1)
    float tick(double dt, const HostTime& host)
    {
        if (sync)
        {
            const double bpc = lfoDivBeats(div);
            if (host.hasPpq && host.playing)
            {
                double c = host.ppq / bpc; double fl = std::floor(c);
                phase = c - fl; cycle = (uint32_t) (int64_t) fl;
            }
            else
            {
                double adv = dt * host.bpm / 60.0 / bpc; phase += adv;
                while (phase >= 1.0) { phase -= 1.0; ++cycle; }
            }
        }
        else
        {
            phase += dt * std::max(0.001f, rateHz);
            while (phase >= 1.0) { phase -= 1.0; ++cycle; }
        }
        value = eval(phase, cycle);
        return value;
    }
    float eval(double ph, uint32_t cyc) const
    {
        const float p = (float) ph;
        switch (shape)
        {
            case 0: return std::sin(6.2831853f * p);
            case 1: return 1.0f - 4.0f * std::fabs(p - 0.5f) ;
            case 2: return 2.0f * p - 1.0f;
            case 3: return p < 0.5f ? 1.0f : -1.0f;
            case 4: { float a = 2.0f * hash01(cyc * 2654435761u + seed) - 1.0f, b = 2.0f * hash01((cyc + 1) * 2654435761u + seed) - 1.0f;
                      float t = 0.5f - 0.5f * std::cos(3.14159265f * p); return a + (b - a) * t; }
            default: return 2.0f * hash01(cyc * 2654435761u + seed) - 1.0f;
        }
    }
};

struct Walk
{
    float x = 0.0f; uint32_t s = 12345;
    float tick(double dt, float speed)
    {
        s = s * 1664525u + 1013904223u; float u1 = (float) (s >> 8) * (1.0f / 16777216.0f);
        s = s * 1664525u + 1013904223u; float u2 = (float) (s >> 8) * (1.0f / 16777216.0f);
        float g = (u1 + u2 - 1.0f) * 2.449f;                      // roughly unit variance
        float d = (float) dt;
        x += -x * 0.08f * speed * d + 0.15f * speed * std::sqrt(d) * g;
        x = std::max(-1.0f, std::min(1.0f, x));
        return x;
    }
};

struct ModSlot { int src = 0, dst = 0; float amt = 0.0f; };

// dst[d] receives the sum of amt*source over all slots routed there (units: fractions of the parameter's range).
inline void evaluateMatrix(const float* srcVal, const ModSlot* slots, float* dst)
{
    for (int d = 0; d < kNumDst; ++d) dst[d] = 0.0f;
    for (int i = 0; i < kModSlots; ++i)
        if (slots[i].src > 0 && slots[i].src < kNumSrc && slots[i].dst > 0 && slots[i].dst < kNumDst)
            dst[slots[i].dst] += slots[i].amt * srcVal[slots[i].src];
}
} // namespace aeola
