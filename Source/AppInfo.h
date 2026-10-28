// Aeola - links, update check, diagnostics. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
// Everything here is plain juce_core, so it can be tested without a window.
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

namespace aeola
{
// One place for every address the plugin shows or contacts. Change them here only.
namespace links
{
    static const char* const feedbackEmail = "aeola@skepp.se";
    static const char* const donate        = "https://ko-fi.com/aeola";
    static const char* const website       = "https://orgfwd.github.io/aeola-vst/";
    static const char* const download      = "https://github.com/orgfwd/aeola-vst/releases/latest";
    static const char* const source        = "https://github.com/orgfwd/aeola-vst";
    static const char* const versionFile   = "https://orgfwd.github.io/aeola-vst/version.json";     // {"version":"1.9.1","notes":"one line"}
}

// "1.10.0" is newer than "1.9.2": compare number by number.
inline bool isNewerVersion(const juce::String& candidate, const juce::String& current)
{
    auto a = juce::StringArray::fromTokens(candidate.trim(), ".", ""), b = juce::StringArray::fromTokens(current.trim(), ".", "");
    for (int i = 0; i < std::max(a.size(), b.size()); ++i)
    {
        const int x = i < a.size() ? a[i].getIntValue() : 0, y = i < b.size() ? b[i].getIntValue() : 0;
        if (x != y) return x > y;
    }
    return false;
}

struct UpdateInfo { juce::String version, notes; bool valid = false; };
inline UpdateInfo parseVersionFile(const juce::String& text)
{
    UpdateInfo u; const juce::var v = juce::JSON::parse(text); auto* o = v.getDynamicObject(); if (!o) return u;
    u.version = o->getProperty("version").toString().trim(); u.notes = o->getProperty("notes").toString().substring(0, 160);
    u.valid = u.version.isNotEmpty() && u.version.containsOnly("0123456789.") && u.version.containsChar('.');
    return u;
}

// Once a day at most, and only if the user has not switched it off, one small file is fetched from the project page (no identifier is sent beyond the
// user agent). The answer is kept in a settings file shared by all instances, so opening ten instances makes at most one request.
class UpdateCheck
{
public:
    static juce::File& fileOverride() { static juce::File f; return f; }          // tests keep their own settings file
    static juce::File file() { if (fileOverride() != juce::File()) return fileOverride(); auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AeolaDrone"); d.createDirectory(); return d.getChildFile("update.json"); }
    static bool enabled() { return read().enabled; }
    static void setEnabled(bool e) { auto s = read(); s.enabled = e; write(s); }
    // the newer version we know about (from the last successful check), or an invalid UpdateInfo
    static UpdateInfo available()
    {
        auto s = read(); UpdateInfo u; if (!s.enabled || !isNewerVersion(s.version, JucePlugin_VersionString)) return u;
        u.version = s.version; u.notes = s.notes; u.valid = true; return u;
    }
    // call from a background thread that is joined before the plugin unloads
    static void runIfDue(const std::function<bool()>& shouldExit, const juce::String& address = links::versionFile)
    {
        auto s = read(); const double now = (double) juce::Time::currentTimeMillis() / 1000.0;
        if (!s.enabled || now - s.lastCheck < 20.0 * 3600.0 || shouldExit()) return;
        s.lastCheck = now; write(s);                                   // also after a failure: one attempt a day, not one per start
        int status = 0;
        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(4000).withStatusCode(&status)
                        .withExtraHeaders("User-Agent: Aeola/" JucePlugin_VersionString "\r\n");
        juce::URL u0(address); std::unique_ptr<juce::InputStream> in = u0.createInputStream(opts);
        if (!in || (status != 200 && !u0.isLocalFile()) || shouldExit()) return;
        auto u = parseVersionFile(in->readEntireStreamAsString());
        if (!u.valid) return;
        s.version = u.version; s.notes = u.notes; write(s);
    }
private:
    struct S { bool enabled = true; double lastCheck = 0; juce::String version, notes; };
    static S read()
    {
        std::lock_guard<std::mutex> g(m()); S s; const juce::var v = juce::JSON::parse(file()); auto* o = v.getDynamicObject(); if (!o) return s;
        if (o->hasProperty("enabled")) s.enabled = (bool) o->getProperty("enabled");
        s.lastCheck = (double) o->getProperty("lastCheck"); s.version = o->getProperty("version").toString(); s.notes = o->getProperty("notes").toString(); return s;
    }
    static void write(const S& s)
    {
        std::lock_guard<std::mutex> g(m()); auto* o = new juce::DynamicObject(); o->setProperty("enabled", s.enabled); o->setProperty("lastCheck", s.lastCheck); o->setProperty("version", s.version); o->setProperty("notes", s.notes);
        file().replaceWithText(juce::JSON::toString(juce::var(o)));
    }
    static std::mutex& m() { static std::mutex mu; return mu; }
};

// The text a tester pastes into a bug report. No location (a custom place could be a home address), no user name, nothing about the project.
struct Diagnostics
{
    juce::String format, host, place, mode, shown, feedName, feedErr; int radiusNm = 0, aircraft = 0, sounding = 0, maxAircraft = 0, voices = 0; double quotaLeft = -1, quotaResetEpoch = 0, ageSec = 0, sampleRate = 0; int requestsToday = -1; bool backup = false, custom = false; float uiScale = 1.0f;
    juce::String text() const
    {
        juce::String t; auto line = [&](const juce::String& k, const juce::String& v) { t << k.paddedRight(' ', 14) << v << "\n"; };
        line("Aeola", JucePlugin_VersionString); line("System", juce::SystemStats::getOperatingSystemName() + ", " + juce::SystemStats::getDeviceDescription() + " (" + (juce::SystemStats::isOperatingSystem64Bit() ? "64-bit" : "32-bit") + ")");
        line("Format / host", format + " / " + (host.isEmpty() ? "unknown" : host)); if (sampleRate > 0) line("Sample rate", juce::String(sampleRate, 0) + " Hz");
        line("Place", (custom ? juce::String("custom place") : place) + ", radius " + juce::String(radiusNm) + " nm");
        line("Sky source", mode + " (now showing: " + shown + ")");
        line("Aircraft", juce::String(aircraft) + " in range, " + juce::String(sounding) + " sounding (limit " + juce::String(maxAircraft) + ", voices " + juce::String(voices) + ")");
        line("Data", feedName + (ageSec > 0 ? ", " + juce::String((int) std::lround(ageSec)) + " s old" : juce::String()) + (backup ? ", backup allowed" : juce::String()));
        if (requestsToday >= 0) line("Requests today", juce::String(requestsToday) + " from this Mac (UTC day, all Aeolas)");
        if (quotaLeft >= 0) line("Quota left", juce::String((int) quotaLeft) + " requests" + (quotaResetEpoch > 0 ? ", refills " + juce::Time((juce::int64) (quotaResetEpoch * 1000.0)).formatted("%H:%M") : juce::String()));
        if (feedErr.isNotEmpty()) line("Last problem", feedErr);
        line("Interface", juce::String((int) std::lround(uiScale * 100.0f)) + " %");
        return t;
    }
};

inline juce::URL feedbackUrl(const juce::String& diagnostics)
{
    return juce::URL(juce::String("mailto:") + links::feedbackEmail)
        .withParameter("subject", juce::String("Aeola ") + JucePlugin_VersionString + " feedback")
        .withParameter("body", "What did you hear, what did you expect?\n\n\n\n--- details (delete if you like) ---\n" + diagnostics);
}
} // namespace aeola
