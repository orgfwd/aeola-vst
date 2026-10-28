// Aeola — note log for MIDI export (audio thread -> message thread, lock-free).
// Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <deque>
#include <atomic>

namespace aeola
{
struct NoteEvent { double t; uint8_t note, vel, channel; bool on; };   // t = seconds since plugin start

class MidiLog
{
public:
    MidiLog() : fifo(1024) {}
    // audio thread
    void push(double t, int note, int vel, int channel, bool on)
    {
        int s1, n1, s2, n2; fifo.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 > 0) { buf[(size_t) s1] = { t, (uint8_t) note, (uint8_t) vel, (uint8_t) channel, on }; fifo.finishedWrite(1); }
    }
    // message thread
    void drain(double nowSec)
    {
        int s1, n1, s2, n2; int ready = fifo.getNumReady(); if (ready <= 0) { trim(nowSec); return; }
        fifo.prepareToRead(ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) events.push_back(buf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) events.push_back(buf[(size_t) (s2 + i)]);
        fifo.finishedRead(n1 + n2);
        trim(nowSec);
    }
    // Writes the last `minutes` of notes as a Standard MIDI File (120 bpm, 960 ticks per quarter).
    bool exportTo(const juce::File& file, double nowSec, double minutes = 30.0) const
    {
        juce::MidiMessageSequence seq;
        double t0 = std::max(0.0, nowSec - minutes * 60.0);
        double first = -1;
        for (auto& e : events) if (e.t >= t0) { if (first < 0) first = e.t; }
        if (first < 0) return false;
        seq.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
        std::deque<NoteEvent> open;
        for (auto& e : events)
        {
            if (e.t < t0) continue;
            double ticks = (e.t - first) * 1920.0;
            seq.addEvent(e.on ? juce::MidiMessage::noteOn(e.channel, e.note, (juce::uint8) e.vel)
                              : juce::MidiMessage::noteOff(e.channel, e.note), ticks);
        }
        seq.updateMatchedPairs();
        juce::MidiFile mf; mf.setTicksPerQuarterNote(960); mf.addTrack(seq);
        juce::FileOutputStream out(file); if (out.failedToOpen()) return false;
        out.setPosition(0); out.truncate();
        return mf.writeTo(out);
    }
    int numEvents() const { return (int) events.size(); }
    void clear() { events.clear(); }

private:
    void trim(double nowSec) { while (!events.empty() && events.front().t < nowSec - 45.0 * 60.0) events.pop_front(); }
    juce::AbstractFifo fifo; NoteEvent buf[1024];
    std::deque<NoteEvent> events;
};
} // namespace aeola
