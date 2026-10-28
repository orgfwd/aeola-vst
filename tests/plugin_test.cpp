// Headless tests that exercise the real plugin code (processor, FX chain, presets, state, sky, editor snapshot).
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <chrono>

using namespace aeola;
static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++fails; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setP(AeolaDroneProcessor& p, const char* id, float actual)
{ if (auto* r = p.apvts.getParameter(id)) r->setValueNotifyingHost(r->convertTo0to1(actual)); }

struct Rendered { std::vector<float> L, R; bool nan = false; float peak = 0; };
static Rendered render(AeolaDroneProcessor& p, double seconds, int block = 512, juce::MidiBuffer* firstMidi = nullptr, int channels = 2)
{
    Rendered r; int total = (int) (seconds * 44100);
    for (int pos = 0; pos < total; pos += block)
    {
        int n = std::min(block, total - pos);
        juce::AudioBuffer<float> buf(channels, n); buf.clear();
        juce::MidiBuffer m; if (pos == 0 && firstMidi) m = *firstMidi;
        p.processBlock(buf, m);
        for (int i = 0; i < n; ++i)
        {
            float l = buf.getSample(0, i), rr = buf.getSample(1, i);
            if (!std::isfinite(l) || !std::isfinite(rr)) r.nan = true;
            r.peak = std::max(r.peak, std::max(std::fabs(l), std::fabs(rr)));
            r.L.push_back(l); r.R.push_back(rr);
        }
    }
    return r;
}
static double rms(const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b && i < x.size(); ++i) s += x[i] * x[i]; return std::sqrt(s / std::max<size_t>(1, b - a)); }

