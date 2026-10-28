// Aeola — plugin processor. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using namespace aeola;

static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

AeolaDroneProcessor::AeolaDroneProcessor()
    : AudioProcessor([] { auto b = BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)
                                                    .withOutput("Reverb only", juce::AudioChannelSet::stereo(), false);
                          for (int c = 0; c < kNumClasses; ++c) b = b.withOutput(className(c), juce::AudioChannelSet::stereo(), false);   // 1.12 class outputs
                          return b; }()),
      apvts(*this, nullptr, "STATE", makeLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) raw[r->paramID] = apvts.getRawParameterValue(r->paramID);
    cfg.seed = 1;
    for (auto& a : modDst) a.store(0.0f); for (auto& a : modSrc) a.store(0.0f); for (auto& a : clsPeakA) a.store(0.0f);
    place = makePlace(); apKeyLast = place.dataKey(); placeSince.store((double) juce::Time::currentTimeMillis() / 1000.0);
    feed.setPlace(place);
    feed.loadCache(place);
    feed.onFrame = [this](const Frame& f, uint64_t idx) {
        const juce::ScopedLock l(capLock);
        if (idx != lastCapKey) { captured.clear(); lastCapKey = idx; }
        captured.push_back(f);
        while (captured.size() > 15) captured.erase(captured.begin());
    };
    feed.start();
    startTimer(500);
}

namespace
{
// MIDI sources and followers inside one host process. An Aeola with MIDI out on and class "All" publishes the aircraft notes it plays;
// Aeolas set to a single class copy their class from it, so every track records the same sky and none of them fetches data.
struct MidiBus
{
    struct R { uint32_t id; uint8_t cls, note, vel; };
    juce::SpinLock lock; const void* owner = nullptr; double beat = 0; int n = 0; R r[aeola::kMaxOrders];
};
MidiBus& midiBus() { static MidiBus b; return b; }
double busNow() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
}

AeolaDroneProcessor::~AeolaDroneProcessor()
{
    stopTimer();
    auto& b = midiBus(); juce::SpinLock::ScopedLockType g(b.lock);
    if (b.owner == this) { b.owner = nullptr; b.n = 0; }
}

bool AeolaDroneProcessor::updateMidiRole()
{
    auto& b = midiBus(); const double now = busNow();
    const bool on = P(ID::midiout) > 0.5f, one = P(ID::midiclass) > 0.5f;
    juce::SpinLock::ScopedLockType g(b.lock);
    if (on && !one) { if (!b.owner || b.owner == this || now - b.beat > 2.0) { b.owner = this; b.beat = now; } }
    else if (b.owner == this) { b.owner = nullptr; b.n = 0; }
    return on && one && b.owner && b.owner != this && now - b.beat <= 2.0;
}

bool AeolaDroneProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int b = 1; b < l.outputBuses.size(); ++b) { auto s = l.getChannelSet(false, b); if (!(s.isDisabled() || s == juce::AudioChannelSet::stereo())) return false; }
    return true;
}

void AeolaDroneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; blockMax = std::max(64, std::min(samplesPerBlock, 2048));
    engine.prepare(sr);
    sL.assign((size_t) blockMax, 0.0f); sR.assign((size_t) blockMax, 0.0f);
    wetL.assign((size_t) blockMax, 0.0f); wetR.assign((size_t) blockMax, 0.0f);
    for (int c = 0; c < kNumClasses; ++c) { clsBufL[c].assign((size_t) blockMax, 0.0f); clsBufR[c].assign((size_t) blockMax, 0.0f); }
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) blockMax, 2 };
    os = std::make_unique<juce::dsp::Oversampling<float>>(2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
    os->initProcessing((size_t) blockMax); os->reset();
    setLatencySamples((int) std::ceil(os->getLatencyInSamples()));
    reverb.prepare(spec); reverb.reset();
    preDelay.prepare(spec); preDelay.reset();
    limiter.prepare(spec); limiter.setThreshold(-1.0f); limiter.setRelease(120.0f); limiter.reset();
    smWarm.reset(sr, 0.05); smMix.reset(sr, 0.05); smWidth.reset(sr, 0.05);
    smWarm.setCurrentAndTargetValue(P(ID::warmth)); smMix.setCurrentAndTargetValue(P(ID::rvmix)); smWidth.setCurrentAndTargetValue(P(ID::width));
    wallEpoch = (double) juce::Time::currentTimeMillis() / 1000.0;
    sampleClock = 0; skyAccum = 0; nPrev = 0; replayStartClock = 0;
    mapper.reset();
    applyParams();
    engine.prepare(sr);
}

