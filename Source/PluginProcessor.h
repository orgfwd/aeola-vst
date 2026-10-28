// Aeola — plugin processor. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <deque>
#include <map>
#include "Params.h"
#include "DroneEngine.h"
#include "SkyMapper.h"
#include "SkyFeed.h"
#include "MidiLog.h"
#include "Presets.h"
#include "Modulation.h"

namespace aeola
{
struct UiVoice { uint32_t id = 0; int note = 0; float freq = 0, level = 0, pan = 0; uint8_t kind = 0, active = 0; };   // kind 0 played, 1 sky, 2 anchor, 3 ghost
struct UiAircraft { double lat = 0, lon = 0; float alt = 0, track = 0; uint32_t id = 0; int note = 0; uint8_t voiced = 0; char callsign[10] = {}; int category = 0; uint8_t status = 0; int wantNote = 0; float gs = 0, vrate = 0, distNm = 0, swell = 0; char type[6] = {}; };
struct UiSnapshot
{
    int nVoices = 0; UiVoice v[kSlots];
    int nGround = 0; UiAircraft gnd[kMaxGround];
    int nAc = 0; UiAircraft ac[kMaxAc];
    int nOut = 0; UiAircraft out[kMaxAc];      // 1.12: aircraft beyond the radius (silent, map only)
    int rootPc = 2, scale = 0, lowNote = 36, highNote = 86, baseNote = 48; float rangeOct = 2.5f; double apLat = 0, apLon = 0, radiusNm = 40;   // for the Profile view's pitch grid
    float roughness = 0, peak = 0, corr = 1; int source = 0, requested = 0, accepted = 0, rejected = 0, shifted = 0, ghosts = 0;
    int airport = 0, liveStatus = 0; bool audition = false; int nReal = 0; bool capped = false; char feedErr[64] = {}; char feedName[12] = "adsb.lol"; double retryIn = 0; double epoch = 0, ageSec = 0; bool deterministic = false; int regShift = 0;
};
}

class AeolaDroneProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    float P(const char* id) const { return raw.at(id)->load(); }
    AeolaDroneProcessor();
    ~AeolaDroneProcessor() override;
    bool isFollowing() const { return followingBus.load(); }
    void testReplay(const std::vector<aeola::Frame>& v) { publishReplay(v); replayKey = makePlace().dataKey(); }     // tests only: hand-built traffic
    float classPeak(int c) const { return clsPeakA[c].load(std::memory_order_relaxed); }
    int classOutputsActive() const;              // how many class outputs the host has switched on

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Aeola"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 20.0; }
    // 1.13.1: no host program list in the VST3 (the wrapper turns it into an automatable "Program" parameter, the 66th, and Live then
    // leaves its parameter list empty). Presets stay in Aeola's own menu; AU hosts still get the list.
    bool hostPrograms() const { return wrapperType != wrapperType_VST3; }
    int getNumPrograms() override { return hostPrograms() ? (int) aeola::factoryPresets().size() : 1; }
    int getCurrentProgram() override { return hostPrograms() ? currentPreset : 0; }
    void setCurrentProgram(int i) override { if (hostPrograms()) loadFactoryPreset(i); }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter("bypass"); }
    const juce::String getProgramName(int i) override { if (!hostPrograms()) return "Aeola"; auto& p = aeola::factoryPresets(); return i >= 0 && i < (int) p.size() ? juce::String(p[(size_t) i].name) : juce::String(); }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- message-thread API (GUI) ----
    void loadFactoryPreset(int i);
    juce::StringArray userPresetNames() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::String& name);
    void storeScene(int slot);                 // 0 = A, 1 = B
    void morphScenes(float amount);            // 0..1 between A and B
    bool hasScenes() const { return sceneValid[0] && sceneValid[1]; }
    void randomise();
    juce::String autoFix();                    // "Why does this sound off?" -> diagnosis text, applies fixes
    bool exportMidi(const juce::File& f);
    void freezeSkyForRender();                 // copy recent live traffic into the project (Replay source)
    int  capturedFrames() const { const juce::ScopedLock l(capLock); return (int) captured.size(); }
    bool getUi(aeola::UiSnapshot& out) const;
    // live modulation readout for the GUI: summed modulation per destination (fractions of range) and source values
    float modDestShown(int d) const { return modDst[d].load(std::memory_order_relaxed); }
    float modSrcShown(int s) const { return modSrc[s].load(std::memory_order_relaxed); }
    bool  hostHasTempo() const { return hostTimeSeen.hasBpm; }
    int  getTimbrePreview(float* ratio, float* amp) const;
    juce::String statusLine() const;
    juce::String hintLine() const;
    void pollMessageThreadTasks();             // feed settings + MIDI log drain (also called by the timer)
    juce::var getProp(const juce::Identifier& id, const juce::var& def) const { return apvts.state.getProperty(id, def); }
    void setProp(const juce::Identifier& id, const juce::var& v) { apvts.state.setProperty(id, v, nullptr); }
    aeola::SkyFeed& getFeed() { return feed; }
    // ---- place (v1.3): an airport from the list or a custom point, plus the radius ----
    aeola::Place makePlace() const;                                   // thread-safe
    static bool isPlaceOrSkyParam(const juce::String& id);          // where you listen, solo / mute: kept by presets and A/B
    bool simpleUi = true;                                           // the editor says which mode it shows (Randomise keeps one base sound in Simple)
    bool isCustomPlace() const { int n; aeola::airports(n); return (int) P(aeola::ID::airport) >= n; }
    void setCustomPlace(double lat, double lon, const juce::String& name);   // message thread: also selects "Custom place" and remembers it
    juce::String placeName() const;                                   // shown in the UI
    struct RecentPlace { juce::String name; double lat = 0, lon = 0; };
    std::vector<RecentPlace> recentPlaces() const;
    void setRadius(float nm) { if (auto* r = apvts.getParameter(aeola::ID::radius)) r->setValueNotifyingHost(r->convertTo0to1(nm)); }
    float getRadius() const { return P(aeola::ID::radius); }
    bool classFlag(int c, int what) const { return P(aeola::ID::classId(c, what)) > 0.5f; }          // what 5 = solo, 6 = mute
    void setClassFlag(int c, int what, bool v) { if (auto* p = apvts.getParameter(aeola::ID::classId(c, what))) p->setValueNotifyingHost(v ? 1.0f : 0.0f); }
    void clearClassFlags() { for (int c = 0; c < aeola::kNumClasses; ++c) { setClassFlag(c, 5, false); setClassFlag(c, 6, false); } }
    double elapsedSeconds() const { return sampleClock; }
    static juce::File userPresetDir() { auto d = aeola::SkyFeed::cacheDir().getChildFile("Presets"); d.createDirectory(); return d; }

    juce::AudioProcessorValueTreeState apvts;
    int currentPreset = -1;