// Golden regression: v1.1 renders of three presets (harmonic timbres only). `--golden-write` regenerates them.
static std::vector<float> goldenRender(const char* presetName)
{
    AeolaDroneProcessor p; p.prepareToPlay(44100, 512);
    int idx = -1; for (int i = 0; i < (int) factoryPresets().size(); ++i) if (juce::String(factoryPresets()[(size_t) i].name) == presetName) idx = i;
    p.loadFactoryPreset(idx); setP(p, ID::source, 4); setP(p, ID::simsync, 1); setP(p, ID::simseed, 77);
    auto r = render(p, 4.0); std::vector<float> out;
    for (size_t i = 2 * 44100; i < r.L.size(); ++i) { out.push_back(r.L[i]); out.push_back(r.R[i]); }
    return out;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File src(AEOLA_SRC_DIR);
    const char* goldenNames[] = { "Arlanda Dawn", "Cologne Cathedral", "Pure Fifths" };
    if (argc > 1 && juce::String(argv[1]) == "--golden-write")
    {
        src.getChildFile("tests/golden").createDirectory();
        for (auto* n : goldenNames) { auto v = goldenRender(n); auto f = src.getChildFile("tests/golden/" + juce::String(n).replaceCharacter(' ', '_') + ".f32"); f.deleteFile(); f.replaceWithData(v.data(), v.size() * sizeof(float)); printf("wrote %s (%zu floats)\n", f.getFullPathName().toRawUTF8(), v.size()); }
        return 0;
    }

    printf("[0] regression: filter Clean / drive 0 / empty mod matrix renders like v1.1\n");
    for (auto* n : goldenNames)
    {
        auto f = src.getChildFile("tests/golden/" + juce::String(n).replaceCharacter(' ', '_') + ".f32");
        if (!f.existsAsFile()) { printf("  skip: no golden for %s\n", n); continue; }
        juce::MemoryBlock mb; f.loadFileAsData(mb); auto cur = goldenRender(n);
        const float* g = (const float*) mb.getData(); size_t ng = mb.getSize() / sizeof(float);
        double worst = 0; for (size_t i = 0; i < std::min(ng, cur.size()); ++i) worst = std::max(worst, (double) std::fabs(g[i] - cur[i]));
        CHECK(ng == cur.size() && worst < 1e-6, "%s: max deviation from v1.1 reference %.2e over %zu samples", n, worst, ng / 2);
    }

    printf("[1] sky: simulated source produces sound, stays under the limiter, UI snapshot is filled\n");
    {
        AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::attack, 1.0f);
        p.prepareToPlay(44100, 512);
        auto r = render(p, 25.0);
        UiSnapshot u; bool have = p.getUi(u);
        CHECK(!r.nan, "no NaN/Inf in output");
        CHECK(r.peak <= 1.0001f, "peak %.3f <= 1.0 (limiter)", r.peak);
        CHECK(rms(r.L, 44100 * 15, 44100 * 25) > 0.002, "audible output: rms %.4f", rms(r.L, 44100 * 15, 44100 * 25));
        CHECK(have && u.source == 4 && u.nVoices >= 3, "ui: source=%d voices=%d aircraft=%d accepted=%d ghosts=%d", u.source, u.nVoices, u.nAc, u.accepted, u.ghosts);
        printf("  info: %s | %s\n", p.statusLine().toRawUTF8(), p.hintLine().toRawUTF8());
    }

    printf("[2] MIDI notes, sustain, hold and pitch bend\n");
    {
        AeolaDroneProcessor p; setP(p, ID::skyon, 0); setP(p, ID::attack, 0.1f); setP(p, ID::release, 1.0f); p.prepareToPlay(44100, 512);
        juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8) 100), 10); m.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 10);
        auto r = render(p, 2.0, 512, &m);
        CHECK(rms(r.L, 44100, 88200) > 0.01, "notes sound: rms %.4f", rms(r.L, 44100, 88200));
        juce::MidiBuffer off; off.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0); off.addEvent(juce::MidiMessage::noteOff(1, 57), 5); off.addEvent(juce::MidiMessage::noteOff(1, 64), 5);
        auto r2 = render(p, 3.0, 512, &off);
        CHECK(rms(r2.L, 44100, 3 * 44100) > 0.005, "sustain pedal keeps notes ringing: rms %.4f", rms(r2.L, 44100, 3 * 44100));
        juce::MidiBuffer up; up.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
        render(p, 0.1, 512, &up);
        auto r3 = render(p, 8.0);
        CHECK(rms(r3.L, 6 * 44100, 8 * 44100) < 1e-4, "released notes fade to silence: rms %.6f", rms(r3.L, 6 * 44100, 8 * 44100));
    }

    printf("[3] determinism: simulation synced to transport gives identical renders\n");
    {
        auto run = [&]() { AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); setP(p, ID::simseed, 123); p.prepareToPlay(44100, 512); return render(p, 12.0); };
        auto a = run(), b = run(); bool same = a.L.size() == b.L.size(); for (size_t i = 0; same && i < a.L.size(); ++i) same = a.L[i] == b.L[i] && a.R[i] == b.R[i];
        CHECK(same, "two independent instances render bit-identical audio (%zu samples)", a.L.size());
    }

    printf("[4] abrupt parameter changes do not click\n");
    {
        AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); p.prepareToPlay(44100, 512);
        render(p, 10.0);
        setP(p, ID::gain, 0.9f); setP(p, ID::brightness, 1.0f); setP(p, ID::cutoff, 400.0f); setP(p, ID::rvmix, 1.0f); setP(p, ID::north, 1.0f); setP(p, ID::warmth, 1.0f); setP(p, ID::width, 0.0f);
        auto r = render(p, 2.0);
        double step = 0; for (size_t i = 1; i < r.L.size(); ++i) step = std::max(step, (double) std::fabs(r.L[i] - r.L[i - 1]));
        CHECK(step < 0.2, "max sample-to-sample step after abrupt changes = %.4f (peak %.3f)", step, r.peak);
        CHECK(r.peak <= 1.0001f, "still limited: peak %.3f", r.peak);
    }

    printf("[5] every factory preset renders cleanly\n");
    {
        int bad = 0, silent = 0; double worstPeak = 0;
        for (int i = 0; i < (int) factoryPresets().size(); ++i)
        {
            AeolaDroneProcessor p; p.prepareToPlay(44100, 512); p.loadFactoryPreset(i); setP(p, ID::source, 4); setP(p, ID::simsync, 1);
            if (i == 22) setP(p, ID::source, 4);
            auto r = render(p, 14.0);
            if (r.nan || r.peak > 1.0001f) { ++bad; printf("    preset %d '%s' peak %.3f nan=%d\n", i, factoryPresets()[(size_t) i].name, r.peak, (int) r.nan); }
            if (rms(r.L, 44100 * 10, 44100 * 14) < 5e-4) { ++silent; printf("    preset %d '%s' is nearly silent (rms %.5f)\n", i, factoryPresets()[(size_t) i].name, rms(r.L, 44100 * 10, 44100 * 14)); }
            worstPeak = std::max(worstPeak, (double) r.peak);
        }
        CHECK(bad == 0, "%zu presets: %d with NaN or peak > 1 (worst peak %.3f)", factoryPresets().size(), bad, worstPeak);
        CHECK(silent == 0, "presets that are silent after 10 s: %d", silent);
    }

    printf("[6] sky parsing of a real adsb.lol payload, mapping and musical guard on real aircraft\n");
    {
        auto sample = src.getChildFile("tests/adsb_sample.json");
        if (!sample.existsAsFile()) printf("  skip: no sample\n");
        else
        {
            Frame f; bool ok = SkyFeed::parseResponse(sample.loadFileAsString(), f, placeFromAirport(0, 40));
            CHECK(ok && f.count > 0, "parsed %d airborne aircraft from the sample (first: %s %s alt %.0f)", f.count, f.ac[0].callsign, f.ac[0].type, f.ac[0].altFt);
            SkyMapper m; SkySettings cfg; cfg.apLat = 59.6519; cfg.apLon = 17.9186; MapResult r; m.map(f, cfg, 1.0e9, r);
            int harsh = 0; for (int a = 0; a < r.n; ++a) for (int b = a + 1; b < r.n; ++b) if (guardPenalty(r.v[a].note, r.v[b].note) > 0.1 + 0.3 * 0.95 + 1e-9) ++harsh;
            CHECK(r.n >= 3 && harsh == 0, "real sky -> %d voices (accepted %d, shifted %d, rejected %d), harsh pairs %d", r.n, r.accepted, r.shifted, r.rejected, harsh);
            CHECK(!SkyFeed::parseResponse("not json", f, placeFromAirport(0, 40)) && !SkyFeed::parseResponse("{\"ac\":5}", f, placeFromAirport(0, 40)), "garbage responses are rejected");
        }
    }

    printf("[7] state round trip incl. frozen sky; MIDI export\n");
    {
        AeolaDroneProcessor p; p.prepareToPlay(44100, 512);
        setP(p, ID::north, 0.77f); setP(p, ID::scale, 6); setP(p, ID::key, 9); setP(p, ID::glide, 3.3f);
        auto sample = src.getChildFile("tests/adsb_sample.json"); Frame f; SkyFeed::parseResponse(sample.loadFileAsString(), f, placeFromAirport(0, 40)); f.epoch = 1.7928e9;
        if (p.getFeed().onFrame) for (int i = 0; i < 4; ++i) { f.epoch += 120; p.getFeed().onFrame(f, 0); }
        p.freezeSkyForRender();
        setP(p, ID::source, 3);
        auto r = render(p, 30.0);
        UiSnapshot u; p.getUi(u);
        CHECK(u.source == 3 && u.nAc > 0, "Replay source plays frozen traffic: source=%d aircraft=%d", u.source, u.nAc);
        juce::MemoryBlock mb; p.getStateInformation(mb);
        AeolaDroneProcessor q; q.setStateInformation(mb.getData(), (int) mb.getSize());
        CHECK(std::fabs(q.apvts.getRawParameterValue(ID::north)->load() - 0.77f) < 1e-3f && (int) q.apvts.getRawParameterValue(ID::scale)->load() == 6 && std::fabs(q.apvts.getRawParameterValue(ID::glide)->load() - 3.3f) < 1e-2f, "parameters restored");
        q.prepareToPlay(44100, 512); auto r2 = render(q, 10.0); UiSnapshot u2; q.getUi(u2);
        CHECK(u2.source == 3 && u2.nAc > 0, "frozen sky restored from project state: source=%d aircraft=%d (requested %d)", u2.source, u2.nAc, u2.requested);
        p.pollMessageThreadTasks();
        auto mid = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("aeola_test.mid");
        bool ok = p.exportMidi(mid); juce::MidiFile mf; juce::FileInputStream in(mid); bool rd = ok && mf.readFrom(in);
        CHECK(ok && rd && mf.getNumTracks() > 0 && mf.getTrack(0)->getNumEvents() > 4, "MIDI export: %d events", rd ? mf.getTrack(0)->getNumEvents() : 0);
    }

    printf("[8] reverb-only output bus and mono-safety\n");
    {
        AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::rvsend, 1); setP(p, ID::rvmix, 0.8f); setP(p, ID::attack, 1.0f);
        juce::AudioProcessor::BusesLayout lay; lay.outputBuses.add(juce::AudioChannelSet::stereo()); lay.outputBuses.add(juce::AudioChannelSet::stereo());
        for (int c = 0; c < kNumClasses; ++c) lay.outputBuses.add(juce::AudioChannelSet::disabled());      // 1.12 class outputs, off
        bool setOk = p.setBusesLayout(lay);
        p.prepareToPlay(44100, 512);
        double auxRms = 0; int total = 44100 * 12;
        for (int pos = 0; pos < total; pos += 512)
        {
            juce::AudioBuffer<float> buf(4, 512); buf.clear(); juce::MidiBuffer m; p.processBlock(buf, m);
            if (pos > 44100 * 6) for (int i = 0; i < 512; ++i) auxRms += buf.getSample(2, i) * buf.getSample(2, i);
        }
        CHECK(setOk, "second (reverb) output bus can be enabled");
        CHECK(auxRms > 1e-4, "reverb-only bus carries signal (energy %.5f)", auxRms);
        UiSnapshot u; p.getUi(u); CHECK(u.corr > -0.2f, "stereo correlation %.2f (not out of phase)", u.corr);
    }

    printf("[9] CPU: 16 voices + Sky + full FX chain at 44.1 kHz / 512\n");
    {
        AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::voices, 16); setP(p, ID::maxac, 12); setP(p, ID::beat, 1.5f); setP(p, ID::rvmix, 0.5f); setP(p, ID::warmth, 0.5f);
        p.prepareToPlay(44100, 512);
        juce::MidiBuffer m; for (int n = 36; n < 60; n += 3) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
        render(p, 10.0, 512, &m);
        auto t0 = std::chrono::steady_clock::now(); render(p, 30.0);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("  info: %.1f%% of one core (includes the test's own sample copying)\n", 100.0 * secs / 30.0);
        CHECK(secs / 30.0 < 0.25, "CPU below 25%% of one core");
    }

    printf("[11] modulation inside the plugin\n");
    {
        auto notes = []() { juce::MidiBuffer m; for (int n : { 48 }) m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0); return m; };
        auto windowsRatio = [&](AeolaDroneProcessor& p) {
            auto m = notes(); render(p, 4.0, 512, &m); auto r = render(p, 10.0);
            double mn = 1e9, mx = 0; for (int w = 0; w < 40; ++w) { double v = rms(r.L, (size_t) w * 11025, (size_t) (w + 1) * 11025); mn = std::min(mn, v); mx = std::max(mx, v); } return mx / std::max(mn, 1e-9); };
        auto base = [](AeolaDroneProcessor& p) { setP(p, ID::skyon, 0); setP(p, ID::srca, 6); setP(p, ID::srcb, 6); setP(p, ID::brightness, 1.0f); setP(p, ID::ftype, 1); setP(p, ID::cutoff, 1500); setP(p, ID::reso, 0.3f); setP(p, ID::attack, 0.05f); setP(p, ID::life, 0); setP(p, ID::drift, 0); setP(p, ID::breath, 0); setP(p, ID::panspread, 0); p.prepareToPlay(44100, 512); };
        AeolaDroneProcessor a; base(a); double flat = windowsRatio(a);
        AeolaDroneProcessor b; setP(b, ID::m1src, 1); setP(b, ID::m1dst, 8); setP(b, ID::m1amt, 0.6f); setP(b, ID::l1rate, 1.0f); base(b); double moving = windowsRatio(b);
        CHECK(flat < 1.15 && moving > 1.5, "LFO 1 -> Cutoff: loudness swings %.2fx with modulation vs %.2fx without", moving, flat);

        // amount 0 and source None leave the sound untouched
        auto render0 = [&](bool wired) { AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); if (wired) { setP(p, ID::m1src, 1); setP(p, ID::m1dst, 8); setP(p, ID::m1amt, 0.0f); setP(p, ID::m2src, 0); setP(p, ID::m2dst, 3); setP(p, ID::m2amt, 1.0f); } p.prepareToPlay(44100, 512); return render(p, 4.0); };
        auto r1 = render0(false), r2 = render0(true); bool same = r1.L.size() == r2.L.size(); for (size_t i = 0; same && i < r1.L.size(); ++i) same = r1.L[i] == r2.L[i] && r1.R[i] == r2.R[i];
        CHECK(same, "a slot with amount 0, or with source None, changes nothing (bit-identical)");

        // destination range check
        AeolaDroneProcessor c; setP(c, ID::skyon, 0); setP(c, ID::m1src, 1); setP(c, ID::m1dst, 8); setP(c, ID::m1amt, 1.0f); setP(c, ID::l1shape, 3); setP(c, ID::l1rate, 3.0f); c.prepareToPlay(44100, 512);
        float lo = 9, hi = -9; for (int i = 0; i < 100; ++i) { render(c, 0.05); float d = c.modDestShown(8); lo = std::min(lo, d); hi = std::max(hi, d); }
        CHECK(std::fabs(lo + 1.0f) < 1e-3f && std::fabs(hi - 1.0f) < 1e-3f, "square LFO at 100%% reports Cutoff modulation %.2f .. %.2f (= +-4 octaves)", lo, hi);

        // tempo sync with a host that supplies tempo and position
        struct Head : juce::AudioPlayHead { double bpm = 120, ppq = 0; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm(bpm); i.setPpqPosition(ppq); i.setIsPlaying(playing); i.setTimeInSeconds(ppq * 60.0 / bpm); return i; } };
        for (double bpm : { 60.0, 120.0 })
        {
            AeolaDroneProcessor p; Head h; h.bpm = bpm; setP(p, ID::skyon, 0); setP(p, ID::l1sync, 1); setP(p, ID::l1div, 4); setP(p, ID::l1shape, 0); p.prepareToPlay(44100, 512); p.setPlayHead(&h);
            int cyc = 0; float prev = 0; for (int blk = 0; blk < 44100 * 16 / 512; ++blk) { juce::AudioBuffer<float> buf(2, 512); buf.clear(); juce::MidiBuffer m; p.processBlock(buf, m); h.ppq += 512.0 / 44100.0 * bpm / 60.0; float v = p.modSrcShown(SrcLfo1); if (prev < 0 && v >= 0) ++cyc; prev = v; }
            CHECK(std::abs(cyc - (int) (16 * bpm / 60)) <= 1, "LFO synced to 1/4 note follows host tempo: %d cycles in 16 s at %.0f BPM", cyc, bpm);
            p.setPlayHead(nullptr);
        }

        // sky sources are smooth and in range
        {
            struct Clock : juce::AudioPlayHead { double t = 43200.0; juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setTimeInSeconds(t); i.setIsPlaying(true); return i; } } clk;   // noon UTC: busy sky
            AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); p.prepareToPlay(44100, 512); p.setPlayHead(&clk);
            std::vector<float> v[4]; for (int i = 0; i < 300; ++i) { render(p, 0.1); clk.t += 0.1; for (int k = 0; k < 4; ++k) v[k].push_back(p.modSrcShown(SrcAircraft + k)); }
            p.setPlayHead(nullptr);
            { UiSnapshot u; p.getUi(u); printf("  info: sky aircraft %d, source %d, raw shown %.3f\n", u.nAc, u.source, p.modSrcShown(SrcAircraft)); }
            bool ok = true; double worst = 0; for (int k = 0; k < 4; ++k) for (size_t i = 0; i < v[k].size(); ++i) { if (v[k][i] < -1e-6f || v[k][i] > 1.0001f) ok = false; if (i) worst = std::max(worst, (double) std::fabs(v[k][i] - v[k][i - 1])); }
            CHECK(ok && v[0].back() > 0.05f && worst < 0.03, "Aircraft count/altitude/proximity/speed stay in 0..1 (now %.2f %.2f %.2f %.2f), max change per 0.1 s %.4f", v[0].back(), v[1].back(), v[2].back(), v[3].back(), worst);
        }
        // MIDI sources
        {
            AeolaDroneProcessor p; p.prepareToPlay(44100, 512);
            juce::MidiBuffer m; m.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 0); m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 64), 4); m.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 8); m.addEvent(juce::MidiMessage::channelPressureChange(1, 100), 12);
            render(p, 0.2, 512, &m);
            CHECK(std::fabs(p.modSrcShown(SrcModWheel) - 1.0f) < 0.01f && std::fabs(p.modSrcShown(SrcVelocity) - 64.0f / 127.0f) < 0.01f && p.modSrcShown(SrcPitchBend) > 0.99f && std::fabs(p.modSrcShown(SrcAftertouch) - 100.0f / 127.0f) < 0.01f, "mod wheel %.2f, velocity %.2f, pitch bend %.2f, aftertouch %.2f", p.modSrcShown(SrcModWheel), p.modSrcShown(SrcVelocity), p.modSrcShown(SrcPitchBend), p.modSrcShown(SrcAftertouch));
        }
        // every target accepts modulation without breaking the output
        {
            bool ok = true; for (int dst = 1; dst < kNumDst; ++dst)
            { AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); setP(p, ID::m1src, 1); setP(p, ID::m1dst, (float) dst); setP(p, ID::m1amt, 1.0f); setP(p, ID::l1shape, 3); setP(p, ID::l1rate, 4.0f); setP(p, ID::ftype, 1); setP(p, ID::drive, 12); p.prepareToPlay(44100, 512);
              auto r = render(p, 4.0); if (r.nan || r.peak > 1.0001f) { ok = false; printf("    target %s: nan=%d peak %.3f\n", modDstNames()[dst], (int) r.nan, r.peak); } }
            CHECK(ok, "all %d targets modulated by a 100%% square LFO: no NaN, output stays limited", kNumDst - 1);
        }
    }

    printf("[12] filter, drive and modulation survive save/load\n");
    {
        AeolaDroneProcessor p; p.prepareToPlay(44100, 512);
        setP(p, ID::ftype, 2); setP(p, ID::drive, 17.5f); setP(p, ID::ftrack, 0.4f); setP(p, ID::fmix, 0.6f); setP(p, ID::m3src, 6); setP(p, ID::m3dst, 10); setP(p, ID::m3amt, -0.35f); setP(p, ID::l2sync, 1); setP(p, ID::l2div, 5); setP(p, ID::l2shape, 4); setP(p, ID::srca, 17);
        juce::MemoryBlock mb; p.getStateInformation(mb); AeolaDroneProcessor q; q.setStateInformation(mb.getData(), (int) mb.getSize());
        auto g = [&](const char* id) { return q.apvts.getRawParameterValue(id)->load(); };
        CHECK((int) g(ID::ftype) == 2 && std::fabs(g(ID::drive) - 17.5f) < 0.05f && std::fabs(g(ID::ftrack) - 0.4f) < 0.01f && std::fabs(g(ID::fmix) - 0.6f) < 0.01f, "filter model, drive, key track, mix restored");
        CHECK((int) g(ID::m3src) == 6 && (int) g(ID::m3dst) == 10 && std::fabs(g(ID::m3amt) + 0.35f) < 0.01f && (int) g(ID::l2sync) == 1 && (int) g(ID::l2div) == 5 && (int) g(ID::l2shape) == 4 && (int) g(ID::srca) == 17, "modulation slot 3, LFO 2 sync/division/shape and the new timbre restored");
        // a v1.1 project (no filter/modulation parameters in the state) loads with everything off
        auto xml = p.apvts.copyState().createXml(); juce::XmlElement* kill = nullptr;
        for (auto* e : xml->getChildIterator()) if (e->getStringAttribute("id") == ID::ftype) kill = e; if (kill) xml->removeChildElement(kill, true);
        for (auto* e = xml->getFirstChildElement(); e;) { auto* nx = e->getNextElement(); juce::String id = e->getStringAttribute("id"); if (id.startsWith("m") && id.length() == 5 && id.endsWith("src")) xml->removeChildElement(e, true); e = nx; }
        juce::MemoryBlock old; AeolaDroneProcessor::copyXmlToBinary(*xml, old); AeolaDroneProcessor r; r.setStateInformation(old.getData(), (int) old.getSize());
        CHECK((int) r.apvts.getRawParameterValue(ID::ftype)->load() == 0 && (int) r.apvts.getRawParameterValue(ID::m3src)->load() == 0, "state without the new parameters loads with Clean filter and empty slots");
    }

    printf("[9b] how many aircraft really sound at once (end to end, simulated busy sky, radius 250 nm)\n");
    {
        struct Cs { const char* n; int ma, vo; float ev; };
        const Cs cs[] = { { "Aircraft 8, Voices 12 (defaults)", 8, 12, 0.5f }, { "Aircraft 12, Voices 12", 12, 12, 0.5f }, { "Aircraft 12, Voices 16, Evolve 0.5", 12, 16, 0.5f }, { "Aircraft 12, Voices 16, Evolve 0", 12, 16, 0.0f } };
        int best = 0;
        for (auto& c : cs)
        {
            AeolaDroneProcessor q; q.setCustomPlace(50.0, 8.5, "Frankfurt area"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 3); setP(q, ID::radius, 250.0f);
            setP(q, ID::maxac, (float) c.ma); setP(q, ID::voices, (float) c.vo); setP(q, ID::evolve, c.ev); setP(q, ID::ghostmin, 0); setP(q, ID::consonance, 0.5f);
            q.prepareToPlay(44100, 512); render(q, 20.0);
            int most = 0, sum = 0, n = 0, aircraftSeen = 0;
            for (int i = 0; i < 40; ++i)
            {   render(q, 3.0); UiSnapshot u; if (!q.getUi(u)) continue; int snd = 0; for (int v = 0; v < u.nVoices; ++v) if (u.v[v].kind == 1 && (u.v[v].id & kAcVoiceBit) && u.v[v].level > 0.001f) ++snd;
                most = std::max(most, snd); sum += snd; ++n; aircraftSeen = std::max(aircraftSeen, u.nAc); }
            printf("  info: %-38s aircraft in range up to %2d | voices sounding avg %.1f, max %d\n", c.n, aircraftSeen, n ? (double) sum / n : 0.0, most);
            best = std::max(best, most);
        }
        CHECK(best >= 12, "12 aircraft can sound at the same time end to end (best %d)", best);
    }

    printf("[9c] value labels above the drawer knobs follow the settings\n");
    {
        AeolaDroneProcessor q; q.setProp("tourDone", true); q.setProp("simple", false); setP(q, ID::source, 4); q.prepareToPlay(44100, 512); render(q, 5.0);
        std::unique_ptr<juce::AudioProcessorEditor> te(q.createEditor()); te->setBounds(0, 0, 1040, 760);
        auto* e = dynamic_cast<AeolaDroneEditor*>(te.get()); auto pump = [&]() { juce::MessageManager::getInstance()->runDispatchLoopUntil(120); };
        auto txt = [&](const char* id, float v) { if (auto* r = q.apvts.getParameter(id)) return r->getText(r->convertTo0to1(v), 20); return juce::String("?"); };
        struct C { int drawer; const char* id; float a, b; };
        const C cs[] = { { 2, ID::rvmix, 0.25f, 0.8f }, { 2, ID::rvsize, 0.3f, 0.9f }, { 2, ID::north, 0.1f, 0.9f }, { 3, ID::cutoff, 500.0f, 4000.0f }, { 3, ID::reso, 0.2f, 0.7f },
                         { 1, ID::brightness, 0.2f, 0.8f }, { 5, ID::consonance, 0.2f, 0.9f }, { 0, ID::radius, 30.0f, 120.0f }, { 0, ID::maxac, 4.0f, 11.0f } };
        int bad = 0;
        for (auto& c : cs)
        {   e->setDrawer(c.drawer); pump();
            for (float v : { c.a, c.b }) { setP(q, c.id, v); pump(); const juce::String shown = e->valueAbove(c.id), want = txt(c.id, v); if (shown != want) { ++bad; printf("    %s: label '%s', setting '%s'\n", c.id, shown.toRawUTF8(), want.toRawUTF8()); } }
        }
        CHECK(bad == 0, "the number above each knob equals the setting after every change (%d mismatches)", bad);
    }

    printf("[9d] live-data budget, limit message, no automatic simulation, airport codes\n");
    {
        // spread what is left of the daily quota over the time that is left
        CHECK(std::fabs(budgetIntervalSec(500, 86400) - 183.8) < 1.0, "full quota, full day: one request every %.0f s", budgetIntervalSec(500, 86400));
        CHECK(budgetIntervalSec(500, 43200) < 95.0 && budgetIntervalSec(500, 43200) > 85.0, "full quota, half a day left: every %.0f s", budgetIntervalSec(500, 43200));
        CHECK(budgetIntervalSec(60, 86400) == 300.0, "little quota left: slow down to the 5 minute ceiling");
        CHECK(budgetIntervalSec(10, 20000) >= 20000.0, "quota used up: wait for the refill (%.0f s)", budgetIntervalSec(10, 20000));
        CHECK(budgetIntervalSec(400, 600) == 20.0, "plenty of quota near the refill: never faster than 20 s");
        CHECK(budgetIntervalSec(-1, 0) == 60.0, "no quota information yet: 60 s");
        // headers exactly as adsb.lol sent them on a 429
        juce::StringPairArray h; h.set("x-ratelimit-limit", "500, 4;w=1, 60;w=60, 500;w=86400"); h.set("x-ratelimit-remaining", "0"); h.set("x-ratelimit-reset", "37031");
        Quota qt; readQuota(h, 1000.0, qt);
        CHECK(qt.limit == 500 && qt.remaining == 0 && std::fabs(qt.resetAt - 38031.0) < 0.5, "rate-limit headers are read (limit %.0f, remaining %.0f, resets in %.0f s)", qt.limit, qt.remaining, qt.resetAt - 1000.0);
        const double nowS = (double) juce::Time::currentTimeMillis() / 1000.0;
        const juce::String m = limitMessage(nowS + 37031.0, nowS);
        CHECK(m.startsWith("Daily data limit reached at adsb.lol, resets ") && m.length() == 50, "limit message names the reset time: '%s'", m.toRawUTF8());
        CHECK(limitMessage(nowS + 40.0, nowS).contains("asked us to wait"), "a short wait says so instead of 'daily'");
        // airport codes
        CHECK(juce::String(iataOf("ESSA")) == "ARN" && juce::String(iataOf("EGLL")) == "LHR" && juce::String(iataOf("WSSS")) == "SIN" && juce::String(iataOf("XXXX")) == "XXXX", "IATA codes shown, unknown codes fall back to the ICAO code");
        { int na; const Airport* ap = airports(na); bool all = true; for (int i = 0; i < na; ++i) all = all && juce::String(iataOf(ap[i].icao)).length() == 3; CHECK(all, "every airport in the list has an IATA code (%d airports)", na); }
        { AeolaDroneProcessor q; CHECK(q.placeName().startsWith("ARN  Stockholm"), "place name uses the IATA code: '%s'", q.placeName().toRawUTF8()); }
        // Auto never invents traffic: whatever the network does, the badge must not say Simulated
        {   AeolaDroneProcessor q; setP(q, ID::source, 0); q.setProp("backupFeed", false); q.prepareToPlay(44100, 512); render(q, 20.0);
            UiSnapshot u; bool ok = q.getUi(u); render(q, 20.0); ok = q.getUi(u);
            CHECK(ok && u.source != 4, "Auto with no live data does not fall back to simulated traffic (source %d, %d aircraft, feed: '%s')", u.source, u.nAc, u.feedErr); }
    }

    printf("[9e] update check, feedback and bug-report details\n");
    {
        CHECK(isNewerVersion("1.10.0", "1.9.2") && isNewerVersion("2.0", "1.9.9") && !isNewerVersion("1.9.0", "1.9.0") && !isNewerVersion("1.8.9", "1.9.0") && isNewerVersion("1.9.0.1", "1.9.0"), "version comparison is numeric, not alphabetical");
        auto ok = parseVersionFile("{\"version\":\"1.9.1\",\"notes\":\"Fixes the Earth view tilt.\"}"); CHECK(ok.valid && ok.version == "1.9.1" && ok.notes.startsWith("Fixes"), "a version file is read");
        CHECK(!parseVersionFile("<html>404</html>").valid && !parseVersionFile("{\"version\":\"latest\"}").valid && !parseVersionFile("{\"version\":\"\"}").valid && !parseVersionFile("").valid, "garbage, a 404 page or a non-number is not an update");
        {   // the whole update path against a local file: one check a day, newer version shown, switch-off respected, own settings file
            auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("aeola_update_test"); dir.createDirectory();
            auto vf = dir.getChildFile("version.json"); vf.replaceWithText("{\"version\":\"99.1.0\",\"notes\":\"test\"}");
            UpdateCheck::fileOverride() = dir.getChildFile("update.json"); UpdateCheck::fileOverride().deleteFile();
            CHECK(!UpdateCheck::available().valid, "no update known before the first check");
            UpdateCheck::runIfDue([] { return false; }, juce::URL(vf).toString(true));
            auto up = UpdateCheck::available(); CHECK(up.valid && up.version == "99.1.0", "a newer version on the page is found (%s)", up.version.toRawUTF8());
            vf.replaceWithText("{\"version\":\"99.2.0\"}"); UpdateCheck::runIfDue([] { return false; }, juce::URL(vf).toString(true));
            CHECK(UpdateCheck::available().version == "99.1.0", "a second check within a day does not fetch again");
            UpdateCheck::setEnabled(false); CHECK(!UpdateCheck::available().valid && !UpdateCheck::enabled(), "switched off: nothing is shown");
            UpdateCheck::setEnabled(true); CHECK(UpdateCheck::available().valid, "switched on again: shown again");
            UpdateCheck::fileOverride().deleteFile(); vf.deleteFile(); UpdateCheck::fileOverride() = juce::File();
        }
        AeolaDroneProcessor q; q.setCustomPlace(59.3293, 18.0686, "Somebody's Street 1, Stockholm"); setP(q, ID::source, 4); q.prepareToPlay(44100, 512); render(q, 5.0);
        std::unique_ptr<juce::AudioProcessorEditor> te(q.createEditor()); te->setBounds(0, 0, 1040, 760);
        const auto dg = dynamic_cast<AeolaDroneEditor*>(te.get())->collectDiagnostics().text();
        printf("%s", dg.toRawUTF8());
        CHECK(dg.contains(JucePlugin_VersionString) && dg.contains("Sky source") && dg.contains("custom place") && dg.contains("Aircraft"), "the details name version, source, place kind and aircraft");
        CHECK(!dg.contains("Somebody") && !dg.contains("59.3") && !dg.contains("18.06"), "the details contain no address or coordinates of a custom place");
        const auto url = feedbackUrl("abc").toString(true);
        CHECK(url.startsWith("mailto:aeola@skepp.se") && url.contains("subject=") && url.contains("body="), "feedback opens an email to %s", links::feedbackEmail);
    }

    printf("[9f] web page screenshots (busy simulated sky)\n");
    {
        AeolaDroneProcessor q; q.setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 3); setP(q, ID::radius, 120.0f);
        setP(q, ID::maxac, 12.0f); setP(q, ID::voices, 14.0f); q.setProp("tourDone", true); q.setProp("simple", true); q.prepareToPlay(44100, 512); render(q, 30.0);
        for (int tabNo : { 0, 2 })
        {   q.setProp("skyTab", tabNo); std::unique_ptr<juce::AudioProcessorEditor> te(q.createEditor()); te->setBounds(0, 0, 1040, 760);
            for (int i = 0; i < 8; ++i) { render(q, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
            auto img = te->createComponentSnapshot(te->getLocalBounds(), true, 1.0f); auto f = src.getChildFile("tests/out").getChildFile(tabNo == 0 ? "shot_sound.png" : "shot_earth.png"); f.deleteFile(); juce::FileOutputStream o(f); juce::PNGImageFormat().writeImageToStream(img, o);
        }
    }

    printf("[9g] MIDI out (aircraft as notes)\n");
    {
        auto run = [&](float on, float mode, float silent, int& nOn, int& nOff, int& chMask, float& peak, int& badVel, int& balance) {
            AeolaDroneProcessor q; q.setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 3); setP(q, ID::radius, 120.0f);
            setP(q, ID::maxac, 12.0f); setP(q, ID::midiout, on); setP(q, ID::midimode, mode); setP(q, ID::midisilent, silent);
            q.prepareToPlay(44100, 512);
            nOn = nOff = chMask = badVel = 0; peak = 0; int held[17][128] = {};
            for (int blk = 0; blk < 44100 * 40 / 512; ++blk)
            {
                juce::AudioBuffer<float> buf(2, 512); buf.clear(); juce::MidiBuffer m; q.processBlock(buf, m);
                peak = std::max(peak, buf.getMagnitude(0, 512));
                for (auto meta : m)
                {
                    auto msg = meta.getMessage();
                    if (msg.isNoteOn()) { ++nOn; chMask |= 1 << (msg.getChannel() - 1); ++held[msg.getChannel()][msg.getNoteNumber()]; if (msg.getVelocity() < 1) ++badVel; }
                    else if (msg.isNoteOff()) { ++nOff; --held[msg.getChannel()][msg.getNoteNumber()]; }
                }
            }
            balance = 0; for (auto& ch : held) for (int v : ch) balance += std::abs(v);
            setP(q, ID::midiout, 0.0f);                    // switching off must release everything
            for (int blk = 0; blk < 8; ++blk)
            {   juce::AudioBuffer<float> buf(2, 512); buf.clear(); juce::MidiBuffer m; q.processBlock(buf, m);
                for (auto meta : m) { auto msg = meta.getMessage(); if (msg.isNoteOff()) { ++nOff; --held[msg.getChannel()][msg.getNoteNumber()]; } } }
            int after = 0; for (auto& ch : held) for (int v : ch) after += std::abs(v);
            if (on > 0.5f) CHECK(after == 0, "turning MIDI out off releases every note (hanging: %d)", after);
        };
        int nOn, nOff, chMask, badVel, bal; float peak;
        run(0.0f, 0.0f, 0.0f, nOn, nOff, chMask, peak, badVel, bal);
        CHECK(nOn == 0 && nOff == 0, "MIDI out off: no MIDI is produced (on %d, off %d)", nOn, nOff);
        run(1.0f, 0.0f, 0.0f, nOn, nOff, chMask, peak, badVel, bal);
        printf("    per class: %d note-ons, %d note-offs, channel mask 0x%x, peak audio %.3f\n", nOn, nOff, chMask, peak);
        CHECK(nOn > 0, "MIDI out on: aircraft produce notes (%d)", nOn);
        CHECK(badVel == 0, "no zero-velocity note-ons");
        CHECK(chMask > 1, "per-class mode uses several channels (mask 0x%x)", chMask);
        CHECK(peak > 0.0f, "audio still plays with MIDI out on");
        run(1.0f, 1.0f, 0.0f, nOn, nOff, chMask, peak, badVel, bal);
        CHECK(nOn > 0 && chMask == 1, "single-channel mode keeps everything on channel 1 (mask 0x%x)", chMask);
        run(1.0f, 0.0f, 1.0f, nOn, nOff, chMask, peak, badVel, bal);
        CHECK(nOn > 0 && peak == 0.0f, "silence option mutes audio but keeps MIDI (peak %.4f)", peak);
        {   // one class per instance: channels never mix, and the union of all classes equals the unfiltered output
            int total = 0, sum = 0; bool pure = true;
            run(1.0f, 0.0f, 0.0f, total, nOff, chMask, peak, badVel, bal);
            for (int c = 0; c < kNumClasses; ++c)
            {
                AeolaDroneProcessor q; q.setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 3); setP(q, ID::radius, 120.0f);
                setP(q, ID::maxac, 12.0f); setP(q, ID::midiout, 1.0f); setP(q, ID::midiclass, (float) (c + 1)); q.prepareToPlay(44100, 512);
                for (int blk = 0; blk < 44100 * 40 / 512; ++blk)
                {   juce::AudioBuffer<float> buf(2, 512); buf.clear(); juce::MidiBuffer m; q.processBlock(buf, m);
                    for (auto meta : m) { auto msg = meta.getMessage(); if (msg.isNoteOn()) { ++sum; if (msg.getChannel() != c + 1) pure = false; } } }
            }
            CHECK(pure, "class filter: each instance sends only its own class channel");
            CHECK(sum == total, "class filter: the nine classes together equal the unfiltered notes (%d vs %d)", sum, total);
        }
    }

    printf("[9h] MIDI out: one source Aeola, nine followers (one per class)\n");
    {
        auto make = [&](float seed, float cls) {
            auto q = std::make_unique<AeolaDroneProcessor>(); q->setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(*q, ID::source, 4); setP(*q, ID::simsync, 1);
            setP(*q, ID::simseed, seed); setP(*q, ID::radius, 120.0f); setP(*q, ID::maxac, 12.0f); setP(*q, ID::midiout, 1.0f); setP(*q, ID::midiclass, cls);
            q->prepareToPlay(44100, 512); return q; };
        auto src = make(3.0f, 0.0f);
        std::vector<std::unique_ptr<AeolaDroneProcessor>> fol;
        for (int c = 0; c < kNumClasses; ++c) fol.push_back(make(7.0f, (float) (c + 1)));     // different seed: a follower that mapped its own sky would differ
        int srcOn = 0, folOn = 0, wrongCh = 0, folHeld[9][128] = {}; bool allFollow = true;
        auto pump = [&](AeolaDroneProcessor& q, int* onCount, int cls) {
            juce::AudioBuffer<float> buf(2, 512); buf.clear(); juce::MidiBuffer m; q.processBlock(buf, m);
            for (auto meta : m) { auto msg = meta.getMessage();
                if (msg.isNoteOn()) { if (onCount) ++*onCount; if (cls >= 0) { if (msg.getChannel() != cls + 1) ++wrongCh; ++folHeld[cls][msg.getNoteNumber()]; } }
                else if (msg.isNoteOff() && cls >= 0) --folHeld[cls][msg.getNoteNumber()]; } };
        for (int blk = 0; blk < 44100 * 40 / 512; ++blk)
        {   pump(*src, &srcOn, -1); for (int c = 0; c < kNumClasses; ++c) pump(*fol[(size_t) c], &folOn, c); }
        for (auto& f : fol) if (!f->isFollowing()) allFollow = false;
        CHECK(srcOn > 0, "source Aeola sends aircraft notes (%d)", srcOn);
        CHECK(allFollow, "all nine class instances follow the source");
        CHECK(folOn == srcOn, "the nine followers together play exactly the source's notes (%d vs %d)", folOn, srcOn);
        CHECK(wrongCh == 0, "every follower keeps its own class channel");
        src.reset();                                            // source gone: followers must let go within ~2 s and release their notes
        juce::Thread::sleep(2300);
        for (int blk = 0; blk < 40; ++blk) for (int c = 0; c < kNumClasses; ++c) pump(*fol[(size_t) c], nullptr, c);
        bool anyFollow = false; for (auto& f : fol) if (f->isFollowing()) anyFollow = true;
        CHECK(!anyFollow, "followers stop following when the source disappears");
    }

    printf("[9i] modulation per aircraft class (1.11)\n");
    {
        CHECK(juce::String(modSrcNames()[SrcSpace]) == "Space" && juce::String(modDstNames()[DstOutput]) == "Output" && SrcClassCount == 16 && DstClassLevel == 15,
              "old source/target indices unchanged (old projects keep their routings)");
        CHECK(juce::String(modSrcNames()[SrcClassCount + kHeli]) == "Helicopter count" && juce::String(modDstNames()[DstClassOctave + kHeli]) == "Helicopter octave",
              "per-class names: %s, %s", modSrcNames()[SrcClassCount + kHeli], modDstNames()[DstClassOctave + kHeli]);
        CHECK(juce::String(ID::modDestParam(DstClassOctave + kHeli)) == ID::classId(kHeli, 2) && juce::String(ID::modDestParam(DstClassLevel + kLarge)) == ID::classId(kLarge, 1),
              "class targets point at the class knobs (modulation rings)");
        // Narrowbody count -> Narrowbody octave, +25 % = one octave when four or more narrowbodies are in range
        auto mk = [&](bool wired) {
            auto q = std::make_unique<AeolaDroneProcessor>(); q->setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(*q, ID::source, 4); setP(*q, ID::simsync, 1); setP(*q, ID::simseed, 3);
            setP(*q, ID::radius, 120.0f); setP(*q, ID::maxac, 12.0f); setP(*q, ID::consonance, 0.0f);
            if (wired) { setP(*q, ID::m1src, (float) (SrcClassCount + kLarge)); setP(*q, ID::m1dst, (float) (DstClassOctave + kLarge)); setP(*q, ID::m1amt, 0.25f); }
            q->prepareToPlay(44100, 512); render(*q, 30.0); return q; };
        auto a = mk(false), b = mk(true);
        UiSnapshot ua, ub; a->getUi(ua); b->getUi(ub);
        int n = 0; double diff = 0; int nbAll = 0;
        for (int i = 0; i < ua.nAc; ++i) if (ua.ac[i].category == kLarge) ++nbAll;
        for (int i = 0; i < ua.nAc; ++i) if (ua.ac[i].category == kLarge && ua.ac[i].voiced)
            for (int k = 0; k < ub.nAc; ++k) if (ub.ac[k].id == ua.ac[i].id && ub.ac[k].voiced) { diff += ub.ac[k].note - ua.ac[i].note; ++n; }
        printf("    narrowbodies in range %d, count source %.2f, voiced in both %d, mean shift %.1f semitones\n", nbAll, b->modSrcShown(SrcClassCount + kLarge), n, n ? diff / n : 0.0);
        CHECK(b->modSrcShown(SrcClassCount + kLarge) > 0.2f, "Narrowbody count source is live (%.2f)", b->modSrcShown(SrcClassCount + kLarge));
        CHECK(n > 0 && diff / n > 6.0, "Narrowbody count -> Narrowbody octave moves narrowbodies up (mean %.1f semitones)", n ? diff / n : 0.0);
        bool othersSame = true;
        for (int i = 0; i < ua.nAc; ++i) if (ua.ac[i].category != kLarge && ua.ac[i].voiced)
            for (int k = 0; k < ub.nAc; ++k) if (ub.ac[k].id == ua.ac[i].id && ub.ac[k].voiced && std::abs(ub.ac[k].note - ua.ac[i].note) >= 12) othersSame = false;
        CHECK(othersSame, "other classes are not moved by an octave");
    }

    printf("[9j] 1.12: automation list, mixer, class outputs, map data, state migration\n");
    {
        {   // exactly 64 automatable parameters (Ableton Live configures a plug-in by itself up to 64)
            AeolaDroneProcessor q; int nAuto = 0, nAll = 0; bool tensionAuto = false, srcaAuto = true, mixAuto = false;
            for (auto* prm : q.getParameters()) { ++nAll; if (prm->isAutomatable()) ++nAuto;
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) { if (r->paramID == ID::tension) tensionAuto = prm->isAutomatable(); if (r->paramID == ID::srca) srcaAuto = prm->isAutomatable(); if (r->paramID == juce::String(ID::classMixId(kHeli, 0))) mixAuto = prm->isAutomatable(); } }
            printf("    %d parameters, %d automatable\n", nAll, nAuto);
            CHECK(nAuto == 64, "exactly 64 automatable parameters (%d)", nAuto);
            CHECK(tensionAuto && mixAuto && !srcaAuto, "macros and mixer automatable, setup menus not");
        }
        auto busy = [&](AeolaDroneProcessor& q) { q.setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 3); setP(q, ID::radius, 120.0f); setP(q, ID::maxac, 12.0f); };
        auto acLevels = [&](AeolaDroneProcessor& q, int cls, float& lvl, float& pan, int& n) {
            UiSnapshot u; q.getUi(u); lvl = 0; pan = 0; n = 0;
            for (int v = 0; v < u.nVoices; ++v) if (u.v[v].kind == 1 && (u.v[v].id & kAcVoiceBit))
                for (int i = 0; i < u.nAc; ++i) if ((u.v[v].id & ~kAcVoiceBit) == u.ac[i].id && (cls < 0 || u.ac[i].category == cls)) { lvl = std::max(lvl, u.v[v].level); pan += u.v[v].pan; ++n; } };
        {   // faders at -inf silence the aircraft; the drone bed keeps playing
            AeolaDroneProcessor q; busy(q); for (int c = 0; c < kNumClasses; ++c) setP(q, ID::classMixId(c, 0), -60.0f);
            q.prepareToPlay(44100, 512); auto r = render(q, 20.0); float lvl, pan; int n; acLevels(q, -1, lvl, pan, n);
            CHECK(n > 0 && lvl < 1e-3f && r.peak > 0.0f, "all faders down: %d aircraft voices silent (max level %.5f), drone bed still plays (peak %.3f)", n, lvl, r.peak);
        }
        {   // a class's pan moves its voices
            AeolaDroneProcessor a, b; busy(a); busy(b); setP(b, ID::classMixId(kLarge, 1), 1.0f); a.prepareToPlay(44100, 512); b.prepareToPlay(44100, 512); render(a, 20.0); render(b, 20.0);
            float la, pa, lb, pb; int na, nb; acLevels(a, kLarge, la, pa, na); acLevels(b, kLarge, lb, pb, nb);
            CHECK(na > 0 && nb > 0 && pb / nb > pa / na + 0.3f, "Narrowbody pan R 100 moves narrowbodies right (mean pan %.2f -> %.2f)", na ? pa / na : 0.0f, nb ? pb / nb : 0.0f);
        }
        {   // meters see each class; class outputs carry a class and take it out of the main mix
            AeolaDroneProcessor q; busy(q); q.enableAllBuses(); q.setProp("classOuts", true); q.pollMessageThreadTasks();
            const int nch = q.getTotalNumOutputChannels(); q.prepareToPlay(44100, 512);
            double clsE[kNumClasses] = {}, mainE = 0; float meterMax = 0;
            for (int blk = 0; blk < 44100 * 20 / 512; ++blk)
            {
                juce::AudioBuffer<float> buf(nch, 512); buf.clear(); juce::MidiBuffer m; q.processBlock(buf, m);
                for (int i = 0; i < 512; ++i) mainE += buf.getSample(0, i) * buf.getSample(0, i);
                for (int c = 0; c < kNumClasses; ++c) { const int ch = q.getChannelIndexInProcessBlockBuffer(false, c + 2, 0); for (int i = 0; i < 512; ++i) clsE[c] += buf.getSample(ch, i) * buf.getSample(ch, i); meterMax = std::max(meterMax, q.classPeak(c)); }
            }
            int withSignal = 0; for (double e : clsE) if (e > 1e-6) ++withSignal;
            printf("    %d channels, classes with signal on their own output: %d, main energy %.3f, meter max %.3f\n", nch, withSignal, mainE, meterMax);
            CHECK(nch == 22, "22 output channels: main, reverb, nine classes (%d)", nch);
            CHECK(withSignal >= 2, "class outputs carry their class (%d classes heard)", withSignal);
            CHECK(meterMax > 0.0f, "mixer meters move (%.3f)", meterMax);
            AeolaDroneProcessor off; busy(off); off.enableAllBuses(); off.prepareToPlay(44100, 512); double offE = 0;
            for (int blk = 0; blk < 44100 * 20 / 512; ++blk) { juce::AudioBuffer<float> buf(nch, 512); buf.clear(); juce::MidiBuffer m; off.processBlock(buf, m); for (int i = 0; i < 512; ++i) offE += buf.getSample(0, i) * buf.getSample(0, i);
                for (int c = 0; c < kNumClasses; ++c) { const int ch = off.getChannelIndexInProcessBlockBuffer(false, c + 2, 0); for (int i = 0; i < 512; ++i) if (buf.getSample(ch, i) != 0.0f) { offE = -1e9; } } }
            CHECK(offE > mainE, "with Separate outputs off, class outputs stay silent and the main mix has everything (%.3f > %.3f)", offE, mainE);
        }
        {   // a project saved before 1.12 has no mixer parameters: they must come back at 0 dB, not at their minimum (silence)
            AeolaDroneProcessor a; juce::MemoryBlock mb; a.getStateInformation(mb);
            auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), (int) mb.getSize());
            for (int c = 0; c < kNumClasses; ++c) for (int w = 0; w < 2; ++w) if (auto* e = xml->getChildByAttribute("id", ID::classMixId(c, w))) xml->removeChildElement(e, true);
            juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary(*xml, old);
            AeolaDroneProcessor b; setP(b, ID::classMixId(kHeli, 0), -30.0f); b.setStateInformation(old.getData(), (int) old.getSize());
            CHECK(std::fabs(b.P(ID::classMixId(kHeli, 0))) < 0.01f && std::fabs(b.P(ID::classMixId(kLarge, 0))) < 0.01f, "old projects: faders at 0 dB (%.2f)", b.P(ID::classMixId(kLarge, 0)));
        }
        {   // presets keep MIDI out and the mix
            AeolaDroneProcessor q; setP(q, ID::midiout, 1.0f); setP(q, ID::classMixId(kHeavy, 0), -12.0f); q.loadFactoryPreset(3);
            CHECK(q.P(ID::midiout) > 0.5f && std::fabs(q.P(ID::classMixId(kHeavy, 0)) + 12.0f) < 0.05f, "a preset keeps MIDI out and the mixer");
        }
        {   // traffic beyond the radius is kept (for the map) up to the map area, and dropped beyond it
            Place ap; ap.lat = 50.0; ap.lon = 8.5; ap.radiusNm = 40.0; Frame f; f.count = 3;
            const double dists[3] = { 20.0, 60.0, 200.0 };
            for (int i = 0; i < 3; ++i) { f.ac[i] = Aircraft(); f.ac[i].id = (uint32_t) (i + 1); f.ac[i].lat = ap.lat + dists[i] / 60.0; f.ac[i].lon = ap.lon; f.ac[i].altFt = 20000; }
            extrapolate(f, 0.0, ap);
            CHECK(f.count == 2 && mapAreaNm(40.0) == 80.0, "map area: radius 40 keeps aircraft up to 80 nm (%d kept)", f.count);
            CHECK(std::fabs(crossEtaMin(0.0, -100.0, 0.0, 360.0, 40.0, false) - 10.0) < 0.01, "time to reach the circle: 10 min (%.2f)", crossEtaMin(0.0, -100.0, 0.0, 360.0, 40.0, false));
        }
        {   // the new targets reach the engine and the mapping
            AeolaDroneProcessor q; setP(q, ID::source, 4); setP(q, ID::m1src, (float) SrcTension); setP(q, ID::tension, 1.0f); setP(q, ID::m1dst, (float) DstNorth); setP(q, ID::m1amt, 1.0f);
            q.prepareToPlay(44100, 512); render(q, 0.5);
            CHECK(q.modDestShown(DstNorth) > 0.4f, "Tension -> North connection is live (%.2f)", q.modDestShown(DstNorth));
        }
    }

    printf("[9k] map with traffic beyond the circle (hand-built frame)\n");
    {
        AeolaDroneProcessor q; q.setCustomPlace(50.0, 8.5, "Rhine-Main"); setP(q, ID::radius, 40.0f); setP(q, ID::source, 3);
        Frame f; f.epoch = 1000.0; f.count = 0;
        struct A { double dN, dE; float alt, trk, gs; int cat; const char* cs; };
        const A list[] = { { 10, 5, 22000, 90, 420, kLarge, "DLH4AB" }, { -15, -12, 9000, 200, 300, kProp, "BER12" }, { 5, -25, 34000, 45, 460, kHeavy, "UAE45" },
                           { 58, 10, 31000, 180, 450, kLarge, "RYR8KQ" }, { -20, 62, 12000, 270, 320, kCargo, "FDX61" }, { -55, -40, 37000, 300, 470, kBiz, "N650X" }, { 30, -60, 3000, 120, 110, kHeli, "CHX3" } };
        for (auto& a : list) { auto& x = f.ac[f.count++]; x = Aircraft(); x.id = 0x400000u + (uint32_t) f.count; x.lat = 50.0 + a.dN / 60.0; x.lon = 8.5 + a.dE / (60.0 * std::cos(50.0 * M_PI / 180.0)); x.altFt = a.alt; x.trackDeg = a.trk; x.gsKt = a.gs; x.category = x.legacyCat = a.cat; x.distNm = (float) std::hypot(a.dN, a.dE); std::snprintf(x.callsign, sizeof x.callsign, "%s", a.cs); std::snprintf(x.type, sizeof x.type, "B738"); }
        q.testReplay({ f }); q.setProp("tourDone", true); q.setProp("simple", false); q.setProp("skyTab", 1); q.setProp("drawer", 6); q.prepareToPlay(44100, 512); render(q, 3.0);
        UiSnapshot u; q.getUi(u);
        CHECK(u.nAc == 3 && u.nOut == 4, "3 aircraft inside the circle play, 4 beyond it are kept for the map (%d / %d)", u.nAc, u.nOut);
        std::unique_ptr<juce::AudioProcessorEditor> te(q.createEditor()); te->setBounds(0, 0, 1040, 760);
        for (int i = 0; i < 8; ++i) { render(q, 0.3); juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
        auto img = te->createComponentSnapshot(te->getLocalBounds(), true, 1.0f); auto fo = src.getChildFile("tests/out/editor_map_outside.png"); fo.deleteFile(); juce::FileOutputStream o(fo); juce::PNGImageFormat().writeImageToStream(img, o);
    }

    printf("[9l] 1.13: land data, new targets, names\n");
    {
        const float sthlm = landAreaNear(59.65, 17.92, 1.0, false), atlantic = landAreaNear(30.0, -40.0, 1.0, false), wide = landAreaNear(59.65, 17.92, 4.0, true);
        CHECK(sthlm > 100.0f && atlantic == 0.0f && wide > sthlm, "land near Stockholm (%.0f), none in the mid-Atlantic (%.0f), coarse data for wide views (%.0f)", sthlm, atlantic, wide);
        CHECK(juce::String(modDstNames()[DstClassLevel + kHeli]) == "Helicopter gain" && juce::String(modDstNames()[DstClassChop + kHeli]) == "Helicopter chop", "per-class target names: %s, %s", modDstNames()[DstClassLevel + kHeli], modDstNames()[DstClassChop + kHeli]);
        CHECK(juce::String(ID::modDestParam(DstRevSize)) == ID::rvsize && juce::String(ID::modDestParam(DstClassSwell + kCargo)) == ID::classId(kCargo, 3), "new targets point at their knobs");
        AeolaDroneProcessor q; setP(q, ID::source, 4); setP(q, ID::m1src, (float) SrcTension); setP(q, ID::tension, 1.0f); setP(q, ID::m1dst, (float) DstLfo1Rate); setP(q, ID::m1amt, 0.5f);
        q.prepareToPlay(44100, 512); render(q, 0.5);
        CHECK(q.modDestShown(DstLfo1Rate) > 0.4f, "Tension -> LFO 1 rate is live (%.2f)", q.modDestShown(DstLfo1Rate));
    }

    printf("[10] editor renders (screenshot to tests/out/)\n");
    {
        AeolaDroneProcessor p; setP(p, ID::source, 4); setP(p, ID::simsync, 1); setP(p, ID::simseed, 9); p.setProp("tourDone", true); p.setProp("simple", false);
        p.prepareToPlay(44100, 512); p.loadFactoryPreset(0); setP(p, ID::source, 4);
        render(p, 40.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        ed->setBounds(0, 0, 1040, 760);
        for (int i = 0; i < 20; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
        auto outDir = src.getChildFile("tests/out"); outDir.createDirectory();
        for (bool simple : { false, true })
        {
            p.setProp("simple", simple);
            ed.reset(p.createEditor()); ed->setBounds(0, 0, 1040, 760);
            for (int i = 0; i < 6; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile(simple ? "editor_simple.png" : "editor_advanced.png"); f.deleteFile();
            juce::FileOutputStream out(f); juce::PNGImageFormat png; bool ok = png.writeImageToStream(img, out);
            CHECK(ok && f.getSize() > 20000, "wrote %s (%lld bytes)", f.getFullPathName().toRawUTF8(), (long long) f.getSize());
            if (!simple)
            {   // Earth view: picture + paint cost (whole editor snapshot per frame, 1x) for Sound / Sky / Earth, and Earth while the camera moves
                auto timeIt = [&](int tabNo, bool spin) {
                    p.setProp("skyTab", tabNo); p.setProp("earthYaw", 0.0); p.setProp("earthTilt", 28.0);
                    for (int i = 0; i < 6; ++i) { render(p, 0.3); juce::MessageManager::getInstance()->runDispatchLoopUntil(20); auto w = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); }
                    const int n = 40; double t0 = juce::Time::getMillisecondCounterHiRes();
                    for (int i = 0; i < n; ++i) { if (spin) p.setProp("earthYaw", (double) i * 3.0); auto w = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); }
                    return (juce::Time::getMillisecondCounterHiRes() - t0) / n; };
                const double msSound = timeIt(0, false), msSky = timeIt(1, false), msEarth = timeIt(2, false), msSpin = timeIt(2, true);
                printf("    editor paint per frame: Sound %.1f ms, Sky %.1f ms, Earth %.1f ms, Earth while rotating %.1f ms (whole editor, software renderer, 1x)\n", msSound, msSky, msEarth, msSpin);
                p.setProp("earthYaw", 0.0); p.setProp("skyTab", 2); for (int i = 0; i < 4; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto im4 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f4 = outDir.getChildFile("editor_earth.png"); f4.deleteFile(); juce::FileOutputStream o4(f4); juce::PNGImageFormat().writeImageToStream(im4, o4);
                p.setProp("earthYaw", 40.0); p.setProp("earthTilt", 48.0); for (int i = 0; i < 3; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto im5 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f5 = outDir.getChildFile("editor_earth_rot.png"); f5.deleteFile(); juce::FileOutputStream o5(f5); juce::PNGImageFormat().writeImageToStream(im5, o5);
                p.setProp("skyTab", 0); p.setProp("earthYaw", 0.0); p.setProp("earthTilt", 28.0); }
            if (!simple)
            {   p.setProp("skyTab", 1); for (int i = 0; i < 4; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto im3 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f3 = outDir.getChildFile("editor_advanced_map.png"); f3.deleteFile(); juce::FileOutputStream o3(f3); juce::PNGImageFormat().writeImageToStream(im3, o3); p.setProp("skyTab", 0); }
            {   p.setProp("tourDone", false); std::unique_ptr<juce::AudioProcessorEditor> te(p.createEditor()); te->setBounds(0, 0, 1040, 760);
                if (auto* e = dynamic_cast<AeolaDroneEditor*>(te.get()))
                    for (int st : { 0, 1, 2, 3, 4 }) { e->tourStep(st); for (int i = 0; i < 3; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
                        auto imt = te->createComponentSnapshot(te->getLocalBounds(), true, 1.0f); auto ft = outDir.getChildFile(juce::String(simple ? "tour_simple_" : "tour_adv_") + juce::String(st) + ".png"); ft.deleteFile(); juce::FileOutputStream ot(ft); juce::PNGImageFormat().writeImageToStream(imt, ot); }
                p.setProp("tourDone", true); }
            if (simple)
            {   p.setProp("skyTab", 2); for (int i = 0; i < 4; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto im6 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f6 = outDir.getChildFile("editor_simple_earth.png"); f6.deleteFile(); juce::FileOutputStream o6(f6); juce::PNGImageFormat().writeImageToStream(im6, o6); p.setProp("skyTab", 0); }
            if (simple)
            {   p.setProp("skyTab", 1); for (int i = 0; i < 4; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto im2 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f2 = outDir.getChildFile("editor_simple_map.png"); f2.deleteFile(); juce::FileOutputStream o2(f2); juce::PNGImageFormat().writeImageToStream(im2, o2); p.setProp("skyTab", 0); }
        }
        // modulation page (preset with filter + four routings)
        p.setProp("simple", false); p.loadFactoryPreset(2 + 0); 
        for (int i = 0; i < (int) factoryPresets().size(); ++i) if (juce::String(factoryPresets()[(size_t) i].name) == "Radar Breathing") p.loadFactoryPreset(i);
        setP(p, ID::source, 4); setP(p, ID::m3src, 2); setP(p, ID::m3dst, 3); setP(p, ID::m3amt, -0.4f); setP(p, ID::m4src, (float) (SrcClassCount + kHeli)); setP(p, ID::m4dst, (float) (DstClassOctave + kHeli)); setP(p, ID::m4amt, 0.25f);
        ed.reset(p.createEditor()); ed->setBounds(0, 0, 1040, 760);
        if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->showModPage(true);
        setP(p, ID::m5src, (float) (SrcClassAlt + kHeavy)); setP(p, ID::m5dst, (float) DstNorth); setP(p, ID::m5amt, 0.4f);
        setP(p, ID::m6src, (float) SrcLfo2); setP(p, ID::m6dst, (float) DstLfo1Rate); setP(p, ID::m6amt, 0.3f);
        for (int i = 0; i < 8; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
        { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_mod.png"); f.deleteFile(); juce::FileOutputStream out(f); juce::PNGImageFormat png; bool ok = png.writeImageToStream(img, out);
          CHECK(ok && f.getSize() > 20000, "wrote %s (%lld bytes)", f.getFullPathName().toRawUTF8(), (long long) f.getSize()); }
        if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->showMixPage();
        setP(p, ID::classMixId(kHeavy, 0), -9.0f); setP(p, ID::classMixId(kLarge, 1), -0.4f);
        for (int i = 0; i < 6; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(40); }
        { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_mix.png"); f.deleteFile(); juce::FileOutputStream out(f); juce::PNGImageFormat png; bool ok = png.writeImageToStream(img, out);
          CHECK(ok && f.getSize() > 20000, "wrote %s (%lld bytes)", f.getFullPathName().toRawUTF8(), (long long) f.getSize()); }
        if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->showModPage(true);
        {   const char* nm[] = { "editor_tab_sound.png", "editor_tab_tune.png", "editor_tab_filter.png" };
            for (int d = 1; d <= 3; ++d)
            {   if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->setDrawer(d);
                for (int i = 0; i < 3; ++i) { render(p, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile(nm[d - 1]); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
            if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get()))
            {   // the filter graph must follow the settings (it was static before 1.8.2)
                e->setDrawer(3); juce::MessageManager::getInstance()->runDispatchLoopUntil(300); const int r0 = e->filterRepaints;
                setP(p, ID::cutoff, 400.0f); juce::MessageManager::getInstance()->runDispatchLoopUntil(300); const int r1 = e->filterRepaints;
                setP(p, ID::ftype, 2); juce::MessageManager::getInstance()->runDispatchLoopUntil(300); const int r2 = e->filterRepaints;
                setP(p, ID::reso, 0.8f); juce::MessageManager::getInstance()->runDispatchLoopUntil(300); const int r3 = e->filterRepaints;
                CHECK(r1 > r0 && r2 > r1 && r3 > r2, "filter graph repaints when cutoff, model and resonance change (%d %d %d %d)", r0, r1, r2, r3);
                auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_tab_filter_ladder.png"); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os);
            }
            if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->setDrawer(0); }
        ed.reset();
        {   // the Aircraft page and a custom place
            AeolaDroneProcessor q; q.setCustomPlace(59.3293, 18.0686, "Stockholm, Sweden"); setP(q, ID::radius, 90.0f); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 4);
            q.setProp("tourDone", true); q.setProp("simple", false); q.prepareToPlay(44100, 512); render(q, 30.0);
            ed.reset(q.createEditor()); ed->setBounds(0, 0, 1040, 760);
            for (int i = 0; i < 6; ++i) { render(q, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
            { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_place.png"); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
            for (int m = 0; m < 2; ++m)
            {   q.setProp("skyTab", m); for (int i = 0; i < 4; ++i) { render(q, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
                auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile(m ? "editor_map.png" : "editor_sound.png"); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
            q.setProp("skyTab", 0);
            if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->showModPage(false);
            for (int i = 0; i < 4; ++i) { render(q, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
            { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_skypage.png"); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
            if (auto* e = dynamic_cast<AeolaDroneEditor*>(ed.get())) e->showAircraftPage(true);
            for (int i = 0; i < 4; ++i) { render(q, 0.5); juce::MessageManager::getInstance()->runDispatchLoopUntil(30); }
            { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); auto f = outDir.getChildFile("editor_aircraft.png"); f.deleteFile(); juce::FileOutputStream os(f); juce::PNGImageFormat().writeImageToStream(img, os); }
            ed.reset();
        }
    }

    printf("[13] v1.3: classes mapping, places, migration\n");
    {
        AeolaDroneProcessor p; p.prepareToPlay(44100, 512);
        // an old project (no v1.3 parameters in the state) loads with sane defaults: airport kept, radius 40, class table at its defaults
        auto xml = p.apvts.copyState().createXml();
        for (auto* e = xml->getFirstChildElement(); e;) { auto* nx = e->getNextElement(); juce::String id = e->getStringAttribute("id"); if (id == ID::radius || id == ID::contrast || id == ID::swell || id == ID::simlevel || (id.startsWith("c") && id.length() > 5 && juce::CharacterFunctions::isDigit(id[1]))) xml->removeChildElement(e, true); e = nx; }
        juce::MemoryBlock old; AeolaDroneProcessor::copyXmlToBinary(*xml, old); AeolaDroneProcessor r; r.setStateInformation(old.getData(), (int) old.getSize());
        CHECK(std::fabs(r.apvts.getRawParameterValue(ID::radius)->load() - 40.0f) < 0.01f && std::fabs(r.apvts.getRawParameterValue(ID::contrast)->load() - 0.7f) < 0.01f && (int) r.apvts.getRawParameterValue(ID::classId(5, 0))->load() > 0, "state without v1.3 parameters loads with radius 40, contrast 70 %, class defaults");
        auto pl = r.makePlace(); CHECK(pl.isAirport && std::fabs(pl.radiusNm - 40.0) < 0.01 && std::string(pl.code) == "ESSA", "old projects keep their airport at 40 nm");

        // custom place + radius survive save/load and drive the place key
        AeolaDroneProcessor c; c.prepareToPlay(44100, 512);
        c.setCustomPlace(59.3293, 18.0686, "Stockholm, Sweden"); setP(c, ID::radius, 75.0f);
        auto cp = c.makePlace(); CHECK(!cp.isAirport && std::fabs(cp.lat - 59.3293) < 1e-4 && std::fabs(cp.radiusNm - 75.0) < 0.01, "custom place and radius are used (%.3f, %.3f, %.0f nm)", cp.lat, cp.lon, cp.radiusNm);
        CHECK(c.placeName() == "Stockholm, Sweden" && c.recentPlaces().size() == 1, "place name and recent list");
        c.setCustomPlace(51.5074, -0.1278, "London, United Kingdom"); c.setCustomPlace(59.3293, 18.0686, "Stockholm, Sweden");
        CHECK(c.recentPlaces().size() == 2 && c.recentPlaces()[0].name == "Stockholm, Sweden", "recent places: newest first, no duplicates");
        juce::MemoryBlock mb; c.getStateInformation(mb); AeolaDroneProcessor d; d.setStateInformation(mb.getData(), (int) mb.getSize());
        auto dp = d.makePlace(); CHECK(!dp.isAirport && std::fabs(dp.lat - 59.3293) < 1e-4 && std::fabs(dp.radiusNm - 75.0) < 0.01 && d.placeName() == "Stockholm, Sweden", "custom place survives save/load");

        // audio: classes mapping renders cleanly over a custom place, and is audibly different from Classic
        auto run = [&](float contrast) {
            AeolaDroneProcessor q; q.prepareToPlay(44100, 512); q.setCustomPlace(59.3293, 18.0686, "x"); setP(q, ID::radius, 80.0f);
            setP(q, ID::contrast, contrast); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::simseed, 31); setP(q, ID::maxac, 8);
            return render(q, 12.0); };
        auto cl = run(1.0f), cl0 = run(0.0f);
        auto diff = [](const Rendered& a, const Rendered& b) { double m = 0; for (size_t i = 44100 * 4; i < std::min(a.L.size(), b.L.size()); ++i) m = std::max(m, (double) std::fabs(a.L[i] - b.L[i])); return m; };
        bool finite = true; double peak = 0; for (size_t i = 0; i < cl.L.size(); ++i) { finite = finite && std::isfinite(cl.L[i]) && std::isfinite(cl.R[i]); peak = std::max(peak, (double) std::fabs(cl.L[i])); }
        CHECK(finite && peak > 0.001 && peak < 1.0, "classes over a custom place: finite audio, peak %.3f", peak);
        { auto rmsOf = [](const Rendered& r) { double e = 0; size_t n = 0; for (size_t i = 44100 * 4; i < r.L.size(); ++i) { e += (double) r.L[i] * r.L[i]; ++n; } return std::sqrt(e / std::max<size_t>(1, n)); };
          double a1 = rmsOf(cl), a2 = rmsOf(cl0); CHECK(a1 > a2 * 0.5 && a1 < a2 * 2.0, "level balance: contrast 100 %% rms %.4f vs 0 %% %.4f", a1, a2); }
        CHECK(diff(cl, cl0) > 0.005, "class contrast changes the sound (%.3f)", diff(cl, cl0));
        {   // audition: solo on a class that is not in the sky plays a virtual aircraft, so the voice can be tuned
            AeolaDroneProcessor q; q.prepareToPlay(44100, 512); q.setCustomPlace(-33.0, 20.0, "empty"); setP(q, ID::source, 4); setP(q, ID::simsync, 1); setP(q, ID::radius, 5.0f);
            setP(q, ID::simlevel, 0.2f); setP(q, ID::classId(kMil, 5), 1.0f); auto rr = render(q, 8.0);
            UiSnapshot ui; q.getUi(ui); double rm = 0; for (size_t i = 44100 * 4; i < rr.L.size(); ++i) rm += (double) rr.L[i] * rr.L[i];
            CHECK(ui.audition && rm > 1e-3, "solo with no aircraft present auditions a virtual aircraft (rms energy %.3f)", rm);
            setP(q, ID::classId(kMil, 5), 0.0f); setP(q, ID::classId(kMil, 6), 0.0f);
        }
        // the sky tick must keep up: place change is picked up by the feed
        AeolaDroneProcessor f; f.prepareToPlay(44100, 512); f.pollMessageThreadTasks(); auto k0 = f.makePlace().dataKey(); setP(f, ID::radius, 45.0f);
        CHECK(f.makePlace().dataKey() == k0, "scrubbing the radius inside a bucket does not refetch"); setP(f, ID::radius, 120.0f); f.pollMessageThreadTasks();
        CHECK(f.makePlace().dataKey() != k0, "a much larger radius needs a new data set");
    }

    printf("\n%s (%d failure%s)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