void AeolaDroneProcessor::applyParams()
{
    auto& e = engine.params;
    const float tens = P(ID::tension) - 0.5f, light = P(ID::light) - 0.5f, motion = P(ID::motion) - 0.5f, space = P(ID::space) - 0.5f;
    e.brightness = clamp01(P(ID::brightness) + light * 0.9f + modWheel * P(ID::modamt) * 0.5f);
    e.srcA = (int) P(ID::srca); e.srcB = (int) P(ID::srcb);
    e.morph = P(ID::morph); e.morphMotion = P(ID::morphmod); e.life = P(ID::life); e.air = P(ID::air);
    e.oddEven = P(ID::oddeven);
    e.inharm = clamp01(P(ID::inharm) + std::max(0.0f, tens) * 2.0f * 0.25f);
    e.beatHz = std::min(6.0f, P(ID::beat) + std::max(0.0f, tens) * 2.0f * 3.0f);
    e.drift = clamp01(P(ID::drift) + motion * 0.8f);
    e.driftRate = P(ID::driftrate) * std::pow(2.0f, motion * 2.0f);
    e.attack = P(ID::attack); e.release = P(ID::release); e.glide = P(ID::glide);
    e.panSpread = P(ID::panspread); e.velSens = P(ID::velsens);
    e.breathDepth = clamp01(P(ID::breath) + motion * 0.6f);
    e.north = P(ID::north); e.system = (int) P(ID::system);
    e.fixedRoot = P(ID::rootmode) > 0.5f; e.fixedRootPc = (int) P(ID::fixedroot);
    e.cutoff = std::max(100.0f, std::min(18000.0f, P(ID::cutoff) * std::pow(2.0f, light * 3.0f)));
    e.resonance = P(ID::reso); e.lowcut = P(ID::lowcut); e.gain = P(ID::gain);
    e.maxVoices = std::min(kSlots - 4, (int) P(ID::voices) + (P(ID::anchor) > 0.5f ? (P(ID::anchorfifth) > 0.5f ? 2 : 1) : 0));       // aircraft voices + the anchor(s)
    e.ftype = (int) P(ID::ftype); e.drive = P(ID::drive); e.ftrack = P(ID::ftrack); e.fmix = P(ID::fmix);

    place = makePlace();
    cfg.rootPc = (int) P(ID::key); cfg.scale = (int) P(ID::scale);
    cfg.baseNote = 12 * ((int) P(ID::baseoct) + 1); cfg.rangeOct = P(ID::range);
    cfg.consonance = clamp01(P(ID::consonance) - tens * 1.2f);
    cfg.maxAircraft = (int) P(ID::maxac); cfg.voiceCap = (int) P(ID::voices);
    cfg.anchor = P(ID::anchor) > 0.5f; cfg.anchorFifth = P(ID::anchorfifth) > 0.5f;
    cfg.ghostMin = (int) P(ID::ghostmin); cfg.ghostLevel = P(ID::ghostlevel);
    cfg.evolveDepth = clamp01(P(ID::evolve) + motion * 0.8f); cfg.evolvePeriodMin = P(ID::evolveperiod);
    cfg.loopStretch = P(ID::stretch); cfg.seed = (uint32_t) P(ID::simseed);
    cfg.apLat = place.lat; cfg.apLon = place.lon; cfg.radiusNm = place.radiusNm;
    cfg.classes = true; cfg.contrast = P(ID::contrast); cfg.swell = P(ID::swell) > 0.5f;
    for (int c = 0; c < kNumClasses; ++c)
    {
        auto& st = cfg.cls[c];
        st.timbre = (int) P(ID::classId(c, 0)) - 1; st.levelDb = P(ID::classId(c, 1)); st.octave = (int) P(ID::classId(c, 2));
        st.attackMul = P(ID::classId(c, 3)); st.chop = P(ID::classId(c, 4));
    }
    cfg.soloMask = cfg.muteMask = 0;
    for (int c = 0; c < kNumClasses; ++c) { if (P(ID::classId(c, 5)) > 0.5f) cfg.soloMask |= 1u << c; if (P(ID::classId(c, 6)) > 0.5f) cfg.muteMask |= 1u << c; }
    if (e.srcA != lastSpecA || e.srcB != lastSpecB || std::fabs(e.morph - lastSpecMorph) > 1e-3f || std::fabs(e.inharm - lastSpecInh) > 1e-3f || std::fabs(e.oddEven - lastSpecOdd) > 1e-3f || std::fabs(e.brightness - lastSpecBright) > 0.02f)
    {   // timbre awareness for the Guard (only matters when the source is inharmonic); recomputed only when the sound changes
        lastSpecA = e.srcA; lastSpecB = e.srcB; lastSpecMorph = e.morph; lastSpecInh = e.inharm; lastSpecOdd = e.oddEven; lastSpecBright = e.brightness;
        const int nt = numTimbres(); const float m = e.morph;
        float h = timbreHarmonicity(timbres()[(size_t) std::min(e.srcA, nt - 1)]) * (1.0f - m) + timbreHarmonicity(timbres()[(size_t) std::min(e.srcB, nt - 1)]) * m;
        cfg.harmonicity = h * (1.0f - std::min(1.0f, e.inharm * 3.0f));
        if (cfg.harmonicity < 0.6f) cfg.nPart = engine.previewSpectrum(196.0, cfg.partRatio, cfg.partAmp); else cfg.nPart = 0;
    }
    simTransport = P(ID::simsync) > 0.5f;

    baseWarm = P(ID::warmth); smWarm.setTargetValue(baseWarm);
    baseMix = clamp01(P(ID::rvmix) + space * 0.6f); baseWidth = P(ID::width);
    smMix.setTargetValue(baseMix);
    smWidth.setTargetValue(baseWidth);
    {   baseRvSize = clamp01(P(ID::rvsize) + space * 0.4f); baseRvDamp = P(ID::rvdamp);
        revModActive = false;
        for (int i = 0; i < kModSlots; ++i) { const int d = (int) P(ID::modSlotId(i, 1)); if ((d == DstRevSize || d == DstRevDamp) && P(ID::modSlotId(i, 0)) > 0.5f) revModActive = true; }
        const float rs = baseRvSize * 0.98f, rd = baseRvDamp;
        if (!revModActive && (rs != lastRvSize || rd != lastRvDamp)) { lastRvSize = rs; lastRvDamp = rd; reverb.setParameters({ rs, rd, 1.0f, 0.0f, 1.0f, 0.0f }); } }
    preDelay.setDelay(std::min(P(ID::rvpre) * 0.001f * (float) sr, 48000.0f * 2 - 1));
    engine.setHold(P(ID::hold) > 0.5f);
    baseP = e;
    anyMod = false;
    for (int i = 0; i < kModSlots; ++i)
    {
        slots[i].src = (int) P(ID::modSlotId(i, 0)); slots[i].dst = (int) P(ID::modSlotId(i, 1)); slots[i].amt = P(ID::modSlotId(i, 2));
        if (slots[i].src > 0 && slots[i].dst > 0 && std::fabs(slots[i].amt) > 1e-4f) anyMod = true;
    }
    baseLfoRate[0] = P(ID::l1rate); baseLfoRate[1] = P(ID::l2rate);
    lfo[0].shape = (int) P(ID::l1shape); lfo[0].rateHz = P(ID::l1rate); lfo[0].sync = P(ID::l1sync) > 0.5f; lfo[0].div = (int) P(ID::l1div); lfo[0].seed = 101;
    lfo[1].shape = (int) P(ID::l2shape); lfo[1].rateHz = P(ID::l2rate); lfo[1].sync = P(ID::l2sync) > 0.5f; lfo[1].div = (int) P(ID::l2div); lfo[1].seed = 202;
}

// Control-rate modulation: runs before every engine render chunk (<= 256 samples).
void AeolaDroneProcessor::updateMod(int len)
{
    const double dt = (double) len / sr;
    float src[kNumSrc] = {};
    src[SrcLfo1] = lfo[0].tick(dt, hostTime); src[SrcLfo2] = lfo[1].tick(dt, hostTime);
    src[SrcWalk] = walk.tick(dt, 1.0f);
    src[SrcAircraft] = skyCount; src[SrcAltitude] = skyAlt; src[SrcProximity] = skyProx; src[SrcSpeed] = skySpeed;
    src[SrcModWheel] = modWheel; src[SrcVelocity] = velSrc; src[SrcAftertouch] = atSrc; src[SrcPitchBend] = bendNorm;
    src[SrcTension] = (P(ID::tension) - 0.5f) * 2.0f; src[SrcLight] = (P(ID::light) - 0.5f) * 2.0f;
    src[SrcMotion] = (P(ID::motion) - 0.5f) * 2.0f; src[SrcSpace] = (P(ID::space) - 0.5f) * 2.0f;
    for (int c = 0; c < kNumClasses; ++c) { src[SrcClassCount + c] = clsCount[c]; src[SrcClassAlt + c] = clsAlt[c]; }
    for (int i = 0; i < kNumSrc; ++i) modSrc[i].store(src[i], std::memory_order_relaxed);
    if (!anyMod) { for (int d = 0; d < kNumDst; ++d) modDst[d].store(0.0f, std::memory_order_relaxed); for (int c = 0; c < kNumClasses; ++c) clsLevelMod[c] = clsOctMod[c] = clsSwellMod[c] = clsChopMod[c] = 0.0f; consMod = evolveMod = rangeMod = ghostMod = stretchMod = 0.0f; return; }

    float d[kNumDst]; evaluateMatrix(src, slots, d);
    for (int i = 0; i < kNumDst; ++i) modDst[i].store(d[i], std::memory_order_relaxed);
    auto& e = engine.params; const auto& b = baseP;
    e.brightness = clamp01(b.brightness + d[DstColour]);
    e.morph = clamp01(b.morph + d[DstMorph]);
    e.morphMotion = clamp01(b.morphMotion + d[DstMorphMotion]);
    e.life = clamp01(b.life + d[DstLife]);
    e.air = clamp01(b.air + d[DstAir]);
    e.beatHz = std::max(0.0f, std::min(8.0f, b.beatHz + d[DstBeat] * 6.0f));
    e.drift = clamp01(b.drift + d[DstDrift]);
    e.cutoff = std::max(60.0f, std::min(18000.0f, b.cutoff * std::pow(2.0f, 4.0f * d[DstCutoff])));
    e.resonance = clamp01(b.resonance + d[DstReso]);
    e.drive = std::max(0.0f, std::min(24.0f, b.drive + d[DstDrive] * 24.0f));
    e.panSpread = clamp01(b.panSpread + d[DstPanSpread]);
    e.gain = clamp01(b.gain * std::pow(10.0f, 6.0f * d[DstOutput] / 20.0f));
    smWidth.setTargetValue(std::max(0.0f, std::min(1.5f, baseWidth + d[DstWidth] * 1.5f)));
    smMix.setTargetValue(clamp01(baseMix + d[DstReverbMix]));
    for (int c = 0; c < kNumClasses; ++c) { clsLevelMod[c] = d[DstClassLevel + c] * 18.0f; clsOctMod[c] = d[DstClassOctave + c] * 4.0f; }
    e.inharm = clamp01(b.inharm + d[DstInharm]);
    e.oddEven = std::max(-1.0f, std::min(1.0f, b.oddEven + d[DstOddEven] * 2.0f));
    e.breathDepth = clamp01(b.breathDepth + d[DstBreath]);
    e.north = clamp01(b.north + d[DstNorth]);
    consMod = d[DstConsonance]; evolveMod = d[DstEvolve];
    // 1.13 targets
    auto lim = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    e.attack = lim(b.attack + d[DstAttack] * 19.99f, 0.01f, 20.0f);
    e.release = lim(b.release + d[DstRelease] * 29.99f, 0.01f, 30.0f);
    e.glide = lim(b.glide + d[DstGlide] * 29.99f, 0.01f, 30.0f);
    e.driftRate = lim(b.driftRate + d[DstDriftRate] * 3.9f, 0.05f, 16.0f);
    e.lowcut = lim(b.lowcut + d[DstLowCut] * 390.0f, 10.0f, 400.0f);
    e.fmix = clamp01(b.fmix + d[DstFilterMix]); e.ftrack = clamp01(b.ftrack + d[DstKeyTrack]);
    smWarm.setTargetValue(clamp01(baseWarm + d[DstWarmth]));
    if (revModActive)
    {   const float rs = clamp01(baseRvSize + d[DstRevSize]) * 0.98f, rd = clamp01(baseRvDamp + d[DstRevDamp]);
        if (std::fabs(rs - lastRvSize) > 0.004f || std::fabs(rd - lastRvDamp) > 0.004f) { lastRvSize = rs; lastRvDamp = rd; reverb.setParameters({ rs, rd, 1.0f, 0.0f, 1.0f, 0.0f }); } }
    lfo[0].rateHz = lim(baseLfoRate[0] + d[DstLfo1Rate] * 19.99f, 0.01f, 20.0f); lfo[1].rateHz = lim(baseLfoRate[1] + d[DstLfo2Rate] * 19.99f, 0.01f, 20.0f);
    rangeMod = d[DstRange] * 3.5f; ghostMod = d[DstGhostLevel]; stretchMod = d[DstStretch] * 1.5f;
    for (int c = 0; c < kNumClasses; ++c) { clsSwellMod[c] = d[DstClassSwell + c] * 3.75f; clsChopMod[c] = d[DstClassChop + c]; }
}