private:
    void timerCallback() override { pollMessageThreadTasks(); }
    int lastSpecA = -1, lastSpecB = -1; float lastSpecMorph = -1, lastSpecInh = -1, lastSpecOdd = -9, lastSpecBright = -1, lastRvSize = -1, lastRvDamp = -1; uint32_t skyTickCount = 0;

    void applyParams();
    void renderSegment(int from, int to);
    void skyTick();
    bool pickFrame(aeola::Frame& out, int& srcUsed, double& ageSec);
    void logOrders(const aeola::MapResult& r);
    static juce::String serialiseFrames(const std::vector<aeola::Frame>& v);
    static void parseFrames(const juce::String& s, std::vector<aeola::Frame>& out);
    void publishReplay(const std::vector<aeola::Frame>& v);
    void syncParamsFromState();

    std::map<juce::String, std::atomic<float>*> raw;
    aeola::Engine engine;
    aeola::SkyMapper mapper; aeola::MapResult mapRes; aeola::SkySettings cfg;
    aeola::SkyFeed feed; aeola::MidiLog log;
    double sr = 44100; int blockMax = 512;
    std::vector<float> sL, sR, wetL, wetR;

    // modulation
    void updateMod(int len);
    aeola::Lfo lfo[2]; aeola::Walk walk; aeola::ModSlot slots[aeola::kModSlots];
    aeola::EngineParams baseP; float baseMix = 0.3f, baseWidth = 1.0f; bool anyMod = false;
    aeola::HostTime hostTime, hostTimeSeen;
    float velSrc = 0, atSrc = 0, bendNorm = 0, skyCount = 0, skyAlt = 0, skyProx = 0, skySpeed = 0;
    float clsCount[aeola::kNumClasses] = {}, clsAlt[aeola::kNumClasses] = {};          // per-class modulation sources (smoothed)
    float clsLevelMod[aeola::kNumClasses] = {}, clsOctMod[aeola::kNumClasses] = {};    // per-class modulation, dB and octaves
    float consMod = 0, evolveMod = 0;                                                   // 1.12 targets that act on the mapping
    float rangeMod = 0, ghostMod = 0, stretchMod = 0, clsSwellMod[aeola::kNumClasses] = {}, clsChopMod[aeola::kNumClasses] = {};   // 1.13
    float baseWarm = 0.15f, baseRvSize = 0.7f, baseRvDamp = 0.5f, baseLfoRate[2] = { 0.2f, 0.07f }; bool revModActive = false;
    std::vector<float> clsBufL[aeola::kNumClasses], clsBufR[aeola::kNumClasses];        // 1.12 class outputs
    uint32_t clsRoute = 0; std::atomic<bool> classOutsOn { false }; std::atomic<float> clsPeakA[aeola::kNumClasses];
    aeola::Aircraft outAc[aeola::kMaxAc]; int nOutAc = 0;                              // 1.12: traffic beyond the radius (map only)
    std::atomic<float> modDst[aeola::kNumDst], modSrc[aeola::kNumSrc];

    // FX
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::Reverb reverb; juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelay { 48000 * 2 };
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float> smWarm, smMix, smWidth;
    float corrNum = 0, corrL = 0, corrR = 0, peakHold = 0;

    // time & sky state
    double wallEpoch = 0, sampleClock = 0, skyAccum = 0, replayStartClock = 0;
    bool sustainDown = false; float modWheel = 0;
    double simBaseEpoch = 1792800000.0;
    bool simTransport = false; double hostSeconds = 0;
    aeola::Frame curFrame, tmpFrame;
    struct Prev { uint32_t id; int note; uint8_t ch; };
    Prev prevOrders[aeola::kMaxOrders]; int nPrev = 0;
    // MIDI out (prototype): one note per sounding aircraft, collected on the sky tick, flushed into the host MIDI buffer
    struct MidiEv { uint8_t ch, note, vel; bool on; };
    static constexpr int kMaxMidiEv = 256;
    MidiEv pendMidi[kMaxMidiEv]; int nPendMidi = 0;
    struct MidiPrev { uint32_t id; uint8_t ch, note; };
    MidiPrev prevMidi[aeola::kMaxOrders]; int nPrevMidi = 0;
    void emitMidi(const aeola::MapResult&, bool fromBus = false);
    bool updateMidiRole();                       // each sky tick: become the MIDI source, or follow it
    std::atomic<bool> followingBus { false };
    aeola::UiSnapshot uiWork; mutable std::atomic<uint32_t> uiVer { 0 }; aeola::UiSnapshot uiSlot;
    int lastSource = 0, requestedSource = 0;

    // capture / replay
    mutable juce::CriticalSection capLock; std::vector<aeola::Frame> captured;
    aeola::Frame replayBuf[2][16]; int replayCount[2] = { 0, 0 }; std::atomic<int> replayIdx { 0 };

    std::vector<float> scene[2]; bool sceneValid[2] = { false, false }; std::atomic<double> placeSince { 0.0 }; bool auditioning = false; uint64_t apKeyLast = 0; uint64_t lastCapKey = 0; uint64_t replayKey = 0;
    std::atomic<double> custLat { 59.6519 }, custLon { 17.9186 }; aeola::Place place;
    juce::Random rng { 1234 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AeolaDroneProcessor)
};
