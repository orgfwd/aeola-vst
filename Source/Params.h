// Aeola — parameter definitions (IDs, ranges, defaults, tooltips).
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Tuning.h"
#include "Sky.h"
#include "Timbre.h"
#include "Modulation.h"

namespace aeola
{
namespace ID
{
#define AEOLA_ID(name) static const char* const name = #name;
AEOLA_ID(srca) AEOLA_ID(srcb) AEOLA_ID(morph) AEOLA_ID(morphmod) AEOLA_ID(life) AEOLA_ID(air)
AEOLA_ID(brightness) AEOLA_ID(oddeven) AEOLA_ID(inharm) AEOLA_ID(beat) AEOLA_ID(drift) AEOLA_ID(driftrate)
AEOLA_ID(attack) AEOLA_ID(release) AEOLA_ID(voices) AEOLA_ID(panspread) AEOLA_ID(velsens) AEOLA_ID(breath)
AEOLA_ID(cutoff) AEOLA_ID(reso) AEOLA_ID(lowcut) AEOLA_ID(warmth)
AEOLA_ID(rvmix) AEOLA_ID(rvsize) AEOLA_ID(rvdamp) AEOLA_ID(rvpre) AEOLA_ID(width) AEOLA_ID(rvsend)
AEOLA_ID(system) AEOLA_ID(north) AEOLA_ID(rootmode) AEOLA_ID(fixedroot) AEOLA_ID(glide) AEOLA_ID(bendrange)
AEOLA_ID(skyon) AEOLA_ID(source) AEOLA_ID(airport) AEOLA_ID(maxac) AEOLA_ID(key) AEOLA_ID(scale) AEOLA_ID(range)
AEOLA_ID(baseoct) AEOLA_ID(consonance) AEOLA_ID(anchor) AEOLA_ID(anchorfifth) AEOLA_ID(ghostmin) AEOLA_ID(ghostlevel)
AEOLA_ID(stretch) AEOLA_ID(evolve) AEOLA_ID(evolveperiod) AEOLA_ID(simseed) AEOLA_ID(simsync) AEOLA_ID(poll)
AEOLA_ID(tension) AEOLA_ID(light) AEOLA_ID(motion) AEOLA_ID(space)
AEOLA_ID(gain) AEOLA_ID(modamt) AEOLA_ID(hold)
AEOLA_ID(midiout) AEOLA_ID(midimode) AEOLA_ID(midisilent) AEOLA_ID(midiclass)
AEOLA_ID(ftype) AEOLA_ID(drive) AEOLA_ID(ftrack) AEOLA_ID(fmix)
AEOLA_ID(l1shape) AEOLA_ID(l1rate) AEOLA_ID(l1sync) AEOLA_ID(l1div) AEOLA_ID(l2shape) AEOLA_ID(l2rate) AEOLA_ID(l2sync) AEOLA_ID(l2div)
AEOLA_ID(m1src) AEOLA_ID(m1dst) AEOLA_ID(m1amt) AEOLA_ID(m2src) AEOLA_ID(m2dst) AEOLA_ID(m2amt)
AEOLA_ID(contrast) AEOLA_ID(swell) AEOLA_ID(radius) AEOLA_ID(simlevel)
AEOLA_ID(m3src) AEOLA_ID(m3dst) AEOLA_ID(m3amt) AEOLA_ID(m4src) AEOLA_ID(m4dst) AEOLA_ID(m4amt)
AEOLA_ID(m5src) AEOLA_ID(m5dst) AEOLA_ID(m5amt) AEOLA_ID(m6src) AEOLA_ID(m6dst) AEOLA_ID(m6amt)
AEOLA_ID(m7src) AEOLA_ID(m7dst) AEOLA_ID(m7amt) AEOLA_ID(m8src) AEOLA_ID(m8dst) AEOLA_ID(m8amt)
#undef AEOLA_ID
// Per-class voice settings: c<class>timbre / level / oct / attack / chop / solo / mute (what 0..6)
inline const char* classId(int cls, int what)
{
    static const std::vector<std::string> names = [] {
        static const char* w[] = { "timbre", "level", "oct", "attack", "chop", "solo", "mute" };
        std::vector<std::string> v; for (int c = 0; c < 9; ++c) for (int i = 0; i < 7; ++i) v.push_back("c" + std::to_string(c) + w[i]);
        return v; }();
    return names[(size_t) (cls * 7 + what)].c_str();
}
inline const char* modSlotId(int slot, int what)          // what: 0 src, 1 dst, 2 amt
{
    static const char* t[8][3] = { { m1src, m1dst, m1amt }, { m2src, m2dst, m2amt }, { m3src, m3dst, m3amt }, { m4src, m4dst, m4amt },
                                   { m5src, m5dst, m5amt }, { m6src, m6dst, m6amt }, { m7src, m7dst, m7amt }, { m8src, m8dst, m8amt } };
    return t[slot][what];
}
// 1.12 mixer: per class fader (dB) and pan. what: 0 fader, 1 pan
inline const char* classMixId(int cls, int what)
{
    static const std::vector<std::string> names = [] { std::vector<std::string> v; for (int c = 0; c < 9; ++c) { v.push_back("c" + std::to_string(c) + "mix"); v.push_back("c" + std::to_string(c) + "pan"); } return v; }();
    return names[(size_t) (cls * 2 + what)].c_str();
}
// The parameters a host may automate. Ableton Live configures a plug-in by itself only up to 64 parameters, so this list is exactly 64:
// the performance controls, the macros and the mixer. Everything else is still saved and still a parameter, but not automatable.
inline bool isAutomatable(const juce::String& id)
{
    static const juce::StringArray keep = [] {
        juce::StringArray s { tension, light, motion, space, gain, morph, life, air, brightness, beat, drift, breath, attack, release,
                              cutoff, reso, drive, warmth, rvmix, rvsize, width, north, glide, consonance, evolve, radius, hold, maxac };
        for (int c = 0; c < 9; ++c) { s.add(classMixId(c, 0)); s.add(classMixId(c, 1)); s.add(classId(c, 5)); s.add(classId(c, 6)); }
        return s; }();
    return keep.contains(id);
}
// parameter that each modulation destination acts on (for the GUI's modulation rings)
inline const char* modDestParam(int dst)
{
    static const std::array<const char*, kNumDst> t = [] {
        std::array<const char*, kNumDst> a {};
        a[(size_t) DstInharm] = inharm; a[(size_t) DstOddEven] = oddeven; a[(size_t) DstBreath] = breath; a[(size_t) DstNorth] = north; a[(size_t) DstConsonance] = consonance; a[(size_t) DstEvolve] = evolve;
        { const char* t2[] = { attack, release, glide, driftrate, lowcut, warmth, fmix, ftrack, rvsize, rvdamp, range, ghostlevel, stretch, l1rate, l2rate };
          for (int i = 0; i < 15; ++i) a[(size_t) (DstAttack + i)] = t2[i]; }
        for (int c = 0; c < 9; ++c) { a[(size_t) (DstClassSwell + c)] = classId(c, 3); a[(size_t) (DstClassChop + c)] = classId(c, 4); }
        const char* base[DstClassLevel] = { "", brightness, morph, morphmod, life, air, beat, drift, cutoff, reso, drive, panspread, width, rvmix, gain };
        for (int i = 0; i < DstClassLevel; ++i) a[(size_t) i] = base[i];
        for (int c = 0; c < kNumClasses; ++c) { a[(size_t) (DstClassLevel + c)] = classId(c, 1); a[(size_t) (DstClassOctave + c)] = classId(c, 2); }
        return a; }();
    return t[(size_t) dst];
}
}

struct ParamInfo { const char* id; const char* tip; };
// One-sentence explanation + "what you'll hear" for every control (used for tooltips).
inline const char* tooltipFor(const juce::String& id)
{
    for (int c = 0; c < 9; ++c)
    {
        if (id == ID::classId(c, 0)) return "Timbre for this class. Global = the plugin's own sound.";
        if (id == ID::classId(c, 1)) return "Gain: this class's built-in loudness (dB), part of its voice and scaled by Class contrast. The Mix tab's fader comes after it.";
        if (id == ID::classId(c, 2)) return "Register of this class in octaves, relative to what altitude gives.";
        if (id == ID::classId(c, 3)) return "Attack and release speed of this class. Low = quick, high = slow swells.";
        if (id == ID::classId(c, 4)) return "Chop: amplitude beating like rotor blades or propellers.";
        if (id == ID::classId(c, 5)) return "Solo: hear only this class (all soloed classes play). If no aircraft of the class is overhead, a virtual one is played so you can tune the voice.";
        if (id == ID::classId(c, 6)) return "Mute this class.";
        if (id == ID::classMixId(c, 0)) return "Mixer fader for this class (dB). Separate from the class's Level on the Aircraft tab, so you can mix live without changing the sound.";
        if (id == ID::classMixId(c, 1)) return "Mixer pan for this class: moves all its aircraft left or right, on top of where they are in the sky.";
    }
    static const ParamInfo t[] = {
        { ID::srca, "Source A: the first tone colour. Hover over each choice in the list to see what it sounds like." },
        { ID::srcb, "Source B: the second tone colour. Morph blends from A to B." },
        { ID::morph, "Morph: blends continuously from Source A (left) to Source B (right). Overtones slide between the two spectra." },
        { ID::morphmod, "Morph motion: every tone slowly drifts between A and B on its own, so no two notes sound exactly alike." },
        { ID::life, "Life: every overtone wanders slightly in level and tuning, like a real instrument. 0 = frozen, high = shimmering." },
        { ID::air, "Air: the amount of breath and bow noise that belongs to the chosen sources." },
        { ID::brightness, "Colour: tilts the whole spectrum. Left = dark and round, right = bright and rich. It works on every source." },
        { ID::oddeven, "Odd/Even: balance of odd and even overtones. Odd = hollow, clarinet-like; even = fuller, organ-like." },
        { ID::inharm, "Inharmonicity: stretches the overtones out of tune. A little gives bell and glass colours." },
        { ID::beat, "Beat: a second, slightly detuned copy of every tone. You hear a slow shimmer; the number is the shimmer rate in Hz." },
        { ID::drift, "Drift: slow wandering of pitch and volume. Gives the drone a pulse of its own." },
        { ID::driftrate, "Drift rate: how quickly the wandering happens." },
        { ID::attack, "Attack: time for a tone to fade in." },
        { ID::release, "Release: time for a tone to fade out after it ends." },
        { ID::voices, "Voices: the most aircraft tones that can sound at once (the anchor drone is extra). Lower = simpler, calmer." },
        { ID::panspread, "Pan spread: how wide tones are spread across the stereo field." },
        { ID::velsens, "Velocity: how much your key velocity changes the volume of played notes." },
        { ID::breath, "Breath: every tone slowly swells and fades on its own loop length. Loop lengths differ so the pattern never repeats." },
        { ID::cutoff, "Cutoff: closes the sound like a curtain. Lower = darker." },
        { ID::reso, "Resonance: emphasis at the cutoff frequency." },
        { ID::lowcut, "Low cut: removes rumble below this frequency so the drone leaves room for your bass." },
        { ID::warmth, "Warmth: gentle tape-like saturation." },
        { ID::rvmix, "Reverb mix: how much room you hear." },
        { ID::rvsize, "Reverb size: small room to endless hall." },
        { ID::rvdamp, "Reverb damping: how quickly highs die away in the tail." },
        { ID::rvpre, "Pre-delay: gap before the reverb starts. Keeps the tone clear in front of the tail." },
        { ID::width, "Width: stereo width of the whole sound. Below 100% narrows toward mono." },
        { ID::rvsend, "Reverb on its own output: sends the reverb to the plugin's second stereo output so you can use your own reverb on the dry drone." },
        { ID::system, "Tuning system used when North is above zero. Just = sweet pure thirds, Pythagorean = open fifths, Harmonic = overtone-series flavour, Meantone = warm old keys." },
        { ID::north, "North: 0% = piano tuning (equal temperament). 100% = pure ratios. Move it and listen to the beating between notes melt away." },
        { ID::rootmode, "Which note pure tuning is built on: the lowest note currently sounding, or a fixed note you choose." },
        { ID::fixedroot, "The fixed root note for pure tuning." },
        { ID::glide, "Glide: how slowly pitches and tuning shifts glide to a new value. Short = snappy, long = drifting." },
        { ID::bendrange, "Pitch bend range in semitones for the pitch wheel." },
        { ID::skyon, "Sky: let the air traffic play the instrument." },
        { ID::source, "Where the sky comes from. Auto uses live data, then a saved copy, then a simulation. Replay plays back traffic captured in this project. Simulated needs no internet." },
        { ID::airport, "Which airport's sky to use." },
        { ID::maxac, "Most aircraft that are allowed to sound at once (also limited by Voices). The sky view shows how many are sounding." },
        { ID::key, "Key (root note) that all Sky notes belong to." },
        { ID::scale, "Scale that all Sky notes belong to. Pentatonic scales are the safest; chromatic allows anything." },
        { ID::range, "Range: how many octaves the altitude range covers." },
        { ID::baseoct, "Base octave: the lowest register Sky notes use." },
        { ID::consonance, "Consonance: how strictly harsh intervals are avoided. High = always pleasant, low = tension and clusters allowed." },
        { ID::anchor, "Anchor: a steady root note so everything else has a reference." },
        { ID::anchorfifth, "Adds a fifth above the anchor when it is in the scale." },
        { ID::ghostmin, "Quiet sky: if fewer aircraft than this are sounding, soft 'ghost' notes from the scale fill in." },
        { ID::ghostlevel, "Level of the ghost notes." },
        { ID::stretch, "Loop stretch: makes all breathing loops longer or shorter." },
        { ID::evolve, "Evolve: slowly moves register and brightness over time so the piece keeps changing." },
        { ID::evolveperiod, "Evolve period: minutes per slow cycle." },
        { ID::simseed, "Simulation seed: each number gives a different, repeatable sky." },
        { ID::simsync, "Simulation follows the DAW timeline, so every render is identical." },
        { ID::poll, "How often live traffic is fetched (seconds). Longer is kinder to the data source." },
        { ID::tension, "Tension macro: left = calm and consonant, right = more shimmer, more inharmonic, more daring intervals." },
        { ID::light, "Light macro: left = dark and warm, right = bright and airy." },
        { ID::motion, "Motion macro: left = still, right = drifting, breathing and evolving." },
        { ID::space, "Space macro: left = dry, right = large and reverberant." },
        { ID::gain, "Output level." },
        { ID::modamt, "Mod wheel amount: how much the mod wheel opens up the brightness." },
        { ID::ftype, "Filter model: Clean = transparent 12 dB filter. Ladder = 24 dB transistor-ladder style filter that saturates and loses bass as resonance rises. Ladder 12 = gentler 12 dB version." },
        { ID::drive, "Drive: pushes the sound into the filter harder for warmth and growl. Works with every filter model." },
        { ID::ftrack, "Key track: lets the cutoff follow the lowest note, so higher chords stay as bright as lower ones." },
        { ID::fmix, "Filter mix: blends the unfiltered and filtered sound." },
        { ID::l1shape, "LFO 1 shape." }, { ID::l2shape, "LFO 2 shape." },
        { ID::l1rate, "LFO 1 speed in cycles per second." }, { ID::l2rate, "LFO 2 speed in cycles per second." },
        { ID::l1sync, "Lock LFO 1 to the host tempo." }, { ID::l2sync, "Lock LFO 2 to the host tempo." },
        { ID::l1div, "LFO 1 length in bars or beats when synced." }, { ID::l2div, "LFO 2 length in bars or beats when synced." },
        { ID::m1src, "Slot 1: what drives the change." }, { ID::m2src, "Slot 2: what drives the change." }, { ID::m3src, "Slot 3: what drives the change." }, { ID::m4src, "Slot 4: what drives the change." }, { ID::m5src, "Slot 5: what drives the change." }, { ID::m6src, "Slot 6: what drives the change." }, { ID::m7src, "Slot 7: what drives the change." }, { ID::m8src, "Slot 8: what drives the change." },
        { ID::m1dst, "Slot 1: which control is moved." }, { ID::m2dst, "Slot 2: which control is moved." }, { ID::m3dst, "Slot 3: which control is moved." }, { ID::m4dst, "Slot 4: which control is moved." }, { ID::m5dst, "Slot 5: which control is moved." }, { ID::m6dst, "Slot 6: which control is moved." }, { ID::m7dst, "Slot 7: which control is moved." }, { ID::m8dst, "Slot 8: which control is moved." },
        { ID::m1amt, "Slot 1 amount. 100% sweeps the control across its whole range (Cutoff: four octaves). Negative flips direction." }, { ID::m2amt, "Slot 2 amount. 100% sweeps the control across its whole range (Cutoff: four octaves). Negative flips direction." },
        { ID::m5amt, "Slot 5 amount. 100% sweeps the control across its whole range. Negative flips direction." }, { ID::m6amt, "Slot 6 amount. 100% sweeps the control across its whole range. Negative flips direction." },
        { ID::m7amt, "Slot 7 amount. 100% sweeps the control across its whole range. Negative flips direction." }, { ID::m8amt, "Slot 8 amount. 100% sweeps the control across its whole range. Negative flips direction." },
        { ID::m3amt, "Slot 3 amount. 100% sweeps the control across its whole range (Cutoff: four octaves). Negative flips direction." }, { ID::m4amt, "Slot 4 amount. 100% sweeps the control across its whole range (Cutoff: four octaves). Negative flips direction." },
        { ID::contrast, "Class contrast: how different the aircraft classes sound. 0 = every aircraft uses the main voice, 100 % = every class has its full character." },
        { ID::swell, "Overhead swell: an aircraft passing close to you blooms in level and brightness, then fades again." },
        { ID::radius, "Radius: how far from your place aircraft are heard (nautical miles). Small = a few close planes. Large = a dense, abstract texture. Scroll over the map to zoom." },
        { ID::simlevel, "Simulation traffic: how busy the simulated sky is (only used when the source is Simulated or the live feed is unavailable)." },
        { ID::hold, "Hold: freezes the chord that is sounding now, so you can play on top." },
        { ID::midiout, "MIDI out: one note per aircraft. In Live (VST3), set another MIDI track's \"MIDI From\" to this track." },
        { ID::midimode, "For hardware and multi-timbral synths: each class gets its own MIDI channel (1-9). Off = all on channel 1. Live cannot filter by channel, so inside Live use Class instead." },
        { ID::midiclass, "Class: All, or one class only. One Aeola set to All is the source; others set to a class copy that class from it, so every track records the same sky." },
        { ID::midisilent, "Mute sound: no audio while MIDI out is on." },
    };
    for (auto& e : t) if (id == e.id) return e.tip;
    return "";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    using PID = ParameterID;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto F = [&](const char* id, const char* name, float lo, float hi, float def, float skewCentre = 0.0f, const char* unit = "") {
        NormalisableRange<float> r(lo, hi);
        if (skewCentre > 0.0f) r.setSkewForCentre(skewCentre);
        String u(unit); bool pct = u.isEmpty();
        auto attrs = AudioParameterFloatAttributes().withLabel(u).withAutomatable(ID::isAutomatable(id))
            .withStringFromValueFunction([pct, u](float v, int) { if (pct) return String(roundToInt(v * 100.0f)) + " %"; return String(v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + u; })
            .withValueFromStringFunction([pct](const String& t) { float v = t.retainCharacters("-0123456789.").getFloatValue(); return pct ? v / 100.0f : v; });
        p.push_back(std::make_unique<AudioParameterFloat>(PID{ id, 1 }, name, r, def, attrs));
    };
    auto C = [&](const char* id, const char* name, StringArray items, int def) { p.push_back(std::make_unique<AudioParameterChoice>(PID{ id, 1 }, name, items, def, AudioParameterChoiceAttributes().withAutomatable(ID::isAutomatable(id)))); };
    auto B = [&](const char* id, const char* name, bool def) { p.push_back(std::make_unique<AudioParameterBool>(PID{ id, 1 }, name, def, AudioParameterBoolAttributes().withAutomatable(ID::isAutomatable(id)))); };
    auto I = [&](const char* id, const char* name, int lo, int hi, int def) { p.push_back(std::make_unique<AudioParameterInt>(PID{ id, 1 }, name, lo, hi, def, AudioParameterIntAttributes().withAutomatable(ID::isAutomatable(id)))); };

    // Sound
    { StringArray names; for (auto& t : timbres()) names.add(t.name); C(ID::srca, "Source A", names, 0); C(ID::srcb, "Source B", names, 2); }
    F(ID::morph, "Morph", 0, 1, 0.0f);
    F(ID::morphmod, "Morph motion", 0, 1, 0.3f);
    F(ID::life, "Life", 0, 1, 0.4f);
    F(ID::air, "Air", 0, 1, 0.3f);
    F(ID::brightness, "Colour", 0, 1, 0.5f);
    F(ID::oddeven, "Odd/Even", -1, 1, 0.0f);
    F(ID::inharm, "Inharmonicity", 0, 1, 0.0f);
    F(ID::beat, "Beat", 0, 6, 0.0f, 0.8f, " Hz");
    F(ID::drift, "Drift", 0, 1, 0.4f);
    F(ID::driftrate, "Drift rate", 0.1f, 4.0f, 1.0f, 1.0f, "x");
    F(ID::attack, "Attack", 0.01f, 20.0f, 2.0f, 2.0f, " s");
    F(ID::release, "Release", 0.01f, 30.0f, 5.0f, 4.0f, " s");
    I(ID::voices, "Voices", 1, 16, 12);
    F(ID::panspread, "Pan spread", 0, 1, 0.5f);
    F(ID::velsens, "Velocity", 0, 1, 0.5f);
    F(ID::breath, "Breath", 0, 1, 0.3f);
    // Tone
    F(ID::cutoff, "Cutoff", 100, 18000, 9000, 1500, " Hz");
    F(ID::reso, "Resonance", 0, 1, 0.1f);
    F(ID::lowcut, "Low cut", 10, 400, 40, 60, " Hz");
    F(ID::warmth, "Warmth", 0, 1, 0.15f);
    // Space
    F(ID::rvmix, "Reverb mix", 0, 1, 0.3f);
    F(ID::rvsize, "Reverb size", 0, 1, 0.7f);
    F(ID::rvdamp, "Reverb damp", 0, 1, 0.5f);
    F(ID::rvpre, "Pre-delay", 0, 250, 20, 40, " ms");
    F(ID::width, "Width", 0, 1.5f, 1.0f);
    B(ID::rvsend, "Reverb to output 2", false);
    // Tune
    C(ID::system, "Tuning system", StringArray(tuningSystemNames(), kNumSystems), 0);
    F(ID::north, "North", 0, 1, 0.0f);
    C(ID::rootmode, "Root mode", { "Lowest note", "Fixed note" }, 0);
    C(ID::fixedroot, "Fixed root", StringArray(noteNames(), 12), 2);
    F(ID::glide, "Glide", 0.01f, 30.0f, 0.6f, 1.0f, " s");
    I(ID::bendrange, "Bend range", 0, 24, 2);
    // Sky
    B(ID::skyon, "Sky", true);
    C(ID::source, "Sky source", { "Auto", "Live", "Cached", "Replay", "Simulated" }, 0);
    { int n; const Airport* a = airports(n); StringArray names; for (int i = 0; i < n; ++i) names.add(String(a[i].icao) + "  " + a[i].name); names.add("Custom place"); C(ID::airport, "Airport", names, 0); }
    I(ID::maxac, "Aircraft", 1, 12, 8);
    C(ID::key, "Key", StringArray(noteNames(), 12), 2);
    { StringArray s; for (int i = 0; i < kNumScales; ++i) s.add(scales()[i].name); C(ID::scale, "Scale", s, 0); }
    F(ID::range, "Range", 0.5f, 4.0f, 2.5f, 2.0f, " oct");
    I(ID::baseoct, "Base octave", 1, 5, 3);
    F(ID::consonance, "Consonance", 0, 1, 0.7f);
    B(ID::anchor, "Anchor", true);
    B(ID::anchorfifth, "Anchor fifth", true);
    I(ID::ghostmin, "Quiet-sky fill", 0, 6, 3);
    F(ID::ghostlevel, "Ghost level", 0, 1, 0.5f);
    F(ID::stretch, "Loop stretch", 0.5f, 2.0f, 1.0f, 1.0f, "x");
    F(ID::evolve, "Evolve", 0, 1, 0.5f);
    F(ID::evolveperiod, "Evolve period", 1, 60, 12, 10, " min");
    I(ID::simseed, "Sim seed", 1, 9999, 1);
    B(ID::simsync, "Sim = transport", false);
    I(ID::poll, "Poll", 30, 600, 120);
    // v1.3: place, mapping and aircraft classes
    F(ID::contrast, "Class contrast", 0, 1, 0.7f);
    B(ID::swell, "Overhead swell", true);
    F(ID::radius, "Radius", 5, 250, 40, 40, " nm");
    F(ID::simlevel, "Sim traffic", 0.2f, 2.0f, 1.0f, 1.0f, "x");
    {   StringArray tn; tn.add("Global"); for (auto& t : timbres()) tn.add(t.name);
        auto idxOf = [](const char* nm) { int i = 0; for (auto& t : timbres()) { if (!std::strcmp(t.name, nm)) return i + 1; ++i; } return 0; };
        struct D { const char* tim; float db; int oct; float att, chop; };
        const D d[9] = { { "Flute", -3, 1, 0.6f, 0.0f },       // Light / GA
                         { "Reed", -2, 0, 0.8f, 0.0f },        // Small
                         { nullptr, 0, 0, 1.0f, 0.0f },        // Narrowbody: the plugin's own voice
                         { "Choir Ah", 1, -1, 2.5f, 0.0f },    // Heavy
                         { "Reed", 0, 0, 0.5f, 0.55f },        // Helicopter
                         { "Bowl", 2, -1, 3.0f, 0.0f },        // Freight
                         { "Glass", -1, 1, 0.5f, 0.0f },       // Business jet
                         { "Organ", 0, 0, 0.8f, 0.45f },       // Turboprop
                         { "Grit", 2, 0, 0.4f, 0.0f } };       // Military
        for (int c = 0; c < 9; ++c)
        {
            String nm = String(className(c)) + " ";
            C(ID::classId(c, 0), (nm + "timbre").toRawUTF8(), tn, d[c].tim ? idxOf(d[c].tim) : 0);
            F(ID::classId(c, 1), (nm + "gain").toRawUTF8(), -12, 6, d[c].db, 0.0f, " dB");
            I(ID::classId(c, 2), (nm + "octave").toRawUTF8(), -2, 2, d[c].oct);
            F(ID::classId(c, 3), (nm + "attack").toRawUTF8(), 0.25f, 4.0f, d[c].att, 1.0f, "x");
            F(ID::classId(c, 4), (nm + "chop").toRawUTF8(), 0, 1, d[c].chop);
            B(ID::classId(c, 5), (nm + "solo").toRawUTF8(), false);
            B(ID::classId(c, 6), (nm + "mute").toRawUTF8(), false);
        }
    }
    // Macros
    F(ID::tension, "Tension", 0, 1, 0.5f);
    F(ID::light, "Light", 0, 1, 0.5f);
    F(ID::motion, "Motion", 0, 1, 0.5f);
    F(ID::space, "Space", 0, 1, 0.5f);
    // Output
    F(ID::gain, "Output", 0, 1, 0.5f);
    F(ID::modamt, "Mod wheel", 0, 1, 0.6f);
    B(ID::hold, "Hold", false);
    B(ID::midiout, "MIDI out", false);
    C(ID::midimode, "MIDI out channels", { "Per class", "Channel 1" }, 0);
    B(ID::midisilent, "MIDI out mutes sound", false);
    { StringArray cn; cn.add("All classes"); for (int c = 0; c < kNumClasses; ++c) cn.add(className(c)); C(ID::midiclass, "MIDI out class", cn, 0); }
    // Filter character
    C(ID::ftype, "Filter model", { "Clean", "Ladder", "Ladder 12" }, 0);
    F(ID::drive, "Drive", 0, 24, 0, 6, " dB");
    F(ID::ftrack, "Key track", 0, 1, 0.0f);
    F(ID::fmix, "Filter mix", 0, 1, 1.0f);
    // Modulation
    { StringArray sh(lfoShapeNames(), 6), dv(lfoDivNames(), 7);
      C(ID::l1shape, "LFO 1 shape", sh, 0); F(ID::l1rate, "LFO 1 rate", 0.01f, 20.0f, 0.2f, 1.0f, " Hz"); B(ID::l1sync, "LFO 1 sync", false); C(ID::l1div, "LFO 1 length", dv, 4);
      C(ID::l2shape, "LFO 2 shape", sh, 1); F(ID::l2rate, "LFO 2 rate", 0.01f, 20.0f, 0.07f, 1.0f, " Hz"); B(ID::l2sync, "LFO 2 sync", false); C(ID::l2div, "LFO 2 length", dv, 2); }
    for (int i = 0; i < kModSlots; ++i)
    {
        String n(i + 1);
        C(ID::modSlotId(i, 0), ("Mod " + n + " source").toRawUTF8(), StringArray(modSrcNames(), kNumSrc), 0);
        C(ID::modSlotId(i, 1), ("Mod " + n + " target").toRawUTF8(), StringArray(modDstNames(), kNumDst), 0);
        { NormalisableRange<float> r(-1.0f, 1.0f); auto attrs = AudioParameterFloatAttributes().withAutomatable(ID::isAutomatable(ID::modSlotId(i, 2)))
            .withStringFromValueFunction([](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; })
            .withValueFromStringFunction([](const String& t) { return t.retainCharacters("-0123456789.").getFloatValue() / 100.0f; });
          p.push_back(std::make_unique<AudioParameterFloat>(PID{ ID::modSlotId(i, 2), 1 }, ("Mod " + n + " amount").toRawUTF8(), r, 0.0f, attrs)); }
    }
    // 1.12 mixer: a fader and a pan per class (kept by presets and A/B: a mix is yours, not part of a sound)
    for (int c = 0; c < kNumClasses; ++c)
    {
        String nm = String(className(c)) + " ";
        { NormalisableRange<float> r(-60.0f, 6.0f); r.setSkewForCentre(-12.0f);
          auto attrs = AudioParameterFloatAttributes().withLabel("dB").withAutomatable(ID::isAutomatable(ID::classMixId(c, 0)))
              .withStringFromValueFunction([](float v, int) { return v <= -59.9f ? String("-inf dB") : String(v, 1) + " dB"; })
              .withValueFromStringFunction([](const String& t) { return t.containsIgnoreCase("inf") ? -60.0f : t.retainCharacters("-0123456789.").getFloatValue(); });
          p.push_back(std::make_unique<AudioParameterFloat>(PID{ ID::classMixId(c, 0), 1 }, (nm + "fader").toRawUTF8(), r, 0.0f, attrs)); }
        { NormalisableRange<float> r(-1.0f, 1.0f);
          auto attrs = AudioParameterFloatAttributes().withAutomatable(ID::isAutomatable(ID::classMixId(c, 1)))
              .withStringFromValueFunction([](float v, int) { const int k = roundToInt(v * 100.0f); return k == 0 ? String("C") : (k < 0 ? "L " + String(-k) : "R " + String(k)); })
              .withValueFromStringFunction([](const String& t) { const float v = t.retainCharacters("0123456789.").getFloatValue() / 100.0f; return t.trim().startsWithIgnoreCase("L") ? -v : v; });
          p.push_back(std::make_unique<AudioParameterFloat>(PID{ ID::classMixId(c, 1), 1 }, (nm + "pan").toRawUTF8(), r, 0.0f, attrs)); }
    }
    // 1.13.1: Aeola's own Bypass, not automatable. Without it the VST3 wrapper adds an automatable one, and Ableton Live then sees
    // more than 64 automatable parameters and leaves its list empty.
    p.push_back(std::make_unique<AudioParameterBool>(PID{ "bypass", 1 }, "Bypass", false, AudioParameterBoolAttributes().withAutomatable(false)));
    return { p.begin(), p.end() };
}
} // namespace aeola