// ---- time/clock-driven rendering -----------------------------------------------------------
void AeolaDroneProcessor::renderSegment(int from, int to)
{
    // `from`/`to` are chunk-relative indices into sL/sR
    while (from < to)
    {
        int len = std::min(to - from, 256);
        updateMod(len);
        {   float* cl[kNumClasses]; float* cr[kNumClasses];
            for (int c = 0; c < kNumClasses; ++c) { cl[c] = clsBufL[c].data() + from; cr[c] = clsBufR[c].data() + from; }
            engine.setClassOutputs(cl, cr, clsRoute); }
        engine.render(sL.data() + from, sR.data() + from, len);
        double dt = (double) len / sr;
        sampleClock += dt; wallEpoch += dt; skyAccum += dt;
        from += len;
        if (skyAccum >= 0.1) { skyAccum = 0; skyTick(); }
    }
}

bool AeolaDroneProcessor::pickFrame(Frame& out, int& srcUsed, double& ageSec)
{
    const Place& ap = place; const uint64_t apKey = ap.dataKey(), centreKey = ap.centreKey();
    const int mode = (int) P(ID::source);                // 0 auto 1 live 2 cached 3 replay 4 simulated
    requestedSource = mode;
    const double simNow = simTransport ? simBaseEpoch + hostSeconds : wallEpoch;
    const double maxLiveAge = 300.0;                     // longer than that without a fresh snapshot, live data is dropped
    const double realNow = (double) juce::Time::currentTimeMillis() / 1000.0;   // data age is real time, also during an offline bounce
    ageSec = 0;

    auto tryLive = [&]() {
        if (!feed.getLive(tmpFrame, centreKey)) return false;
        double age = realNow - tmpFrame.epoch; if (age < 0) age = 0;
        if (age > maxLiveAge) return false;
        out = tmpFrame; extrapolate(out, age, ap); out.epoch = wallEpoch; ageSec = age; srcUsed = 1; return true;
    };
    auto tryCache = [&]() {
        if (!feed.getCached(tmpFrame, centreKey)) return false;
        double age = realNow - tmpFrame.epoch; if (age < 0) age = 0;
        if (age > 6.0 * 3600.0) return false;
        // an old snapshot is not flown on for minutes: that would be invented positions
        out = tmpFrame; extrapolate(out, std::min(age, 90.0), ap); out.epoch = wallEpoch; ageSec = age; srcUsed = 2; return true;
    };
    auto tryStale = [&]() {      // the last real snapshot of this place, kept after live data stopped arriving (limit reached, offline): labelled "Saved", not flown on
        if (!feed.getLive(tmpFrame, centreKey)) return false;
        double age = realNow - tmpFrame.epoch; if (age < 0) age = 0;
        if (age > 6.0 * 3600.0) return false;
        out = tmpFrame; extrapolate(out, std::min(age, 90.0), ap); out.epoch = wallEpoch; ageSec = age; srcUsed = 2; return true;
    };
    auto tryReplay = [&]() {
        int idx = replayIdx.load(std::memory_order_acquire); int c = replayCount[idx];
        if (c <= 0 || replayKey != apKey) return false;
        const Frame* fr = replayBuf[idx];
        double span = std::max(60.0, fr[c - 1].epoch - fr[0].epoch + (double) P(ID::poll));
        double t = fr[0].epoch + std::fmod(simTransport ? hostSeconds : sampleClock - replayStartClock, span);
        int pick = 0; for (int i = 0; i < c; ++i) if (fr[i].epoch <= t) pick = i;
        out = fr[pick]; extrapolate(out, t - fr[pick].epoch, ap); out.epoch = t; srcUsed = 3; return true;
    };
    auto sim = [&]() { simulatePlace(ap, cfg.seed, simNow, P(ID::simlevel), out, cfg.classes); srcUsed = 4; return true; };

    switch (mode)
    {
        case 1: if (tryLive()) return true; out.count = 0; out.nGround = 0; srcUsed = 0; return false;      // no data: say so, never "live, 0 s ago"
        case 2: if (tryCache() || tryStale()) return true; out.count = 0; out.nGround = 0; srcUsed = 0; return false;
        case 3: if (tryReplay()) return true; return sim();
        case 4: return sim();
        default:
            // Real data only: live, else the last real snapshot (labelled), else an empty sky with the drone bed and a message saying why.
            // Simulated traffic is never switched on automatically; it is a choice in Sky source.
            if (tryLive() || tryStale() || tryCache()) return true;
            out.count = 0; out.nGround = 0; srcUsed = 0; return false;
    }
}

