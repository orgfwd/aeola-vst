// Aeola — factory presets. Each has a purpose label so the browser tells you what it is for.
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include "Params.h"

namespace aeola
{
struct PresetValue { const char* id; float v; };
struct Preset { const char* name; const char* purpose; std::vector<PresetValue> values; };

// choice indices: key 0=C..11=B (D=2, A=9, F=5, G=7); scale 0 Dorian 1 Minor 2 Major 3 Lydian 4 Mixolydian 5 Phrygian
// 6 Pent.minor 7 Pent.major 8 Hirajoshi 9 Whole tone 10 Open fifths 11 Chromatic; system 0 Just 1 Pyth 2 Harmonic 3 Meantone;
// airport 0 ESSA 1 EDDK 2 EGLL 3 EHAM 4 EDDF 5 EKCH 6 ENGM 7 EFHK 8 KJFK 9 KLAX 10 RJTT 11 YSSY; source 0 Auto 4 Simulated
inline const std::vector<Preset>& factoryPresets()
{
    using namespace ID;
    static const std::vector<Preset> p = {
        { "Arlanda Dawn", "Ambient bed", { {srca,0},{srcb,10},{morph,0.25f},{airport,0},{key,2},{scale,0},{north,0.6f},{beat,0.6f},{brightness,0.45f},{attack,6},{release,10},{rvmix,0.4f},{evolve,0.6f},{consonance,0.8f} } },
        { "Cargo Night", "Score cue", { {srca,8},{srcb,5},{morph,0.4f},{life,0.5f},{airport,0},{key,9},{scale,1},{baseoct,2},{brightness,0.3f},{lowcut,60},{cutoff,3500},{rvmix,0.45f},{tension,0.35f},{north,0.4f},{attack,8} } },
        { "Pure Fifths", "Meditation", { {srca,2},{srcb,0},{morph,0.2f},{scale,10},{key,2},{system,1},{north,1},{beat,0},{consonance,1},{brightness,0.35f},{rvmix,0.5f},{evolve,0.3f},{ghostmin,3} } },
        { "Glass Choir", "Pad for pop and film", { {srca,0},{srcb,4},{morph,0.5f},{morphmod,0.5f},{scale,7},{key,0},{inharm,0.25f},{brightness,0.7f},{beat,1.2f},{rvsize,0.9f},{rvmix,0.5f},{north,0.5f},{attack,4} } },
        { "Slow Tide", "Sleep and focus", { {srca,12},{srcb,13},{morph,0.35f},{air,0.5f},{attack,12},{release,20},{glide,8},{drift,0.2f},{brightness,0.25f},{cutoff,2200},{rvmix,0.5f},{maxac,4},{ghostmin,4},{evolve,0.8f},{evolveperiod,30} } },
        { "Beating Lab", "Hear what tuning does", { {srca,0},{srcb,1},{morph,0},{life,0.1f},{scale,6},{key,9},{north,0},{beat,2.5f},{brightness,0.5f},{maxac,3},{ghostmin,0},{anchor,1},{consonance,0.9f},{rvmix,0.2f} } },
        { "Heathrow Hum", "Tension riser", { {srca,11},{srcb,8},{morph,0.5f},{morphmod,0.6f},{airport,2},{scale,5},{key,4},{consonance,0.35f},{tension,0.8f},{beat,3},{brightness,0.55f},{inharm,0.15f},{attack,10},{rvmix,0.4f},{maxac,10} } },
        { "Cologne Cathedral", "Big reverb hall", { {srca,1},{srcb,2},{morph,0.4f},{airport,1},{scale,3},{key,7},{rvmix,0.65f},{rvsize,0.97f},{rvpre,60},{brightness,0.5f},{north,0.7f},{release,14} } },
        { "Kyoto Rain", "Delicate texture", { {srca,10},{srcb,0},{morph,0.3f},{life,0.6f},{airport,10},{scale,8},{key,9},{brightness,0.6f},{beat,0.8f},{oddeven,0.4f},{maxac,6},{rvmix,0.4f},{cutoff,6500} } },
        { "Quiet Harbour", "Focus and study", { {srca,7},{srcb,12},{morph,0.3f},{air,0.5f},{airport,5},{scale,7},{key,7},{maxac,4},{ghostmin,4},{brightness,0.3f},{cutoff,4000},{rvmix,0.35f},{consonance,0.95f} } },
        { "Northern Lights", "Cinematic opener", { {srca,4},{srcb,0},{morph,0.5f},{morphmod,0.5f},{airport,6},{scale,3},{key,5},{evolve,0.9f},{evolveperiod,20},{rvmix,0.55f},{brightness,0.55f},{beat,1.0f},{north,0.8f} } },
        { "Helsinki Ice", "Cold pad", { {srca,9},{srcb,0},{morph,0.4f},{airport,7},{scale,9},{key,2},{consonance,0.6f},{brightness,0.75f},{inharm,0.3f},{cutoff,12000},{beat,1.5f},{rvmix,0.45f} } },
        { "JFK Rush Hour", "Busy, layered texture", { {srca,12},{srcb,6},{morph,0.5f},{morphmod,0.7f},{airport,8},{maxac,12},{voices,16},{breath,0.6f},{scale,0},{key,2},{consonance,0.75f},{drift,0.5f},{rvmix,0.4f} } },
        { "Pacific Slow", "Wide, calm bed", { {srca,3},{srcb,12},{morph,0.4f},{airport,9},{scale,2},{key,4},{attack,9},{release,16},{width,1.3f},{rvmix,0.45f},{brightness,0.4f} } },
        { "Sydney Morning", "Bright ambient", { {srca,5},{srcb,0},{morph,0.35f},{airport,11},{scale,7},{key,9},{brightness,0.65f},{beat,0.7f},{rvmix,0.35f},{evolve,0.5f} } },
        { "Piano vs Pure", "Ear training", { {srca,0},{srcb,0},{morph,0},{life,0},{morphmod,0},{scale,10},{key,0},{north,0.5f},{beat,0},{glide,3},{maxac,2},{ghostmin,2},{anchor,0},{rvmix,0.15f},{consonance,1} } },
        { "Overtone Garden", "Harmonic-series colour", { {srca,6},{srcb,8},{morph,0.4f},{system,2},{north,1},{oddeven,0.5f},{brightness,0.6f},{scale,4},{key,7},{beat,0.4f},{rvmix,0.4f} } },
        { "Meantone Chapel", "Warm old-world pad", { {srca,1},{srcb,5},{morph,0.3f},{system,3},{north,1},{scale,2},{key,0},{rvmix,0.5f},{brightness,0.4f},{warmth,0.35f} } },
        { "Dark Matter", "Sub drone", { {srca,8},{srcb,11},{morph,0.4f},{life,0.5f},{baseoct,1},{lowcut,25},{cutoff,900},{brightness,0.3f},{anchor,1},{scale,1},{key,5},{rvmix,0.3f},{attack,10},{maxac,4},{range,1.5f} } },
        { "Shimmer Sky", "High, airy layer", { {srca,0},{srcb,9},{morph,0.5f},{morphmod,0.5f},{baseoct,4},{beat,2},{brightness,0.8f},{lowcut,200},{rvmix,0.55f},{rvsize,0.9f},{scale,7},{key,2},{range,2} } },
        { "Simulated Calm", "Works fully offline", { {srca,0},{srcb,12},{morph,0.4f},{source,4},{simseed,7},{simsync,1},{scale,6},{key,9},{brightness,0.4f},{rvmix,0.4f},{evolve,0.5f} } },
        { "Keys Companion", "Play over it", { {srca,12},{srcb,1},{morph,0.3f},{maxac,3},{ghostmin,0},{brightness,0.35f},{rvmix,0.25f},{gain,0.35f},{consonance,1},{scale,6},{key,9} } },
        { "Dorian Lullaby", "Soft melody bed", { {srca,3},{srcb,5},{morph,0.3f},{scale,0},{key,3},{maxac,5},{consonance,0.9f},{beat,0.3f},{brightness,0.4f},{attack,5},{rvmix,0.45f} } },
        { "Hirajoshi Dusk", "Evening mood", { {srca,9},{srcb,2},{morph,0.3f},{scale,8},{key,2},{beat,0.9f},{brightness,0.5f},{oddeven,0.3f},{rvmix,0.5f},{evolve,0.7f},{evolveperiod,25} } },
        { "Whole-Tone Float", "Dreamy and unresolved", { {srca,0},{srcb,13},{morph,0.4f},{air,0.4f},{scale,9},{key,0},{consonance,0.55f},{brightness,0.6f},{beat,1.4f},{rvmix,0.5f} } },
        { "Frozen Chord", "Hold and play over", { {srca,2},{srcb,5},{morph,0.4f},{hold,0},{scale,0},{maxac,6},{consonance,0.85f},{brightness,0.5f},{release,12} } },
        { "Tension Rise", "Build-up (automate Tension)", { {srca,7},{srcb,8},{morph,0.2f},{tension,0.2f},{scale,5},{key,4},{beat,0.5f},{rvmix,0.4f},{consonance,0.8f},{attack,6} } },
        { "Open Window", "Light, natural sound", { {srca,0},{srcb,6},{morph,0.2f},{brightness,0.55f},{scale,2},{key,7},{rvmix,0.2f},{warmth,0.1f},{beat,0.2f},{cutoff,14000} } },
        { "Tape Loops", "Eno-style unequal loops", { {srca,5},{srcb,3},{morph,0.5f},{morphmod,0.5f},{breath,0.85f},{stretch,1.4f},{scale,7},{key,0},{maxac,8},{drift,0.3f},{brightness,0.45f},{rvmix,0.4f},{north,0.5f} } },
        { "Long Shadow", "Slow film underscore", { {srca,8},{srcb,2},{morph,0.3f},{baseoct,2},{scale,1},{key,2},{attack,14},{release,25},{glide,12},{evolve,1},{evolveperiod,40},{rvmix,0.55f},{cutoff,3000} } },
        { "Vowel Drift", "Voices without words", { {srca,2},{srcb,4},{morph,0.5f},{morphmod,0.8f},{life,0.6f},{scale,7},{key,0},{rvmix,0.5f},{attack,5} } },
        { "Singing Bowls", "Meditative metal", { {srca,10},{srcb,9},{morph,0.2f},{life,0.6f},{scale,10},{key,9},{system,1},{north,1},{rvmix,0.55f},{beat,0} } },
        { "Bellfield", "Slow bell wash", { {srca,9},{srcb,0},{morph,0.3f},{attack,3},{release,16},{scale,7},{key,7},{rvmix,0.6f},{rvsize,0.95f},{maxac,6} } },
        { "Horn Section", "Warm brass pad", { {srca,8},{srcb,5},{morph,0.35f},{scale,3},{key,5},{brightness,0.45f},{rvmix,0.4f},{attack,4} } },
        { "Air Choir", "Breathy voices", { {srca,13},{srcb,2},{morph,0.5f},{air,0.7f},{morphmod,0.6f},{scale,0},{key,9},{rvmix,0.55f} } },
        { "Rust Drone", "Gritty bed, filter breathes", { {srca,17},{srcb,14},{morph,0.3f},{ftype,1},{cutoff,1800},{reso,0.35f},{drive,9},{warmth,0.3f},{scale,1},{key,9},{rvmix,0.35f},{m1src,2},{m1dst,8},{m1amt,0.35f},{l2rate,0.06f} } },
        { "Slow Sweep", "Filter opens and closes over minutes", { {srca,14},{srcb,12},{morph,0.4f},{ftype,1},{cutoff,700},{reso,0.45f},{l1shape,1},{l1rate,0.03f},{m1src,1},{m1dst,8},{m1amt,0.55f},{scale,0},{key,2},{rvmix,0.4f} } },
        { "Radar Breathing", "More aircraft open the filter", { {srca,12},{srcb,0},{morph,0.3f},{ftype,1},{cutoff,500},{reso,0.25f},{m1src,4},{m1dst,8},{m1amt,0.6f},{m2src,5},{m2dst,1},{m2amt,0.3f},{scale,6},{key,9},{rvmix,0.4f} } },
        { "Tempo Pulse", "Swell locked to the host tempo", { {srca,8},{srcb,12},{morph,0.35f},{ftype,2},{cutoff,900},{reso,0.3f},{l1sync,1},{l1div,2},{l1shape,0},{m1src,1},{m1dst,8},{m1amt,0.4f},{m2src,1},{m2dst,14},{m2amt,0.25f},{scale,7},{key,7} } },
        { "Growl", "Saw drone with drive; mod wheel opens it", { {srca,14},{srcb,16},{morph,0.4f},{ftype,1},{drive,14},{cutoff,1200},{reso,0.4f},{m1src,8},{m1dst,8},{m1amt,0.8f},{m2src,8},{m2dst,10},{m2amt,0.5f},{scale,1},{key,4},{rvmix,0.25f} } },
        { "Pulse Garden", "Hollow pulses that wander", { {srca,15},{srcb,6},{morph,0.5f},{life,0.6f},{m1src,3},{m1dst,2},{m1amt,0.5f},{l2shape,4},{l2rate,0.15f},{m2src,2},{m2dst,1},{m2amt,0.3f},{scale,6},{key,0},{rvmix,0.45f} } },
        { "Bell Weather", "Inharmonic bells follow the traffic", { {srca,9},{srcb,10},{morph,0.4f},{release,12},{consonance,0.8f},{m1src,6},{m1dst,13},{m1amt,0.3f},{m2src,7},{m2dst,6},{m2amt,0.3f},{scale,7},{key,2},{rvmix,0.4f} } },
        { "Wind Tunnel", "Dark air with resonance sweeps", { {srca,13},{srcb,17},{morph,0.5f},{air,0.8f},{ftype,1},{cutoff,400},{reso,0.6f},{drive,6},{l1shape,4},{l1rate,0.12f},{m1src,1},{m1dst,8},{m1amt,0.7f},{m2src,3},{m2dst,11},{m2amt,0.5f},{rvmix,0.5f} } },
        { "Mono Safe Pad", "Fits dense mixes", { {srca,12},{srcb,5},{morph,0.3f},{width,0.5f},{lowcut,120},{rvmix,0.2f},{panspread,0.2f},{brightness,0.45f},{scale,7},{key,2} } },
    };
    return p;
}

// A preset is a sound. Where you listen (place, radius), the Sky switch, the output level and your per-class voices and solo / mute
// belong to you and survive a preset change. Source is reset to Auto unless the preset asks for something else.
inline bool keptByPresets(const juce::String& id)
{
    if (id == ID::skyon || id == ID::gain || id == ID::airport || id == ID::radius) return true;
    if (id.startsWith("midi")) return true;                                                        // MIDI out routing is part of your setup, not of a sound
    return id.length() >= 3 && id[0] == 'c' && juce::CharacterFunctions::isDigit(id[1]);          // c<class><what>: the class voices
}
inline void applyPreset(juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    for (auto* prm : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm))
        {
            juce::String id = r->paramID;
            if (keptByPresets(id)) continue;
            r->setValueNotifyingHost(r->getDefaultValue());
        }
    for (auto& v : preset.values)
        if (auto* r = apvts.getParameter(v.id))
            if (!keptByPresets(r->paramID) || juce::String(v.id) == ID::source)
                r->setValueNotifyingHost(r->convertTo0to1(v.v));
}
} // namespace aeola
