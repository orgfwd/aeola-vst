// Aeola engine v3 — polyphonic additive drone voices with a morphing timbre bank. JUCE-free (testable offline).
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later (derived from Aeola, https://github.com/orgfwd/aeola)
//
//  * 24 voice slots: up to `maxVoices` live voices plus extra slots so a stolen voice can fade out (25 ms).
//  * Control-rate update every kBlock (32) samples; all audible quantities ramp linearly across the sub-block
//    and every user parameter is smoothed. Only partials that are actually audible are rendered.
//  * Each voice computes its own spectrum (two timbres morphed, colour tilt, formants, per-partial life).
//  * 32-bit fixed-point phase accumulators + 4096-point sine table; partials above 0.45*fs are never rendered.
//  * No allocation, locks or system calls in render().
#pragma once
#include "Tuning.h"
#include "Timbre.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace aeola
{
constexpr int kSlots = 24;
constexpr int kBlock = 32;
constexpr double kTwoPi = 6.283185307179586;

struct SineTable
{
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable() { for (int i = 0; i <= N; ++i) t[i] = (float) std::sin(kTwoPi * i / N); }
    inline float lookup(uint32_t ph) const
    {
        uint32_t i = ph >> 20;
        float f = (float) (ph & 0xFFFFF) * (1.0f / 1048576.0f);
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};
inline const SineTable& sineTable() { static const SineTable s; return s; }

// Zavalishin TPT state-variable filter
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set(float cutoff, float res01, double sr)
    {
        cutoff = std::min(cutoff, (float) (sr * 0.45));
        float g = std::tan(3.14159265f * cutoff / (float) sr);
        k = 2.0f - 1.85f * std::min(std::max(res01, 0.0f), 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick(float v0, float& lp, float& hp)
    {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; hp = v0 - k * v1 - v2;
    }
    inline float bandpass(float v0)
    {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v1;
    }
    void reset() { ic1 = ic2 = 0; }
};


// 47-tap Kaiser half-band style FIR used for 2x oversampling around the nonlinear filter path.
struct Os2
{
    static constexpr int N = 47, H = 24;
    float h[N];
    Os2()
    {
        auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double beta = 7.0; double sum = 0;
        for (int n = 0; n < N; ++n)
        {
            double m = n - (N - 1) / 2.0, sinc = m == 0 ? 0.5 : std::sin(3.141592653589793 * 0.5 * m) / (3.141592653589793 * m);
            double r = 2.0 * n / (N - 1) - 1.0, w = i0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0(beta);
            h[n] = (float) (sinc * w); sum += h[n];
        }
        for (int n = 0; n < N; ++n) h[n] = (float) (h[n] / sum);        // unity DC gain at fs2
    }
};
inline const Os2& os2() { static const Os2 o; return o; }

struct Up2
{
    float x[Os2::H] = {}; int pos = 0;
    void reset() { for (auto& v : x) v = 0; pos = 0; }
    inline void process(float in, float& u0, float& u1)
    {
        pos = (pos + 1) % Os2::H; x[pos] = in;
        const auto& f = os2(); float a = 0, b = 0;
        for (int k = 0; k < Os2::H; ++k) { float xv = x[(pos - k + Os2::H) % Os2::H]; a += f.h[2 * k] * xv; if (k < Os2::H - 1) b += f.h[2 * k + 1] * xv; }
        u0 = 2.0f * a; u1 = 2.0f * b;
    }
};
struct Down2
{
    float z[64] = {}; int pos = 0;
    void reset() { for (auto& v : z) v = 0; pos = 0; }
    inline float process(float a, float b)
    {
        pos = (pos + 1) & 63; z[pos] = a; pos = (pos + 1) & 63; z[pos] = b;
        const auto& f = os2(); float y = 0;
        for (int j = 0; j < Os2::N; ++j) y += f.h[j] * z[(pos - j) & 63];
        return y;
    }
};
struct DelayN
{
    float b[32] = {}; int pos = 0;
    void reset() { for (auto& v : b) v = 0; pos = 0; }
    inline float process(float in, int d) { b[pos & 31] = in; float o = b[(pos - d) & 31]; ++pos; return o; }
};

// Moog-style transistor ladder (Huovilainen-type tanh per stage). `poles` = 4 (24 dB/oct) or 2 (12 dB/oct).
struct Ladder
{
    float s[4] = {}, t[4] = {};
    void reset() { for (int i = 0; i < 4; ++i) s[i] = t[i] = 0; }
    inline float tick(float in, float g, float k, int poles)
    {
        constexpr float S = 4.0f;                                  // input scaling: stage saturation starts around 0.25 FS
        float x = in * S - k * s[poles - 1];
        float a = std::tanh(x);
        s[0] += g * (a - t[0]);        t[0] = std::tanh(s[0]);
        s[1] += g * (t[0] - t[1]);     t[1] = std::tanh(s[1]);
        if (poles == 4)
        {
            s[2] += g * (t[1] - t[2]); t[2] = std::tanh(s[2]);
            s[3] += g * (t[2] - t[3]); t[3] = std::tanh(s[3]);
        }
        return s[poles - 1] * (1.0f / S);
    }
};
struct PathB
{
    Up2 up; Down2 dn; Ladder lad; Svf svf; DelayN dry;
    void reset() { up.reset(); dn.reset(); lad.reset(); svf.reset(); dry.reset(); }
};

struct Smooth
{
    float cur = 0, target = 0;
    void set(float v) { cur = target = v; }
    inline void step(float k) { cur += (target - cur) * k; }
};

struct EngineParams
{
    int srcA = 0, srcB = 2; float morph = 0.0f, morphMotion = 0.3f, life = 0.4f, air = 0.3f;
    float brightness = 0.5f /* "Colour" */, oddEven = 0.0f, inharm = 0.0f, beatHz = 0.0f;
    float drift = 0.4f, driftRate = 1.0f;
    float attack = 2.0f, release = 5.0f, glide = 0.6f;
    float panSpread = 0.5f, velSens = 0.5f, breathDepth = 0.0f;
    float north = 0.0f; int system = kJust5; bool fixedRoot = false; int fixedRootPc = 0;
    float cutoff = 12000.0f, resonance = 0.1f, lowcut = 40.0f, gain = 0.5f;
    int maxVoices = 12;
    int ftype = 0; float drive = 0.0f /* dB */, ftrack = 0.0f, fmix = 1.0f;   // 0 Clean, 1 Ladder 24, 2 Ladder 12
};

struct Voice
{
    enum State : uint8_t { Off = 0, Held, Releasing, Fading };
    State state = Off;
    uint32_t id = 0;
    bool isSky = false, physDown = false, pending = false, anchor = false;
    int noteInt = 60;
    float pitch = 60, pitchTarget = 60, bend = 0;
    float cents = 0, centsTarget = 0;
    float level = 1, levelTarget = 1, pan = 0, panTarget = 0;
    float brightOff = 0, vibDepth = 0;
    float breathPeriod = 7.3f, breathPhase = 0;
    float env = 0, envPrev = 0;
    float lfoPh = 0, lfoRate = 0.1f, ampPh = 0, ampRate = 0.05f, panPh = 0, panRate = 0.02f, morphPh = 0, morphRate = 0.03f;
    float lifePh[kPartials] = {}, lifeRate[kPartials] = {};
    uint32_t seed = 1, nseed = 7;
    uint32_t phA[kPartials] = {}, phB[kBPartials] = {};
    float ampPrev[kPartials] = {}, ampBPrev[kBPartials] = {};
    float gLPrev = 0, gRPrev = 0, noisePrev = 0;
    Svf nf;
    uint64_t age = 0;
    // v1.3 per-aircraft character (Sky classes). clsTimbre < 0 = the plugin's own Source A/B voice.
    int clsTimbre = -1; float clsBlend = 0, attMul = 1, relMul = 1, chopRate = 0, chopDepth = 0, chopPh = 0;
    int bus = 0;                    // 1.12: 0 = main mix (drone bed, played notes), 1..9 = aircraft class (mixer meters, class outputs)

    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (1.0f / 16777216.0f); }
    bool audible() const { return state != Off; }
    bool live() const { return state == Held || state == Releasing; }
};

struct VoiceStyle { int timbre = -1; float blend = 0, attackMul = 1, releaseMul = 1, chopRate = 0, chopDepth = 0; int bus = 0; };
constexpr int kClassBuses = 9;

class Engine
{
public:
    EngineParams params;

    void prepare(double sampleRate)
    {
        sr = sampleRate; invSr = 1.0 / sr;
        for (auto& v : voices) v = Voice();
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        sCut.set(params.cutoff); sLow.set(params.lowcut); sGain.set(params.gain); sRes.set(params.resonance);
        sBright.set(params.brightness); sOdd.set(params.oddEven); sInh.set(params.inharm); sBeat.set(params.beatHz);
        sDrift.set(params.drift); sBreath.set(params.breathDepth); sPan.set(params.panSpread);
        sMorph.set(params.morph); sMotion.set(params.morphMotion); sLife.set(params.life); sAir.set(params.air);
        sDrive.set(params.drive); sTrack.set(params.ftrack); sFmix.set(params.fmix); sRoot.set(0.0f);
        for (auto& pb : pb2) pb.reset();
        pathMix = 0; fadeG = 1; curType = params.ftype;
        counter = 0; age = 0;
    }

    // ---- note API -------------------------------------------------------------------------
    void noteOn(int note, float velocity01)
    {
        for (auto& v : voices)
            if (v.live() && !v.isSky && v.id == (uint32_t) note) { v.physDown = true; v.pending = false; v.state = Voice::Held; return; }
        Voice* v = allocate();
        if (!v) return;
        start(*v, (uint32_t) note, false, note, 0.35f + 0.65f * ((1.0f - params.velSens) + params.velSens * velocity01), 0.0f);
        v->physDown = true;
    }
    void noteOff(int note, bool sustainDown)
    {
        for (auto& v : voices)
            if (v.state == Voice::Held && !v.isSky && v.id == (uint32_t) note)
            {
                v.physDown = false;
                if (sustainDown || hold) v.pending = true; else v.state = Voice::Releasing;
            }
    }
    void setHold(bool h) { if (h == hold) return; hold = h; if (!h) releasePending(); }
    bool isHold() const { return hold; }
    void sustainChanged(bool down) { if (!down) releasePending(); }
    void allNotesOff(bool skyToo = true)
    {
        for (auto& v : voices) if (v.state == Voice::Held && (skyToo || !v.isSky)) { v.state = Voice::Releasing; v.pending = false; }
    }
    void setBend(float semitones) { for (auto& v : voices) if (!v.isSky) v.bend = semitones; globalBend = semitones; }

    // ---- sky / generative voices -----------------------------------------------------------
    void skyVoice(uint32_t id, int note, float level, float pan, float brightOff, float breathPeriod, float vib, bool isAnchor = false, const VoiceStyle* st = nullptr)
    {
        Voice* found = nullptr;
        for (auto& v : voices) if (v.live() && v.isSky && v.id == id) { found = &v; break; }
        if (!found)
        {
            found = allocate();
            if (!found) return;
            start(*found, id, true, note, level, pan);
            found->breathPeriod = breathPeriod;
        }
        found->state = Voice::Held;
        found->noteInt = note; found->pitchTarget = (float) note;
        found->levelTarget = level; found->panTarget = pan; found->brightOff = brightOff;
        found->vibDepth = vib; found->breathPeriod = breathPeriod; found->anchor = isAnchor;
        if (st) { found->clsTimbre = st->timbre; found->clsBlend = st->blend; found->attMul = st->attackMul; found->relMul = st->releaseMul; found->chopRate = st->chopRate; found->chopDepth = st->chopDepth; found->bus = st->bus; }
        else { found->clsTimbre = -1; found->clsBlend = 0; found->attMul = found->relMul = 1; found->chopDepth = 0; found->bus = 0; }
    }
    void releaseSkyExcept(const uint32_t* keepIds, int nKeep)
    {
        for (auto& v : voices)
            if (v.state == Voice::Held && v.isSky)
            {
                bool keep = false;
                for (int i = 0; i < nKeep; ++i) if (keepIds[i] == v.id) { keep = true; break; }
                if (!keep) v.state = Voice::Releasing;
            }
    }
    int  liveCount() const { int c = 0; for (auto& v : voices) c += v.live(); return c; }
    int  audibleCount() const { int c = 0; for (auto& v : voices) c += v.audible(); return c; }
    const std::array<Voice, kSlots>& getVoices() const { return voices; }
    double targetFreq(int noteInt) const
    {
        for (auto& v : voices) if (v.state == Voice::Held && v.noteInt == noteInt)
            return midiToHz(v.pitchTarget + v.centsTarget / 100.0);
        return 0.0;
    }

    // Static spectrum for the GUI: what a C4-ish note would look like with the current timbre settings.
    int previewSpectrum(double f0, float* ratio, float* amp) const
    {
        SpecIn in; in.a = params.srcA; in.b = params.srcB; in.morph = params.morph; in.colour = params.brightness;
        in.oddEven = params.oddEven; in.inharm = params.inharm; in.f0 = f0; in.sr = sr; in.lifePh = nullptr;
        computeSpectrum(in, ratio, amp); return kPartials;
    }

    void render(float* outL, float* outR, int n)
    {
        int pos = 0;
        while (pos < n) { int len = std::min(kBlock, n - pos); subBlock(outL + pos, outR + pos, len, pos); pos += len; }
    }
    // 1.12 class outputs: classes whose bit is set in routeMask are written (dry, after Output gain) to their own buffers and left out of the
    // main mix. Pointers are for the next render() call and start at its first sample.
    void setClassOutputs(float* const* L, float* const* R, uint32_t routeMask)
    { for (int c = 0; c < kClassBuses; ++c) { clsOutL[c] = L ? L[c] : nullptr; clsOutR[c] = R ? R[c] : nullptr; } route = L ? routeMask : 0u; }
    // peak of each class since the last call (after Output gain), for the mixer meters
    float takeClassPeak(int c) { const float p = clsPeak[c]; clsPeak[c] = 0.0f; return p; }

private:
    Voice* allocate()
    {
        int live = liveCount();
        if (live >= params.maxVoices)
        {
            Voice* victim = nullptr;
            for (auto& v : voices) if (v.state == Voice::Releasing && (!victim || v.env < victim->env)) victim = &v;
            if (!victim) for (auto& v : voices) if (v.state == Voice::Held && !v.anchor && (!victim || v.age < victim->age)) victim = &v;
            if (!victim) return nullptr;
            victim->state = Voice::Fading;
        }
        for (auto& v : voices) if (v.state == Voice::Off) return &v;
        return nullptr;
    }
    void start(Voice& v, uint32_t id, bool sky, int note, float level, float pan)
    {
        uint32_t s = v.seed; v = Voice(); v.seed = s * 747796405u + (uint32_t) (++counter) * 2891336453u + id;
        v.nseed = v.seed ^ 0x9E3779B9u;
        v.state = Voice::Held; v.id = id; v.isSky = sky; v.noteInt = note;
        v.pitch = v.pitchTarget = (float) note; v.bend = sky ? 0.0f : globalBend;
        v.level = v.levelTarget = level; v.pan = v.panTarget = pan;
        v.env = v.envPrev = 0.0f;
        for (int k = 0; k < kPartials; ++k) { v.phA[k] = (uint32_t) (v.rnd() * 4294967040.0f); v.lifePh[k] = v.rnd(); v.lifeRate[k] = 0.04f + 0.30f * v.rnd(); }
        for (int k = 0; k < kBPartials; ++k) v.phB[k] = (uint32_t) (v.rnd() * 4294967040.0f);
        v.lfoPh = v.rnd(); v.lfoRate = 0.05f + 0.25f * v.rnd();
        v.ampPh = v.rnd(); v.ampRate = 0.03f + 0.12f * v.rnd();
        v.panPh = v.rnd(); v.panRate = 0.01f + 0.04f * v.rnd();
        v.morphPh = v.rnd(); v.morphRate = 0.02f + 0.06f * v.rnd();
        static const float primes[8] = { 5.3f, 5.9f, 6.7f, 7.3f, 7.9f, 8.3f, 8.9f, 9.7f };
        v.breathPeriod = primes[(id + counter) % 8]; v.breathPhase = v.rnd();
        v.age = ++age;
        v.cents = v.centsTarget = computeCents(note, std::min(note, rootNote()));
    }
    void releasePending()
    {
        for (auto& v : voices) if (v.state == Voice::Held && !v.isSky && v.pending && !v.physDown) { v.state = Voice::Releasing; v.pending = false; }
    }
    int rootNote() const
    {
        int r = 1000;
        for (auto& v : voices) if (v.state == Voice::Held) r = std::min(r, v.noteInt);
        return r == 1000 ? 60 : r;
    }
    float computeCents(int note, int root) const
    {
        if (params.north <= 0.0f) return 0.0f;
        int base = params.fixedRoot ? params.fixedRootPc : root;
        return (float) (params.north * pureOffsetCents(params.system, note - base));
    }

    void subBlock(float* outL, float* outR, int len, int pos = 0)
    {
        const float kSm = 1.0f - std::exp(-(float) (kBlock / (0.03 * sr)));
        sCut.target = params.cutoff; sLow.target = params.lowcut; sGain.target = params.gain; sRes.target = params.resonance;
        sBright.target = params.brightness; sOdd.target = params.oddEven; sInh.target = params.inharm; sBeat.target = params.beatHz;
        sDrift.target = params.drift; sBreath.target = params.breathDepth; sPan.target = params.panSpread;
        sMorph.target = params.morph; sMotion.target = params.morphMotion; sLife.target = params.life; sAir.target = params.air;
        sDrive.target = params.drive; sTrack.target = params.ftrack; sFmix.target = params.fmix; sRoot.target = (float) (rootNote() - 60);
        for (auto* s : { &sDrive, &sTrack, &sFmix }) s->step(kSm);
        sRoot.step(1.0f - std::exp(-(float) (kBlock / (0.4 * sr))));
        for (auto* s : { &sCut, &sLow, &sGain, &sRes, &sBright, &sOdd, &sInh, &sBeat, &sDrift, &sBreath, &sPan, &sMorph, &sMotion, &sLife, &sAir }) s->step(kSm);

        const int root = rootNote();
        const float dt = (float) (kBlock * invSr);
        const float glideK = 1.0f - std::exp(-dt / std::max(0.005f, params.glide));
        const float attK = 1.0f - std::exp(-dt / (std::max(0.005f, params.attack) / 3.0f));
        const float relK = 1.0f - std::exp(-dt / (std::max(0.005f, params.release) / 9.2f));
        const float fadeK = 1.0f - std::exp(-dt / 0.006f);
        const float lvlK = 1.0f - std::exp(-dt / 0.05f);
        const int nT = numTimbres();
        const Timbre& TA = timbres()[(size_t) std::min(params.srcA, nT - 1)];
        const Timbre& TB = timbres()[(size_t) std::min(params.srcB, nT - 1)];

        float mixL[kBlock] = {}, mixR[kBlock] = {};
        float cL[kClassBuses][kBlock] = {}, cR[kClassBuses][kBlock] = {}; uint32_t clsUsed = 0;
        const float invLen = 1.0f / (float) len;
        const auto& tbl = sineTable();

        for (auto& v : voices)
        {
            if (v.state == Voice::Off) continue;
            switch (v.state)
            {
                case Voice::Held:      v.env += (1.0f - v.env) * (v.attMul == 1.0f ? attK : 1.0f - std::exp(-dt / (std::max(0.005f, params.attack * v.attMul) / 3.0f))); break;
                case Voice::Releasing: v.env -= v.env * (v.relMul == 1.0f ? relK : 1.0f - std::exp(-dt / (std::max(0.005f, params.release * v.relMul) / 9.2f))); break;
                case Voice::Fading:    v.env -= v.env * fadeK; break;
                default: break;
            }
            if (v.state != Voice::Held && v.env < 1.0e-4f && v.envPrev < 1.0e-4f) { v.state = Voice::Off; continue; }

            if (v.state == Voice::Held) v.centsTarget = computeCents(v.noteInt, root);
            v.pitch += (v.pitchTarget - v.pitch) * glideK;
            v.cents += (v.centsTarget - v.cents) * glideK;
            v.level += (v.levelTarget - v.level) * lvlK;
            v.pan   += (v.panTarget - v.pan) * lvlK;

            v.lfoPh += v.lfoRate * params.driftRate * dt; if (v.lfoPh >= 1.0f) v.lfoPh -= 1.0f;
            v.ampPh += v.ampRate * params.driftRate * dt; if (v.ampPh >= 1.0f) v.ampPh -= 1.0f;
            v.panPh += v.panRate * dt;                    if (v.panPh >= 1.0f) v.panPh -= 1.0f;
            v.morphPh += v.morphRate * dt;                if (v.morphPh >= 1.0f) v.morphPh -= 1.0f;
            v.breathPhase += dt / std::max(1.0f, v.breathPeriod); if (v.breathPhase >= 1.0f) v.breathPhase -= 1.0f;
            for (int k = 0; k < kPartials; ++k) { v.lifePh[k] += v.lifeRate[k] * dt; if (v.lifePh[k] >= 1.0f) v.lifePh[k] -= 1.0f; }

            float driftC = sDrift.cur * 8.0f * (0.6f * std::sin(6.2831853f * v.lfoPh) + 0.4f * std::sin(6.2831853f * v.lfoPh * 2.7f));
            driftC += v.vibDepth * 4.0f * std::sin(6.2831853f * v.lfoPh * 9.0f);
            float semis = v.pitch + v.bend + (v.cents + driftC) / 100.0f;
            double f0 = midiToHz((double) semis);
            float breathe = 1.0f - sDrift.cur * 0.35f * (0.5f + 0.5f * std::sin(6.2831853f * v.ampPh));
            float breath = 1.0f - sBreath.cur * (0.5f - 0.5f * std::cos(6.2831853f * v.breathPhase));
            float panPos = std::max(-1.0f, std::min(1.0f, (v.pan + 0.25f * std::sin(6.2831853f * v.panPh)) * sPan.cur * 2.0f));
            float ang = (panPos * 0.5f + 0.5f) * 1.5707963f;
            float chop = 1.0f;
            if (v.chopDepth > 0.001f) { v.chopPh += v.chopRate * dt; if (v.chopPh >= 1.0f) v.chopPh -= std::floor(v.chopPh); chop = 1.0f - v.chopDepth * 0.5f * (1.0f - std::cos(6.2831853f * v.chopPh)); }
            float vol = v.env * v.level * breathe * breath * chop * 0.16f;
            float gL = vol * std::cos(ang), gR = vol * std::sin(ang);

            // ---- this voice's spectrum (two timbres morphed, per-voice morph motion) ----
            const bool cls = v.clsTimbre >= 0;
            SpecIn si; si.a = cls ? std::min(v.clsTimbre, nT - 1) : params.srcA; si.b = cls ? params.srcA : params.srcB;
            si.morph = cls ? std::min(1.0f, std::max(0.0f, (1.0f - v.clsBlend) * (0.9f + sMotion.cur * 0.5f * std::sin(6.2831853f * v.morphPh))))
                           : std::min(1.0f, std::max(0.0f, sMorph.cur + sMotion.cur * 0.5f * std::sin(6.2831853f * v.morphPh)));
            const Timbre& vTA = cls ? timbres()[(size_t) si.a] : TA; const Timbre& vTB = cls ? TA : TB;
            si.colour = std::min(1.0f, std::max(0.0f, sBright.cur + v.brightOff)); si.oddEven = sOdd.cur; si.inharm = sInh.cur;
            si.life = sLife.cur; si.f0 = f0; si.sr = sr; si.lifePh = v.lifePh;
            float ratio[kPartials], amp[kPartials];
            computeSpectrum(si, ratio, amp);
            const float autoBeat = vTA.autoBeat + (vTB.autoBeat - vTA.autoBeat) * si.morph;
            const float beatHz = std::min(8.0f, sBeat.cur + autoBeat);
            const float noiseAmt = (vTA.noise + (vTB.noise - vTA.noise) * si.morph) * sAir.cur * 2.0f;
            const float wB = std::min(1.0f, beatHz / 0.15f) * 0.6f;
            const float wA = 1.0f - 0.3f * (wB / 0.6f);

            float ampNow[kPartials], ampBNow[kBPartials] = {};
            uint32_t inc[kPartials], incB[kBPartials] = {};
            int idxA[kPartials], nA = 0, idxB[kBPartials], nB = 0;
            for (int k = 0; k < kPartials; ++k)
            {
                double fh = f0 * ratio[k];
                ampNow[k] = (k < kBPartials ? wA : 1.0f) * amp[k];
                inc[k] = (uint32_t) (fh * invSr * 4294967296.0);
                if (std::max(ampNow[k], v.ampPrev[k]) > 2.0e-4f) idxA[nA++] = k;
                if (k < kBPartials)
                {
                    ampBNow[k] = wB * amp[k];
                    incB[k] = (uint32_t) ((fh + (double) beatHz * (double) ratio[k]) * invSr * 4294967296.0);
                    if (std::max(ampBNow[k], v.ampBPrev[k]) > 2.0e-4f) idxB[nB++] = k;
                }
            }

            float dAmp[kPartials], dAmpB[kBPartials], aCur[kPartials], aBCur[kBPartials];
            for (int k = 0; k < kPartials; ++k) { dAmp[k] = (ampNow[k] - v.ampPrev[k]) * invLen; aCur[k] = v.ampPrev[k]; }
            for (int k = 0; k < kBPartials; ++k) { dAmpB[k] = (ampBNow[k] - v.ampBPrev[k]) * invLen; aBCur[k] = v.ampBPrev[k]; }
            float gl = v.gLPrev, gr = v.gRPrev, dgl = (gL - v.gLPrev) * invLen, dgr = (gR - v.gRPrev) * invLen;
            float nz = v.noisePrev, dnz = (noiseAmt * 0.35f - v.noisePrev) * invLen;
            if (noiseAmt > 1e-4f || v.noisePrev > 1e-4f) v.nf.set(std::min(9000.0f, std::max(200.0f, (float) f0 * 3.0f * std::pow(2.0f, (si.colour - 0.5f) * 3.0f))), 0.55f, sr);
            const bool useNoise = noiseAmt > 1e-4f || v.noisePrev > 1e-4f;
            const int vb = (v.bus >= 1 && v.bus <= kClassBuses) ? v.bus - 1 : -1;
            const bool toMain = !(vb >= 0 && ((route >> vb) & 1u) && clsOutL[vb] && clsOutR[vb]);     // routed classes leave the main mix
            float* mtL = vb >= 0 ? cL[vb] : nullptr; float* mtR = vb >= 0 ? cR[vb] : nullptr; if (vb >= 0) clsUsed |= 1u << vb;
            for (int i = 0; i < len; ++i)
            {
                float s = 0.0f;
                for (int j = 0; j < nA; ++j) { int k = idxA[j]; v.phA[k] += inc[k]; aCur[k] += dAmp[k]; s += aCur[k] * tbl.lookup(v.phA[k]); }
                for (int j = 0; j < nB; ++j) { int k = idxB[j]; v.phB[k] += incB[k]; aBCur[k] += dAmpB[k]; s += aBCur[k] * tbl.lookup(v.phB[k]); }
                // phases of skipped partials must still advance so they re-enter without a jump
                if (useNoise)
                {
                    v.nseed = v.nseed * 1664525u + 1013904223u;
                    float w = (float) (int32_t) v.nseed * (1.0f / 2147483648.0f);
                    nz += dnz; s += nz * v.nf.bandpass(w) * 2.0f;
                }
                gl += dgl; gr += dgr;
                const float l = s * gl, r = s * gr;
                if (toMain) { mixL[i] += l; mixR[i] += r; }      // same summing order as before 1.12: the main mix is bit-identical
                if (mtL) { mtL[i] += l; mtR[i] += r; }
            }
            // keep skipped partials' phases moving
            for (int k = 0; k < kPartials; ++k) { bool used = false; for (int j = 0; j < nA; ++j) if (idxA[j] == k) { used = true; break; } if (!used) v.phA[k] += inc[k] * (uint32_t) len; }
            for (int k = 0; k < kBPartials; ++k) { bool used = false; for (int j = 0; j < nB; ++j) if (idxB[j] == k) { used = true; break; } if (!used) v.phB[k] += incB[k] * (uint32_t) len; }
            std::memcpy(v.ampPrev, ampNow, sizeof ampNow); std::memcpy(v.ampBPrev, ampBNow, sizeof ampBNow);
            v.gLPrev = gL; v.gRPrev = gR; v.envPrev = v.env; v.noisePrev = noiseAmt * 0.35f;
        }

        {   // classes: meter, and the routed ones out on their own output
            const float gOut = sGain.cur * 2.0f;
            for (int c = 0; c < kClassBuses; ++c)
            {
                const bool routed = (route >> c) & 1u && clsOutL[c] && clsOutR[c];
                if (!(clsUsed >> c & 1u)) { if (routed) for (int i = 0; i < len; ++i) { clsOutL[c][pos + i] = 0.0f; clsOutR[c][pos + i] = 0.0f; } continue; }
                float pk = clsPeak[c];
                for (int i = 0; i < len; ++i) pk = std::max(pk, std::max(std::fabs(cL[c][i]), std::fabs(cR[c][i])) * gOut);
                clsPeak[c] = pk;
                if (routed) for (int i = 0; i < len; ++i) { clsOutL[c][pos + i] = cL[c][i] * gOut; clsOutR[c][pos + i] = cR[c][i] * gOut; }
            }
        }
        const float trackMul = std::exp2(sTrack.cur * sRoot.cur / 12.0f);
        float cut = std::max(60.0f, std::min(sCut.cur * trackMul, (float) (sr * 0.45)));
        float low = std::max(10.0f, std::min(sLow.cur, 1000.0f));
        lpL.set(cut, sRes.cur, sr); lpR.set(cut, sRes.cur, sr);
        hpL.set(low, 0.0f, sr);     hpR.set(low, 0.0f, sr);
        const float g = sGain.cur * 2.0f;
        const float fm = std::min(1.0f, std::max(0.0f, sFmix.cur));

        // ---- which filter path is wanted: A = the original clean SVF, B = oversampled drive + ladder/SVF ----
        const bool wantB = params.ftype != 0 || params.drive > 0.05f;
        pathMix += ((wantB ? 1.0f : 0.0f) - pathMix) * (1.0f - std::exp(-(float) (kBlock / (0.012 * sr))));
        if (!wantB && pathMix < 1.0e-4f) { if (pathMix != 0.0f) for (auto& q : pb2) q.reset(); pathMix = 0.0f; }
        if (wantB && pathMix > 0.9999f) pathMix = 1.0f;
        // filter model switch: dip the wet signal, swap models, come back (no click, no stale state)
        const float fadeStart = fadeG;
        if (curType != params.ftype) { fadeG -= 0.5f; if (fadeG <= 0.0f) { fadeG = 0.0f; curType = params.ftype; for (auto& q : pb2) { q.lad.reset(); q.svf.reset(); } } }
        else if (fadeG < 1.0f) fadeG = std::min(1.0f, fadeG + 0.5f);
        const float fadeEnd = fadeG;

        float aL[kBlock], aR[kBlock];
        for (int i = 0; i < len; ++i)
        {
            float lp, hp, dummy;
            hpL.tick(mixL[i], dummy, hp); lpL.tick(hp, lp, dummy);
            aL[i] = pathMix >= 1.0f ? 0.0f : ((hp * (1.0f - fm) + lp * fm) * g);
            hpL_out[i] = hp;
            hpR.tick(mixR[i], dummy, hp); lpR.tick(hp, lp, dummy);
            aR[i] = pathMix >= 1.0f ? 0.0f : ((hp * (1.0f - fm) + lp * fm) * g);
            hpR_out[i] = hp;
        }
        if (pathMix <= 0.0f)
        {
            for (int i = 0; i < len; ++i) { outL[i] = aL[i]; outR[i] = aR[i]; }
        }
        else
        {
            const double sr2 = sr * 2.0;
            const float cut2 = std::max(60.0f, std::min(sCut.cur * trackMul, (float) (sr * 0.45)));
            const float gl = 1.0f - std::exp(-(float) (6.283185307179586 * cut2 / sr2));
            const float res = std::min(1.0f, std::max(0.0f, sRes.cur));
            const int poles = curType == 2 ? 2 : 4;
            const float kfb = curType == 2 ? 18.0f * res : 4.5f * res;
            const float dB = sDrive.cur, gdrv = std::pow(10.0f, dB / 20.0f), wdrv = std::min(1.0f, dB / 3.0f);
            const float mk = 1.0f / (8.0f * std::pow(gdrv, 0.25f));
            for (auto& q : pb2) q.svf.set(cut2, res, sr2);
            const float fstep = (fadeEnd - fadeStart) * invLen;
            float fcur = fadeStart; const float pm = pathMix;
            for (int i = 0; i < len; ++i)
            {
                fcur += fstep;
                for (int c = 0; c < 2; ++c)
                {
                    PathB& q = pb2[c]; float x = c ? hpR_out[i] : hpL_out[i];
                    float dryD = q.dry.process(x, 23);
                    float u[2]; q.up.process(x, u[0], u[1]);
                    float f[2];
                    for (int k = 0; k < 2; ++k)
                    {
                        float v = u[k];
                        if (wdrv > 0.0f) { float sh = std::tanh(gdrv * 8.0f * v) * mk; v = v + wdrv * (sh - v); }
                        if (curType == 0) { float lp, hp; q.svf.tick(v, lp, hp); f[k] = lp; }
                        else f[k] = q.lad.tick(v, gl, kfb, poles);
                    }
                    float y = q.dn.process(f[0], f[1]);
                    float b = (dryD * (1.0f - fm) + y * fcur * fm) * g;
                    float a = c ? aR[i] : aL[i];
                    float o = pm >= 1.0f ? b : (a * (1.0f - pm) + b * pm);
                    (c ? outR : outL)[i] = o;
                }
            }
        }
        for (auto* f : { &hpL, &hpR, &lpL, &lpR }) { if (std::fabs(f->ic1) < 1e-20f) f->ic1 = 0; if (std::fabs(f->ic2) < 1e-20f) f->ic2 = 0; }
    }

    double sr = 44100, invSr = 1.0 / 44100;
    std::array<Voice, kSlots> voices;
    uint32_t counter = 0; uint64_t age = 0;
    bool hold = false; float globalBend = 0;
    float* clsOutL[kClassBuses] = {}; float* clsOutR[kClassBuses] = {}; uint32_t route = 0; float clsPeak[kClassBuses] = {};
    Svf hpL, hpR, lpL, lpR;
    float hpL_out[kBlock], hpR_out[kBlock];
    PathB pb2[2]; float pathMix = 0, fadeG = 1; int curType = 0;
    Smooth sDrive, sTrack, sFmix, sRoot;
    Smooth sCut, sLow, sGain, sRes, sBright, sOdd, sInh, sBeat, sDrift, sBreath, sPan, sMorph, sMotion, sLife, sAir;
};
} // namespace aeola