void AeolaDroneProcessor::skyTick()
{
    const bool skyOn = P(ID::skyon) > 0.5f;
    const bool follow = updateMidiRole(); followingBus.store(follow);
    int src = 0; double age = 0;
    if (skyOn && !follow) pickFrame(curFrame, src, age); else { curFrame.count = 0; }
    {   // 1.12: only aircraft inside the radius play; the rest are kept for the map (approaching, leaving)
        int w = 0; nOutAc = 0;
        for (int i = 0; i < curFrame.count; ++i)
        {
            if (curFrame.ac[i].distNm <= cfg.radiusNm) curFrame.ac[w++] = curFrame.ac[i];
            else if (nOutAc < kMaxAc) outAc[nOutAc++] = curFrame.ac[i];
        }
        curFrame.count = w;
    }
    lastSource = src;
    // Solo audition: a soloed class with no aircraft in the sky plays one virtual aircraft, so a voice can always be heard and tuned
    auditioning = false;
    if (skyOn && cfg.soloMask)
        for (int c = 0; c < kNumClasses; ++c)
        {
            if (!(cfg.soloMask & (1u << c)) || (cfg.muteMask & (1u << c)) || curFrame.count >= kMaxAc) continue;
            bool present = false; for (int i = 0; i < curFrame.count; ++i) if (curFrame.ac[i].category == c) present = true;
            if (present) continue;
            Aircraft& a = curFrame.ac[curFrame.count++]; a = Aircraft();
            a.id = 0x3000000u | (uint32_t) c; a.category = a.legacyCat = c; a.altFt = (c == kHeli ? 1800.0f : (c == kLight ? 4500.0f : 14000.0f)); a.gsKt = (c == kHeli ? 90.0f : (c == kLight || c == kProp ? 150.0f : 400.0f));
            double d = std::max(3.0, cfg.radiusNm * 0.12); a.distNm = (float) d; a.trackDeg = 0.0f;
            a.lat = cfg.apLat; a.lon = cfg.apLon + d / (60.0 * std::max(0.05, std::cos(cfg.apLat * M_PI / 180.0)));
            std::snprintf(a.callsign, sizeof a.callsign, "AUDITION"); std::snprintf(a.type, sizeof a.type, "TEST");
            auditioning = true;
        }
    {   // sky -> modulation sources (smoothed over ~5 s so new data never jumps)
        float tc = 1.0f - std::exp(-0.1f / 5.0f);
        float n = (float) curFrame.count, mx = (float) std::max(1, (int) P(ID::maxac));
        float alt = 0, spd = 0, near = 1e9f;
        for (int i = 0; i < curFrame.count; ++i) { alt += curFrame.ac[i].altFt; spd += curFrame.ac[i].gsKt; near = std::min(near, curFrame.ac[i].distNm); }
        double rad = place.radiusNm;
        float tCount = std::min(1.0f, n / mx), tAlt = n > 0 ? std::min(1.0f, alt / n / 40000.0f) : 0.0f;
        float tProx = n > 0 ? std::max(0.0f, 1.0f - near / (float) rad) : 0.0f, tSpd = n > 0 ? std::min(1.0f, spd / n / 500.0f) : 0.0f;
        skyCount += (tCount - skyCount) * tc; skyAlt += (tAlt - skyAlt) * tc; skyProx += (tProx - skyProx) * tc; skySpeed += (tSpd - skySpeed) * tc;
        for (int c = 0; c < kNumClasses; ++c)
        {   // per class: count (four or more = 1) and average altitude; audition aircraft do not count
            int k = 0; float a = 0;
            for (int i = 0; i < curFrame.count; ++i) if (curFrame.ac[i].category == c && (curFrame.ac[i].id & 0xFF000000u) != 0x03000000u) { ++k; a += curFrame.ac[i].altFt; }
            const float tk = std::min(1.0f, (float) k / 4.0f), ta = k > 0 ? std::min(1.0f, a / (float) k / 40000.0f) : 0.0f;
            clsCount[c] += (tk - clsCount[c]) * tc; clsAlt[c] += (ta - clsAlt[c]) * tc;
        }
    }

    if (follow)
    {   // this Aeola only forwards its class from the source Aeola: no data, no mapping, no sound of its own
        engine.releaseSkyExcept(nullptr, 0);
        if (nPrev > 0) { MapResult empty; logOrders(empty); }
        mapRes = MapResult(); emitMidi(mapRes, true);
    }
    else if (!skyOn)
    {
        if (nPrev > 0) { engine.releaseSkyExcept(nullptr, 0); MapResult empty; logOrders(empty); }
        if (nPrevMidi > 0) { MapResult empty; emitMidi(empty); }
        mapRes = MapResult();
    }
    else if (!engine.isHold())
    {
        const double basis = simTransport ? simBaseEpoch + hostSeconds : wallEpoch;
        SkySettings mc = cfg;                          // per-class modulation (level, octave) applies to the mapping only
        mc.consonance = clamp01(cfg.consonance + consMod); mc.evolveDepth = clamp01(cfg.evolveDepth + evolveMod);
        mc.rangeOct = std::max(0.5f, std::min(4.0f, cfg.rangeOct + rangeMod)); mc.ghostLevel = clamp01(cfg.ghostLevel + ghostMod);
        mc.loopStretch = std::max(0.5f, std::min(2.0f, cfg.loopStretch + stretchMod));
        for (int c = 0; c < kNumClasses; ++c) { mc.cls[c].attackMul = std::max(0.25f, std::min(4.0f, cfg.cls[c].attackMul + clsSwellMod[c])); mc.cls[c].chop = clamp01(cfg.cls[c].chop + clsChopMod[c]); }
        for (int c = 0; c < kNumClasses; ++c)
        {
            mc.cls[c].levelDb = std::max(-12.0f, std::min(6.0f, cfg.cls[c].levelDb + clsLevelMod[c]));
            mc.cls[c].octave = std::max(-2, std::min(2, cfg.cls[c].octave + (int) std::lround(clsOctMod[c])));
        }
        mapper.map(curFrame, mc, basis, mapRes);
        uint32_t keep[kMaxOrders];
        for (int i = 0; i < mapRes.n; ++i)
        {
            const auto& o = mapRes.v[i];
            keep[i] = o.id;
            VoiceStyle vs; vs.timbre = o.timbre; vs.blend = o.blend; vs.attackMul = o.attackMul; vs.releaseMul = o.releaseMul; vs.chopRate = o.chopRate; vs.chopDepth = o.chopDepth;
            float lvl = o.level, pan = o.pan;
            if ((o.id & kAcVoiceBit) && !o.anchor && !o.ghost)
            {   // 1.12 mixer: fader and pan per class, and the class's own bus (meter, separate output)
                const int c = std::max(0, std::min(kNumClasses - 1, o.cls)); const float db = P(ID::classMixId(c, 0));
                lvl *= db <= -59.9f ? 0.0f : std::pow(10.0f, db / 20.0f);
                pan = std::max(-1.0f, std::min(1.0f, pan + P(ID::classMixId(c, 1))));
                vs.bus = 1 + c;
            }
            engine.skyVoice(o.id, o.note, lvl, pan, o.brightOff, o.breath, o.vib, o.anchor || o.ghost, &vs);
        }
        engine.releaseSkyExcept(keep, mapRes.n);
        logOrders(mapRes);
        emitMidi(mapRes);
    }

    // ---- UI snapshot (seqlock) ----
    uiWork.nVoices = 0;
    for (const auto& v : engine.getVoices())
        if (v.audible() && uiWork.nVoices < kSlots)
        {
            auto& u = uiWork.v[uiWork.nVoices++];
            u.id = v.id; u.note = v.noteInt; u.freq = (float) midiToHz(v.pitch + v.bend + v.cents / 100.0f);
            u.level = v.env * v.level; u.pan = v.pan; u.active = v.state == Voice::Held;
            u.kind = !v.isSky ? 0 : (v.anchor ? (v.id >= 6000 ? 3 : 2) : 1);
        }
    uiWork.nAc = std::min(curFrame.count, kMaxAc);
    for (int i = 0; i < uiWork.nAc; ++i)
    {
        const auto& a = curFrame.ac[i]; auto& u = uiWork.ac[i];
        u.lat = a.lat; u.lon = a.lon; u.alt = a.altFt; u.track = a.trackDeg; u.id = a.id; u.category = a.category;
        u.gs = a.gsKt; u.vrate = a.vrateFpm; u.distNm = a.distNm; std::memcpy(u.type, a.type, sizeof u.type);
        std::memcpy(u.callsign, a.callsign, sizeof u.callsign); u.voiced = 0; u.note = 0;
        for (int k = 0; k < mapRes.n; ++k) if (mapRes.v[k].id == (kAcVoiceBit | a.id)) { u.voiced = 1; u.note = mapRes.v[k].note; }
        u.status = 0; u.wantNote = 0;
        for (int k = 0; k < mapRes.nOut; ++k) if (mapRes.outcome[k].id == a.id) { u.status = mapRes.outcome[k].status; u.wantNote = mapRes.outcome[k].want; u.swell = mapRes.outcome[k].swell; }
    }
    uiWork.nOut = nOutAc;
    for (int i = 0; i < nOutAc; ++i)
    {
        const auto& a = outAc[i]; auto& u = uiWork.out[i]; u = UiAircraft();
        u.lat = a.lat; u.lon = a.lon; u.alt = a.altFt; u.track = a.trackDeg; u.id = a.id; u.category = a.category; u.gs = a.gsKt; u.vrate = a.vrateFpm; u.distNm = a.distNm;
        std::memcpy(u.type, a.type, sizeof u.type); std::memcpy(u.callsign, a.callsign, sizeof u.callsign);
    }
    uiWork.nGround = std::min(curFrame.nGround, kMaxGround);
    for (int i = 0; i < uiWork.nGround; ++i)
    {
        const auto& a = curFrame.ground[i]; auto& u = uiWork.gnd[i]; u = UiAircraft();
        u.lat = a.lat; u.lon = a.lon; u.id = a.id; u.category = a.category; u.status = 6; u.distNm = a.distNm;
        std::memcpy(u.callsign, a.callsign, sizeof u.callsign); std::memcpy(u.type, a.type, sizeof u.type);
    }
    uiWork.baseNote = cfg.baseNote; uiWork.rangeOct = cfg.rangeOct; uiWork.rootPc = cfg.rootPc; uiWork.scale = cfg.scale; uiWork.lowNote = cfg.baseNote - 12; uiWork.highNote = cfg.baseNote + (int) std::lround(cfg.rangeOct * 12.0f) + 14;
    uiWork.apLat = cfg.apLat; uiWork.apLon = cfg.apLon; uiWork.radiusNm = cfg.radiusNm;
    uiWork.roughness = mapRes.roughness; uiWork.accepted = mapRes.accepted; uiWork.rejected = mapRes.rejected;
    uiWork.shifted = mapRes.shifted; uiWork.ghosts = mapRes.ghosts; uiWork.regShift = mapRes.regShiftDeg;
    uiWork.source = src; uiWork.requested = requestedSource; uiWork.airport = (int) P(ID::airport);
    std::snprintf(uiWork.feedName, sizeof uiWork.feedName, "%s", feed.sourceName()); uiWork.liveStatus = feed.status(); uiWork.epoch = curFrame.epoch; uiWork.ageSec = age;
    uiWork.nReal = 0; for (int i = 0; i < uiWork.nAc; ++i) if ((uiWork.ac[i].id & 0xFF000000u) != 0x03000000u) ++uiWork.nReal;
    uiWork.capped = uiWork.nReal >= kMaxAc;
    uiWork.retryIn = feed.status() == 2 ? std::max(0.0, feed.retryAt() - (double) juce::Time::currentTimeMillis() / 1000.0) : 0.0;
    if (feed.status() == 2 && skyTickCount % 10 == 0) { auto e = feed.lastError(); std::snprintf(uiWork.feedErr, sizeof uiWork.feedErr, "%s", e.toRawUTF8()); }      // ScopedLock: once a second is plenty
    else if (feed.status() != 2) uiWork.feedErr[0] = 0;
    ++skyTickCount;
    uiWork.deterministic = (src == 4 && simTransport) || src == 3; uiWork.audition = auditioning;
    uiWork.peak = peakHold; peakHold *= 0.7f;
    float den = std::sqrt(corrL * corrR); uiWork.corr = den > 1e-9f ? corrNum / den : 1.0f; corrNum = corrL = corrR = 0;
    uiVer.fetch_add(1, std::memory_order_acq_rel); uiSlot = uiWork; uiVer.fetch_add(1, std::memory_order_release);
}

void AeolaDroneProcessor::emitMidi(const MapResult& r, bool fromBus)
{
    const bool on = P(ID::midiout) > 0.5f, perClass = P(ID::midimode) < 0.5f;
    const int onlyCls = (int) P(ID::midiclass) - 1;                  // -1 = all classes
    auto& bus = midiBus();
    MidiBus::R all[kMaxOrders]; int nAll = 0;                          // every aircraft note, before the class filter
    if (on && fromBus)
    {   juce::SpinLock::ScopedLockType g(bus.lock); nAll = bus.n; for (int i = 0; i < nAll; ++i) all[i] = bus.r[i]; }
    else if (on)
    {
        for (int k = 0; k < r.n && nAll < kMaxOrders; ++k)
        {
            const auto& o = r.v[k];
            if (!(o.id & kAcVoiceBit) || o.anchor || o.ghost) continue;      // aircraft only: no drone bed, no ghosts
            all[nAll++] = { o.id, (uint8_t) juce::jlimit(0, 8, o.cls), (uint8_t) juce::jlimit(0, 127, o.note),
                            (uint8_t) juce::jlimit(20, 127, (int) std::lround(30.0f + 97.0f * o.level)) };
        }
        juce::SpinLock::ScopedLockType g(bus.lock);
        if (bus.owner == this) { bus.n = nAll; for (int i = 0; i < nAll; ++i) bus.r[i] = all[i]; }
    }
    MidiPrev want[kMaxOrders]; uint8_t vel[kMaxOrders]; int nWant = 0;
    for (int k = 0; k < nAll; ++k)
    {
        if (onlyCls >= 0 && all[k].cls != onlyCls) continue;               // filter at output only: the mapping stays identical
        vel[nWant] = all[k].vel;
        want[nWant++] = { all[k].id, (uint8_t) (perClass ? 1 + all[k].cls : 1), all[k].note };
    }
    auto push = [&](const MidiPrev& p, uint8_t v, bool isOn) { if (nPendMidi < kMaxMidiEv) pendMidi[nPendMidi++] = { p.ch, p.note, v, isOn }; };
    for (int i = 0; i < nPrevMidi; ++i)
    {
        bool still = false;
        for (int k = 0; k < nWant; ++k) if (want[k].id == prevMidi[i].id && want[k].note == prevMidi[i].note && want[k].ch == prevMidi[i].ch) { still = true; break; }
        if (!still) push(prevMidi[i], 0, false);
    }
    for (int k = 0; k < nWant; ++k)
    {
        bool had = false;
        for (int i = 0; i < nPrevMidi; ++i) if (prevMidi[i].id == want[k].id && prevMidi[i].note == want[k].note && prevMidi[i].ch == want[k].ch) { had = true; break; }
        if (!had) push(want[k], vel[k], true);
    }
    nPrevMidi = nWant;
    for (int k = 0; k < nWant; ++k) prevMidi[k] = want[k];
}

void AeolaDroneProcessor::logOrders(const MapResult& r)
{
    auto chanFor = [](const VoiceOrder& o) -> uint8_t { return (o.anchor || o.ghost) ? 3 : 1; };
    for (int i = 0; i < nPrev; ++i)
    {
        bool still = false;
        for (int k = 0; k < r.n; ++k) if (r.v[k].id == prevOrders[i].id && r.v[k].note == prevOrders[i].note) { still = true; break; }
        if (!still) log.push(sampleClock, prevOrders[i].note, 0, prevOrders[i].ch, false);
    }
    for (int k = 0; k < r.n; ++k)
    {
        bool had = false;
        for (int i = 0; i < nPrev; ++i) if (prevOrders[i].id == r.v[k].id && prevOrders[i].note == r.v[k].note) { had = true; break; }
        if (!had) log.push(sampleClock, r.v[k].note, (int) (40 + 80 * r.v[k].level), chanFor(r.v[k]), true);
    }
    nPrev = r.n;
    for (int k = 0; k < r.n; ++k) prevOrders[k] = { r.v[k].id, r.v[k].note, chanFor(r.v[k]) };
}

// ---- audio ----------------------------------------------------------------------------------
void AeolaDroneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    applyParams();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto t = pos->getTimeInSeconds()) hostSeconds = *t;
            hostTime.hasBpm = pos->getBpm().hasValue(); if (hostTime.hasBpm) hostTime.bpm = *pos->getBpm();
            hostTime.hasPpq = pos->getPpqPosition().hasValue(); if (hostTime.hasPpq) hostTime.ppq = *pos->getPpqPosition();
            hostTime.playing = pos->getIsPlaying();
            hostTimeSeen = hostTime;
        }

    auto mainBus = getBusBuffer(buffer, false, 0);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
    const bool auxOn = getBusCount(false) > 1 && getBus(false, 1) != nullptr && getBus(false, 1)->isEnabled();
    const bool sendMode = auxOn && P(ID::rvsend) > 0.5f;
    clsRoute = 0;
    if (classOutsOn.load())
        for (int c = 0; c < kNumClasses; ++c)
            if (getBusCount(false) > c + 2 && getBus(false, c + 2) != nullptr && getBus(false, c + 2)->isEnabled() && getBus(false, c + 2)->getNumberOfChannels() == 2
                && getChannelIndexInProcessBlockBuffer(false, c + 2, 1) < buffer.getNumChannels()) clsRoute |= 1u << c;

    auto it = midi.begin();
    for (int base = 0; base < n; base += blockMax)
    {
        const int len = std::min(blockMax, n - base);
        int cur = 0;
        while (it != midi.end() && (*it).samplePosition < base + len)
        {
            const auto meta = *it; ++it;
            int at = std::max(cur, meta.samplePosition - base);
            renderSegment(cur, at); cur = at;
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                engine.noteOn(m.getNoteNumber(), m.getFloatVelocity()); velSrc = m.getFloatVelocity();
                log.push(sampleClock, m.getNoteNumber(), m.getVelocity(), 2, true);
            }
            else if (m.isNoteOff())
            {
                engine.noteOff(m.getNoteNumber(), sustainDown);
                if (!sustainDown) log.push(sampleClock, m.getNoteNumber(), 0, 2, false);
            }
            else if (m.isController())
            {
                if (m.getControllerNumber() == 64) { bool d = m.getControllerValue() >= 64; if (sustainDown && !d) engine.sustainChanged(false); sustainDown = d; }
                else if (m.getControllerNumber() == 1) modWheel = (float) m.getControllerValue() / 127.0f;
                else if (m.getControllerNumber() == 123 || m.getControllerNumber() == 120) engine.allNotesOff(false);
            }
            else if (m.isPitchWheel())
            {
                bendNorm = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
                engine.setBend(bendNorm * P(ID::bendrange));
            }
            else if (m.isChannelPressure()) atSrc = (float) m.getChannelPressureValue() / 127.0f;
            else if (m.isAftertouch()) atSrc = (float) m.getAfterTouchValue() / 127.0f;
        }
        renderSegment(cur, len);

        // ---- FX chain on this chunk ----
        float* ch[2] = { sL.data(), sR.data() };
        juce::AudioBuffer<float> work(ch, 2, len);
        juce::dsp::AudioBlock<float> blk(work);
        {   // warmth: tanh saturation at 2x oversampling
            auto up = os->processSamplesUp(blk);
            const int un = (int) up.getNumSamples();
            float w0 = smWarm.getCurrentValue(); float w1 = w0;
            for (int i = 0; i < len; ++i) w1 = smWarm.getNextValue();
            for (int i = 0; i < un; ++i)
            {
                float w = w0 + (w1 - w0) * (float) i / (float) std::max(1, un);
                float d = 1.0f + 5.0f * w;
                for (size_t c = 0; c < 2; ++c) { float* p = up.getChannelPointer(c); p[i] = std::tanh(d * p[i]) / d; }
            }
            os->processSamplesDown(blk);
        }
        // reverb (wet only), with pre-delay
        for (int i = 0; i < len; ++i)
        {
            preDelay.pushSample(0, sL[(size_t) i]); preDelay.pushSample(1, sR[(size_t) i]);
            wetL[(size_t) i] = preDelay.popSample(0); wetR[(size_t) i] = preDelay.popSample(1);
        }
        {
            float* wch[2] = { wetL.data(), wetR.data() };
            juce::AudioBuffer<float> wb(wch, 2, len); juce::dsp::AudioBlock<float> wblk(wb);
            juce::dsp::ProcessContextReplacing<float> ctx(wblk); reverb.process(ctx);
        }
        // mix + width
        float* outL = mainBus.getWritePointer(0) + base; float* outR = mainBus.getWritePointer(1) + base;
        float* auxL = nullptr; float* auxR = nullptr;
        if (auxOn) { auto ab = getBusBuffer(buffer, false, 1); auxL = ab.getWritePointer(0) + base; auxR = ab.getWritePointer(1) + base; }
        for (int i = 0; i < len; ++i)
        {
            float mix = smMix.getNextValue() * 1.5f, wd = smWidth.getNextValue();
            float dl = sL[(size_t) i], dr = sR[(size_t) i];
            float wl = wetL[(size_t) i] * mix, wr = wetR[(size_t) i] * mix;
            float l = sendMode ? dl : dl + wl, r = sendMode ? dr : dr + wr;
            float mid = 0.5f * (l + r), side = 0.5f * (l - r) * wd;
            outL[i] = mid + side; outR[i] = mid - side;
            if (auxOn) { auxL[i] = wl; auxR[i] = wr; }
        }
        for (int c = 0; c < kNumClasses; ++c)
            if ((clsRoute >> c) & 1u)
            {   auto cb = getBusBuffer(buffer, false, c + 2); float* oL = cb.getWritePointer(0) + base; float* oR = cb.getWritePointer(1) + base;
                for (int i = 0; i < len; ++i) { oL[i] = juce::jlimit(-1.0f, 1.0f, clsBufL[c][(size_t) i]); oR[i] = juce::jlimit(-1.0f, 1.0f, clsBufR[c][(size_t) i]); } }
        for (int c = 0; c < kNumClasses; ++c) { const float pk = engine.takeClassPeak(c); clsPeakA[c].store(std::max(pk, clsPeakA[c].load(std::memory_order_relaxed) * 0.85f), std::memory_order_relaxed); }
        {   // brickwall-ish limiter on the main output
            juce::dsp::AudioBlock<float> mb(mainBus); auto sub = mb.getSubBlock((size_t) base, (size_t) len);
            juce::dsp::ProcessContextReplacing<float> ctx(sub); limiter.process(ctx);
        }
        for (int i = 0; i < len; ++i)
        {
            outL[i] = juce::jlimit(-1.0f, 1.0f, outL[i]); outR[i] = juce::jlimit(-1.0f, 1.0f, outR[i]);
            peakHold = std::max(peakHold, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
            corrNum += outL[i] * outR[i]; corrL += outL[i] * outL[i]; corrR += outR[i] * outR[i];
        }
    }

    // ---- MIDI out: replace the incoming notes (already played by Aeola) with the aircraft notes ----
    const bool midiOn = P(ID::midiout) > 0.5f;
    if (midiOn) midi.clear();
    for (int i = 0; i < nPendMidi; ++i)
    {
        const auto& e = pendMidi[i];
        midi.addEvent(e.on ? juce::MidiMessage::noteOn((int) e.ch, (int) e.note, e.vel) : juce::MidiMessage::noteOff((int) e.ch, (int) e.note), 0);
    }
    nPendMidi = 0;
    if (midiOn && P(ID::midisilent) > 0.5f) buffer.clear();
}

// ---- GUI-facing -----------------------------------------------------------------------------
bool AeolaDroneProcessor::getUi(UiSnapshot& out) const
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        uint32_t v1 = uiVer.load(std::memory_order_acquire);
        if (v1 & 1) continue;
        out = uiSlot;
        std::atomic_thread_fence(std::memory_order_acquire);
        if (uiVer.load(std::memory_order_relaxed) == v1) return v1 > 0;
    }
    return false;
}

int AeolaDroneProcessor::getTimbrePreview(float* ratio, float* amp) const
{
    // spectrum of a note in the middle of the instrument's range (about 196 Hz), without life movement
    return engine.previewSpectrum(196.0, ratio, amp);
}

juce::String AeolaDroneProcessor::statusLine() const
{
    UiSnapshot u; getUi(u);
    const char* srcNames[] = { "WAITING", "LIVE", "CACHED", "REPLAY", "SIMULATED" };
    juce::String key = noteNames()[(int) *raw.at(ID::key)], sc = scales()[(int) *raw.at(ID::scale)].name;
    if (*raw.at(ID::skyon) < 0.5f) return "Sky off  -  play with MIDI";
    return (isCustomPlace() ? placeName().upToFirstOccurrenceOf(",", false, false).substring(0, 22).toUpperCase() : juce::String(iataOf(makePlace().code))) + juce::String::fromUTF8("  \xc2\xb7  ") + srcNames[std::min(4, std::max(0, u.source))] + juce::String::fromUTF8("  \xc2\xb7  ") + juce::String(u.nAc) + " aircraft  \u00b7  " + key + " " + sc;
}

juce::String AeolaDroneProcessor::hintLine() const
{
    UiSnapshot u; getUi(u);
    if (followingBus.load()) return juce::String("Following the Aeola set to All classes. This track gets ") + className((int) *raw.at(ID::midiclass) - 1) + " notes only.";
    int mode = (int) *raw.at(ID::source);
    if (*raw.at(ID::skyon) < 0.5f) return "Sky is off. Turn it on to let air traffic play, or play the instrument with MIDI.";
    if (mode == 0 && u.source == 2) return "Live traffic is unavailable, so this is the last real snapshot. It will switch back to live traffic by itself.";
    if (mode == 0 && u.source == 0 && u.nAc == 0 && u.feedErr[0]) return "No traffic data right now: only the drone bed plays. Choose Simulated in Sky source if you want invented traffic.";
    if (mode == 1 && u.nAc == 0) return "Waiting for live traffic...";
    if (u.roughness > 0.33f && *raw.at(ID::consonance) < 0.9f) return "Some notes are clashing. Raise Consonance, or choose a pentatonic scale.";
    if (u.rejected > 2) return "Several aircraft were muted to keep it musical. Lower Consonance if you want more tension.";
    if (u.nAc == 0) return "Quiet sky - soft ghost notes keep the harmony going.";
    if (u.deterministic) return "This sky is repeatable: bounces will be identical.";
    return "The sky is playing in key. Move North to hear the beating between notes change.";
}

int AeolaDroneProcessor::classOutputsActive() const
{
    int n = 0; for (int c = 0; c < kNumClasses; ++c) if (getBusCount(false) > c + 2 && getBus(false, c + 2) != nullptr && getBus(false, c + 2)->isEnabled()) ++n;
    return n;
}

void AeolaDroneProcessor::pollMessageThreadTasks()
{
    Place pl = makePlace();
    if (pl.dataKey() != apKeyLast) { apKeyLast = pl.dataKey(); placeSince.store((double) juce::Time::currentTimeMillis() / 1000.0); feed.setPlace(pl); feed.loadCache(pl); }
    feed.setPollSeconds(20);                              // the fastest pace; the daily request budget (adsb.lol: 500 a day) normally makes it slower, see budgetIntervalSec
    feed.setBackupEnabled((bool) getProp("backupFeed", false));
    classOutsOn.store((bool) getProp("classOuts", false));
    int mode = (int) P(ID::source);
    feed.setEnabled(P(ID::skyon) > 0.5f && (mode == 0 || mode == 1) && !followingBus.load());
    log.drain(sampleClock);
}

bool AeolaDroneProcessor::exportMidi(const juce::File& f) { log.drain(sampleClock); return log.exportTo(f, sampleClock, 30.0); }

// ---- presets, scenes, helpers ---------------------------------------------------------------
void AeolaDroneProcessor::loadFactoryPreset(int i)
{
    auto& p = factoryPresets(); if (i < 0 || i >= (int) p.size()) return;
    currentPreset = i; applyPreset(apvts, p[(size_t) i]);
}

juce::StringArray AeolaDroneProcessor::userPresetNames() const
{
    juce::StringArray n; for (auto& f : userPresetDir().findChildFiles(juce::File::findFiles, false, "*.aeolapreset")) n.add(f.getFileNameWithoutExtension());
    n.sort(true); return n;
}
bool AeolaDroneProcessor::saveUserPreset(const juce::String& name)
{
    auto state = apvts.copyState(); state.removeProperty("capture", nullptr);
    if (auto xml = state.createXml()) return xml->writeTo(userPresetDir().getChildFile(juce::File::createLegalFileName(name) + ".aeolapreset"));
    return false;
}
bool AeolaDroneProcessor::loadUserPreset(const juce::String& name)
{
    auto f = userPresetDir().getChildFile(juce::File::createLegalFileName(name) + ".aeolapreset");
    if (auto xml = juce::XmlDocument::parse(f))
    {
        auto vt = juce::ValueTree::fromXml(*xml);
        if (!vt.isValid()) return false;
        auto cap = apvts.state.getProperty("capture");
        apvts.replaceState(vt); syncParamsFromState(); apvts.state.setProperty("capture", cap, nullptr);
        return true;
    }
    return false;
}

void AeolaDroneProcessor::storeScene(int slot)
{
    scene[slot].clear();
    for (auto* prm : getParameters()) scene[slot].push_back(prm->getValue());
    sceneValid[slot] = true;
}
void AeolaDroneProcessor::morphScenes(float m)
{
    if (!hasScenes()) return;
    auto& ps = getParameters();
    for (int i = 0; i < ps.size() && i < (int) scene[0].size(); ++i)
    {
        auto* prm = ps[i];
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm)) { const auto id = rp->paramID; if (isPlaceOrSkyParam(id)) continue; }
        bool discrete = dynamic_cast<juce::AudioParameterChoice*>(prm) || dynamic_cast<juce::AudioParameterBool*>(prm) || dynamic_cast<juce::AudioParameterInt*>(prm);
        float v = discrete ? (m < 0.5f ? scene[0][(size_t) i] : scene[1][(size_t) i]) : scene[0][(size_t) i] + (scene[1][(size_t) i] - scene[0][(size_t) i]) * m;
        if (std::fabs(prm->getValue() - v) > 1e-6f) prm->setValueNotifyingHost(v);
    }
}

bool AeolaDroneProcessor::isPlaceOrSkyParam(const juce::String& id)
{
    if (id == ID::airport || id == ID::radius || id == ID::source || id == ID::skyon || id == ID::simseed || id == ID::hold || id == ID::gain || id == ID::midiout || id == ID::midimode || id == ID::midisilent || id == ID::midiclass) return true;
    for (int c = 0; c < kNumClasses; ++c) if (id == ID::classId(c, 5) || id == ID::classId(c, 6) || id == ID::classMixId(c, 0) || id == ID::classMixId(c, 1)) return true;
    return false;
}

void AeolaDroneProcessor::randomise()
{
    auto set = [&](const char* id, float actual) { if (auto* r = apvts.getParameter(id)) r->setValueNotifyingHost(r->convertTo0to1(actual)); };
    auto rf = [&](float a, float b) { return a + (b - a) * rng.nextFloat(); };
    static const int musical[] = { 0, 1, 2, 3, 4, 6, 7, 8, 10 };
    set(ID::brightness, rf(0.2f, 0.8f)); set(ID::beat, rng.nextFloat() < 0.5f ? 0.0f : rf(0.3f, 2.5f)); set(ID::drift, rf(0.15f, 0.6f));
    set(ID::attack, rf(1.0f, 10.0f)); set(ID::release, rf(3.0f, 15.0f)); set(ID::north, rng.nextFloat() < 0.4f ? 0.0f : rf(0.4f, 1.0f));
    set(ID::system, (float) rng.nextInt(4)); set(ID::scale, (float) musical[rng.nextInt(9)]); set(ID::key, (float) rng.nextInt(12));
    set(ID::srca, (float) rng.nextInt(numTimbres())); set(ID::srcb, (float) rng.nextInt(numTimbres())); set(ID::morph, simpleUi ? 0.0f : rf(0.0f, 1.0f)); set(ID::life, rf(0.25f, 0.7f));
    set(ID::rvmix, rf(0.2f, 0.6f)); set(ID::evolve, rf(0.3f, 0.9f)); set(ID::consonance, rf(0.6f, 1.0f));
    set(ID::cutoff, rf(2500.0f, 14000.0f)); set(ID::oddeven, rf(-0.3f, 0.5f)); set(ID::inharm, rng.nextFloat() < 0.7f ? 0.0f : rf(0.05f, 0.3f));
    currentPreset = -1;
}

juce::String AeolaDroneProcessor::autoFix()
{
    UiSnapshot u; getUi(u);
    juce::String msg; auto set = [&](const char* id, float v) { if (auto* r = apvts.getParameter(id)) r->setValueNotifyingHost(r->convertTo0to1(v)); };
    int sc = (int) P(ID::scale);
    if (sc == 9 || sc == 11) { set(ID::scale, 6); msg << "Switched to Pentatonic minor - chromatic and whole-tone scales are easy to make clash. "; }
    if (P(ID::consonance) < 0.85f) { set(ID::consonance, 0.9f); msg << "Raised Consonance to 90%. "; }
    if (P(ID::beat) > 3.0f) { set(ID::beat, 1.5f); msg << "Calmed the Beat control (it was shimmering very fast). "; }
    if (P(ID::anchor) < 0.5f) { set(ID::anchor, 1); msg << "Turned the Anchor drone on so notes have a reference. "; }
    if (P(ID::tension) > 0.7f) { set(ID::tension, 0.5f); msg << "Brought Tension back to the middle. "; }
    if (msg.isEmpty()) msg = "Nothing obviously wrong. If it still feels off, try a different Key, or a slower Glide and longer Attack for a softer feel.";
    return msg.trim();
}

// ---- capture / replay -----------------------------------------------------------------------
juce::String AeolaDroneProcessor::serialiseFrames(const std::vector<Frame>& v)
{
    juce::Array<juce::var> frames;
    for (auto& f : v)
    {
        auto* o = new juce::DynamicObject(); o->setProperty("e", f.epoch);
        juce::Array<juce::var> acs;
        for (int i = 0; i < f.count; ++i)
        {
            const auto& a = f.ac[i]; juce::Array<juce::var> r;
            r.add((int) a.id); r.add(juce::String(a.callsign)); r.add(juce::String(a.type)); r.add(a.category);
            r.add(a.lat); r.add(a.lon); r.add(a.altFt); r.add(a.gsKt); r.add(a.trackDeg); r.add(a.vrateFpm); r.add(a.distNm);
            acs.add(r);
        }
        o->setProperty("a", acs); frames.add(juce::var(o));
    }
    return juce::JSON::toString(juce::var(frames), true);
}
void AeolaDroneProcessor::parseFrames(const juce::String& s, std::vector<Frame>& out)
{
    out.clear(); auto v = juce::JSON::parse(s); if (!v.isArray()) return;
    for (auto& fv : *v.getArray())
    {
        Frame f; f.epoch = (double) fv["e"]; auto acs = fv["a"]; if (!acs.isArray()) continue;
        for (auto& r : *acs.getArray())
        {
            if (f.count >= kMaxAc || !r.isArray() || r.size() < 11) continue;
            Aircraft a; a.id = (uint32_t) (int) r[0];
            std::snprintf(a.callsign, sizeof a.callsign, "%s", r[1].toString().toRawUTF8());
            std::snprintf(a.type, sizeof a.type, "%s", r[2].toString().toRawUTF8());
            a.category = (int) r[3]; a.lat = (double) r[4]; a.lon = (double) r[5]; a.altFt = (float) (double) r[6]; a.gsKt = (float) (double) r[7];
            a.trackDeg = (float) (double) r[8]; a.vrateFpm = (float) (double) r[9]; a.distNm = (float) (double) r[10];
            f.ac[f.count++] = a;
        }
        out.push_back(f);
    }
}
void AeolaDroneProcessor::publishReplay(const std::vector<Frame>& v)
{
    replayKey = makePlace().dataKey();
    int back = 1 - replayIdx.load();
    int c = (int) std::min<size_t>(v.size(), 16);
    for (int i = 0; i < c; ++i) replayBuf[back][i] = v[v.size() - (size_t) c + (size_t) i];
    replayCount[back] = c; replayStartClock = sampleClock;
    replayIdx.store(back, std::memory_order_release);
}
void AeolaDroneProcessor::freezeSkyForRender()
{
    std::vector<Frame> all; { const juce::ScopedLock l(capLock); all = captured; }
    if (all.empty()) return;
    std::sort(all.begin(), all.end(), [](const Frame& a, const Frame& b) { return a.epoch < b.epoch; });
    size_t startIdx = all.size() - 1;                                    // newest contiguous run (gaps <= 20 min)
    while (startIdx > 0 && all[startIdx].epoch - all[startIdx - 1].epoch <= 1200.0) --startIdx;
    std::vector<Frame> copy(all.begin() + (long) startIdx, all.end());
    publishReplay(copy);
    setProp("capture", serialiseFrames(copy)); setProp("captureKey", (juce::int64) makePlace().dataKey());
    if (auto* r = apvts.getParameter(ID::source)) r->setValueNotifyingHost(r->convertTo0to1(3.0f));
}

// JUCE's APVTS skips a restore when its cached (snapped) value already equals the saved one, which leaves
// an unsnapped bool/choice value (e.g. from host fuzzing) behind. Force every parameter to the saved value.
void AeolaDroneProcessor::syncParamsFromState()
{
    for (auto child : apvts.state)
    {
        if (!child.hasType("PARAM")) continue;
        if (auto* prm = apvts.getParameter(child.getProperty("id").toString()))
        {
            float n = prm->convertTo0to1((float) (double) child.getProperty("value"));
            if (std::fabs(prm->getValue() - n) > 1e-6f) prm->setValueNotifyingHost(n);
        }
    }
}

void AeolaDroneProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    { int n; aeola::airports(n); setProp("customIdx", n); }        // lets a later version with more airports find "Custom place" again
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void AeolaDroneProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto vt = juce::ValueTree::fromXml(*xml);
        if (vt.isValid())
        {
            const bool v13 = vt.getChildWithProperty("id", ID::radius).isValid();          // projects from before v1.3 lack the place / class parameters
            juce::StringArray saved; for (auto child : vt) if (child.hasType("PARAM")) saved.add(child.getProperty("id").toString());
            apvts.replaceState(vt); syncParamsFromState();
            for (auto* prm : getParameters())       // parameters newer than the project (1.12: mixer faders at -inf would silence everything) get their default
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) if (!saved.contains(r->paramID)) r->setValueNotifyingHost(r->getDefaultValue());
            if (!v13)
            {   // parameters that did not exist yet come back from replaceState at their minimum: give them their defaults, and the Classic mapping
                auto toDefault = [&](const char* id) { if (auto* q = apvts.getParameter(id)) q->setValueNotifyingHost(q->getDefaultValue()); };
                for (auto* id : { ID::radius, ID::contrast, ID::swell, ID::simlevel }) toDefault(id);
                for (int c = 0; c < kNumClasses; ++c) for (int k = 0; k < 7; ++k) toDefault(ID::classId(c, k));
            }
            {   int n; aeola::airports(n); const int savedN = (int) apvts.state.getProperty("customIdx", n);
                if (savedN != n && (int) P(ID::airport) == savedN) if (auto* q = apvts.getParameter(ID::airport)) q->setValueNotifyingHost(q->convertTo0to1((float) n)); }
            custLat.store((double) apvts.state.getProperty("placeLat", 59.6519)); custLon.store((double) apvts.state.getProperty("placeLon", 17.9186));
        }
        juce::String cap = apvts.state.getProperty("capture", "").toString();
        if (cap.isNotEmpty())
        {
            std::vector<Frame> v; parseFrames(cap, v);
            if (!v.empty())
            {
                publishReplay(v);
                if (apvts.state.hasProperty("captureKey")) replayKey = (uint64_t) (juce::int64) apvts.state.getProperty("captureKey");
                else replayKey = placeFromAirport((int) apvts.state.getProperty("captureAirport", 0), 40.0).dataKey();      // projects saved before v1.3
            }
        }
    }
}

Place AeolaDroneProcessor::makePlace() const
{
    int n; aeola::airports(n);
    const int idx = (int) P(ID::airport); const double radius = P(ID::radius);
    if (idx < n) return placeFromAirport(idx, radius);
    Place p; p.lat = custLat.load(); p.lon = custLon.load(); p.radiusNm = radius; p.isAirport = false;
    std::snprintf(p.name, sizeof p.name, "Custom place"); std::snprintf(p.code, sizeof p.code, "PLACE");
    return p;
}
juce::String AeolaDroneProcessor::placeName() const
{
    if (!isCustomPlace()) { auto p = makePlace(); return juce::String(iataOf(p.code)) + "  " + juce::String(p.name); }
    auto nm = apvts.state.getProperty("placeName", "").toString();
    return nm.isNotEmpty() ? nm : juce::String(custLat.load(), 3) + ", " + juce::String(custLon.load(), 3);
}
std::vector<AeolaDroneProcessor::RecentPlace> AeolaDroneProcessor::recentPlaces() const
{
    std::vector<RecentPlace> v;
    for (auto& line : juce::StringArray::fromLines(apvts.state.getProperty("recentPlaces", "").toString()))
    {
        auto t = juce::StringArray::fromTokens(line, "|", "");
        if (t.size() == 3) { RecentPlace r; r.name = t[0]; r.lat = t[1].getDoubleValue(); r.lon = t[2].getDoubleValue(); v.push_back(r); }
    }
    return v;
}
void AeolaDroneProcessor::setCustomPlace(double lat, double lon, const juce::String& name)
{
    lat = juce::jlimit(-85.0, 85.0, lat); lon = juce::jlimit(-180.0, 180.0, lon);
    custLat.store(lat); custLon.store(lon);
    const juce::String nm = name.replaceCharacter('|', ' ').replaceCharacter('\n', ' ').trim();
    setProp("placeLat", lat); setProp("placeLon", lon); setProp("placeName", nm);
    // remember (newest first, 8 entries, no duplicates within ~1 km)
    juce::StringArray lines; lines.add(nm.isNotEmpty() ? nm + "|" + juce::String(lat, 4) + "|" + juce::String(lon, 4) : juce::String(lat, 3) + ", " + juce::String(lon, 3) + "|" + juce::String(lat, 4) + "|" + juce::String(lon, 4));
    for (auto& r : recentPlaces())
        if (std::fabs(r.lat - lat) > 0.01 || std::fabs(r.lon - lon) > 0.01) lines.add(r.name + "|" + juce::String(r.lat, 4) + "|" + juce::String(r.lon, 4));
    while (lines.size() > 8) lines.remove(lines.size() - 1);
    setProp("recentPlaces", lines.joinIntoString("\n"));
    int n; aeola::airports(n);
    if (auto* a = apvts.getParameter(ID::airport)) a->setValueNotifyingHost(a->convertTo0to1((float) n));
}

juce::AudioProcessorEditor* AeolaDroneProcessor::createEditor() { return new AeolaDroneEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AeolaDroneProcessor(); }
