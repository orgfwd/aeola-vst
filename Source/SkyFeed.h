// Aeola — live traffic feed (background thread), disk cache, dead reckoning.
// Data: adsb.lol (ODbL 1.0). Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include "Sky.h"
#include "AppInfo.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

namespace aeola
{
// Move aircraft to where they should be `dt` seconds after the snapshot (dead reckoning), recompute distance.
inline void extrapolate(Frame& f, double dtSec, const Place& ap)
{
    dtSec = std::max(0.0, std::min(dtSec, 900.0));
    for (int i = 0; i < f.count; ++i)
    {
        Aircraft& a = f.ac[i];
        double nm = a.gsKt * dtSec / 3600.0;
        offsetLatLon(a.lat, a.lon, a.trackDeg, nm, a.lat, a.lon);
        a.altFt = (float) std::max(0.0, (double) a.altFt + a.vrateFpm * dtSec / 60.0);
        double dN = (a.lat - ap.lat) * 60.0, dE = (a.lon - ap.lon) * 60.0 * std::cos(ap.lat * M_PI / 180.0);
        a.distNm = (float) std::sqrt(dN * dN + dE * dE);
    }
    // drop aircraft that have left the map area (the processor splits them into in range / approaching)
    const double area = mapAreaNm(ap.radiusNm);
    int w = 0; for (int i = 0; i < f.count; ++i) if (f.ac[i].distNm <= area) f.ac[w++] = f.ac[i];
    f.count = w; f.epoch += dtSec;
    // parked traffic only inside the listening radius too
    const double cx = std::cos(ap.lat * M_PI / 180.0); w = 0;
    for (int i = 0; i < f.nGround; ++i) { const auto& g = f.ground[i]; const double dN = (g.lat - ap.lat) * 60.0, dE = (g.lon - ap.lon) * 60.0 * cx; if (std::sqrt(dN * dN + dE * dE) <= ap.radiusNm) f.ground[w++] = g; }
    f.nGround = w;
}

// How the daily request quota is spent. adsb.lol answers with x-ratelimit-remaining (requests left) and x-ratelimit-reset (seconds until it refills).
// Spread what is left over the time that is left, keep a reserve for place changes and for other programs on the same network.
inline double budgetIntervalSec(double remaining, double secondsToReset, double minSec = 20.0, double maxSec = 300.0)
{
    if (remaining < 0.0) return 60.0;                                  // no quota information yet: a moderate pace
    const double spare = remaining - 30.0;
    if (spare <= 0.0) return std::max(60.0, secondsToReset);             // used up: wait for the refill
    return std::max(minSec, std::min(maxSec, secondsToReset / spare));
}
struct Quota { double limit = 500, remaining = -1, resetAt = 0; };
inline void readQuota(const juce::StringPairArray& h, double nowS, Quota& q)
{
    if (h.containsKey("x-ratelimit-remaining")) q.remaining = h.getValue("x-ratelimit-remaining", "-1").getDoubleValue();
    if (h.containsKey("x-ratelimit-reset")) { const double r = h.getValue("x-ratelimit-reset", "0").getDoubleValue(); if (r > 0) q.resetAt = nowS + r; }
    if (h.containsKey("x-ratelimit-limit")) { const double l = h.getValue("x-ratelimit-limit", "0").upToFirstOccurrenceOf(",", false, false).getDoubleValue(); if (l > 0) q.limit = l; }
}
inline juce::String limitMessage(double untilS, double nowS)
{
    if (untilS - nowS > 1800.0) return "Daily data limit reached at adsb.lol, resets " + juce::Time((juce::int64) (untilS * 1000.0)).formatted("%H:%M");
    return "adsb.lol asked us to wait (rate limit)";
}

class SkyFeed : private juce::Thread
{
public:
    SkyFeed() : juce::Thread("Aeola sky feed") { pruneCache(); }
    ~SkyFeed() override { signalThreadShouldExit(); notify(); stopThread(3000); }

    void start() { if (!isThreadRunning()) startThread(juce::Thread::Priority::low); }
    void setPlace(const Place& p)
    {
        if (p.dataKey() == placeKey.load()) return;
        { const juce::ScopedLock l(placeLock); place = p; place.radiusNm = p.fetchRadius(); }
        placeKey.store(p.dataKey()); changedAt.store((juce::int64) juce::Time::getMillisecondCounter()); notify();
    }
    void setPollSeconds(int s) { poll.store(juce::jlimit(10, 600, s)); }           // the fastest allowed pace; the quota budget usually makes it slower
    void setBackupEnabled(bool b) { backup.store(b); }                              // optional second source (adsb.fi) while adsb.lol's limit is reached
    const char* sourceName() const { return usingBackup.load() ? "adsb.fi" : "adsb.lol"; }
    double quotaRemaining() const { std::lock_guard<std::mutex> g(shared().m); return shared().quota.remaining; }
    double quotaResetEpoch() const { std::lock_guard<std::mutex> g(shared().m); return shared().quota.resetAt; }
    void setEnabled(bool e) { enabled.store(e); if (e) notify(); }
    void fetchNow() { notify(); }

    // audio-thread safe: copies the newest live snapshot, returns false when none / mid-write
    bool getLive(Frame& out, uint64_t key) const
    {
        if (!haveLive.load() || liveKey.load() != key) return false;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            uint32_t v1 = ver.load(std::memory_order_acquire);
            if (v1 & 1) continue;
            std::memcpy(&out, &slot, sizeof(Frame));
            std::atomic_thread_fence(std::memory_order_acquire);
            if (ver.load(std::memory_order_relaxed) == v1) return true;
        }
        return false;
    }
    bool getCached(Frame& out, uint64_t key) const
    {   // double buffered: the message thread fills the other buffer and then flips the index, so a read never sees a half-written frame
        if (!haveCache.load(std::memory_order_acquire) || cacheKey.load() != key) return false;
        std::memcpy(&out, &cache[cacheIdx.load(std::memory_order_acquire)], sizeof(Frame)); return true;
    }
    double lastSuccessEpoch() const { return lastOk.load(); }
    int status() const { return state.load(); }     // 0 idle, 1 ok, 2 error
    juce::String lastError() const { const juce::ScopedLock l(errLock); return err; }
    double retryAt() const { return retryEpoch.load(); }      // when the next attempt is due after an error (epoch seconds)
    std::function<void(const Frame&, uint64_t key)> onFrame;   // called on the feed thread after each success

    static juce::File cacheDir() { auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("AeolaDrone"); d.createDirectory(); return d; }
    static juce::File cacheFile(uint64_t key) { return cacheDir().getChildFile(juce::String("cache_") + juce::String::toHexString((juce::int64) key) + ".json"); }

    // message thread: load the persisted cache for the airport
    void loadCache(const Place& pl)
    {
        auto f = cacheFile(pl.dataKey());
        if (!f.existsAsFile()) { haveCache.store(false); return; }
        const int back = 1 - cacheIdx.load();
        if (parseResponse(f.loadFileAsString(), cache[back], [&] { Place q = pl; q.radiusNm = pl.fetchRadius(); return q; }()) && cache[back].count > 0)
        {
            cache[back].epoch = (double) f.getLastModificationTime().toMilliseconds() / 1000.0;
            haveCache.store(false); cacheIdx.store(back, std::memory_order_release); cacheKey.store(pl.centreKey()); haveCache.store(true, std::memory_order_release);
        }
        else haveCache.store(false);
    }

    // Parse an adsb.lol /v2 response into a Frame (also used for tests and cache files). Returns false on malformed input.
    static bool parseResponse(const juce::String& text, Frame& out, const Place& ap)
    {
        using namespace juce;
        auto root = JSON::parse(text);
        auto* obj = root.getDynamicObject();
        if (obj == nullptr) return false;
        auto acVar = obj->getProperty("ac");
        if (!acVar.isArray()) return false;
        out = Frame();
        double now = (double) obj->getProperty("now");
        std::vector<Aircraft> all, gnd;
        out.epoch = now > 1e11 ? now / 1000.0 : (now > 0 ? now : Time::currentTimeMillis() / 1000.0);
        for (auto& e : *acVar.getArray())
        {
            auto* o = e.getDynamicObject(); if (!o) continue;
            if (!o->hasProperty("lat") || !o->hasProperty("lon")) continue;
            Aircraft a;
            String hex = o->getProperty("hex").toString();
            a.id = (uint32_t) hex.getHexValue32() & 0xFFFFFFu;
            if (hex.startsWithChar('~')) a.id |= 0x1000000u;           // non-ICAO (TIS-B) address: keep it apart from real ones
            std::snprintf(a.callsign, sizeof a.callsign, "%s", o->getProperty("flight").toString().trim().substring(0, 8).toRawUTF8());
            std::snprintf(a.type, sizeof a.type, "%s", o->getProperty("t").toString().substring(0, 4).toRawUTF8());
            String cat = o->getProperty("category").toString();
            a.legacyCat = legacyCategory(a.type, a.callsign, cat.toRawUTF8()); a.category = categoryFromType(a.type, a.callsign, cat.toRawUTF8(), (int) o->getProperty("dbFlags"));
            a.lat = (double) o->getProperty("lat"); a.lon = (double) o->getProperty("lon");
            auto altVar = o->getProperty("alt_baro");
            const bool onGround = altVar.toString() == "ground";
            if (!onGround && altVar.isVoid()) altVar = o->getProperty("alt_geom");          // some aircraft only send GPS altitude
            if (!onGround && altVar.isVoid() && (double) o->getProperty("gs") > 50.0) continue;  // airborne with no altitude at all: cannot be placed
            a.altFt = onGround ? 0.0f : std::max(0.0f, (float) (double) altVar);
            a.gsKt = (float) (double) o->getProperty("gs");
            a.trackDeg = (float) (double) o->getProperty("track");
            a.vrateFpm = (float) (double) o->getProperty("baro_rate");
            // a position older than a minute is not shown by trackers either; a younger one is moved on to "now"
            const double seen = o->hasProperty("seen_pos") ? (double) o->getProperty("seen_pos") : 0.0;
            if (seen > 60.0) continue;
            if (seen > 0.5 && a.gsKt > 1.0f && !onGround) { offsetLatLon(a.lat, a.lon, a.trackDeg, a.gsKt * seen / 3600.0, a.lat, a.lon); a.altFt = (float) std::max(0.0, (double) a.altFt + a.vrateFpm * seen / 60.0); }
            double dN = (a.lat - ap.lat) * 60.0, dE = (a.lon - ap.lon) * 60.0 * std::cos(ap.lat * M_PI / 180.0);
            a.distNm = (float) std::sqrt(dN * dN + dE * dE);
            if (a.distNm > mapAreaNm(ap.radiusNm)) continue;
            if (onGround || (a.altFt < 200.0f && a.gsKt < 50.0f)) { gnd.push_back(a); continue; }      // on the ground: silent, but shown on the map
            // low but fast = just taking off or landing: that is the most audible moment at an airport, keep it
            all.push_back(a);
        }
        std::sort(all.begin(), all.end(), [](const Aircraft& x, const Aircraft& y) { return x.distNm < y.distNm; });
        for (auto& a : all) { if (out.count >= kMaxAc) break; out.ac[out.count++] = a; }       // the nearest kMaxAc aircraft
        std::sort(gnd.begin(), gnd.end(), [](const Aircraft& x, const Aircraft& y) { return x.distNm < y.distNm; });
        for (auto& a : gnd) { if (out.nGround >= kMaxGround) break; out.ground[out.nGround++] = a; }
        return true;
    }

private:
    void run() override
    {
        // first fetch shortly after start-up, then every `poll` seconds
        while (!threadShouldExit())
        {
            if (enabled.load())
            {
                // a place that is still being changed (zooming, typing) is not fetched yet; and never more often than every 6 s
                while (!threadShouldExit() && (juce::int64) juce::Time::getMillisecondCounter() - changedAt.load() < 1200) wait(300);
                const juce::int64 sinceLast = (juce::int64) juce::Time::getMillisecondCounter() - lastFetchMs;
                if (sinceLast < 6000) wait((int) (6000 - sinceLast));
                if (threadShouldExit()) break;
                lastFetchMs = (juce::int64) juce::Time::getMillisecondCounter(); fetchOnce();
            }
            if (!updateChecked) { updateChecked = true; UpdateCheck::runIfDue([this] { return threadShouldExit(); }); }       // once a day, off in Settings
            const double nowS = juce::Time::currentTimeMillis() / 1000.0;
            const double untilRetry = retryEpoch.load() - nowS;     // after an error: what the server asked for, or a minute
            double budget; { std::lock_guard<std::mutex> g(shared().m); budget = budgetIntervalSec(shared().quota.remaining, std::max(1.0, shared().quota.resetAt - nowS)); }
            if (usingBackup.load()) budget = 30.0;                  // adsb.fi allows 1 request a second; once every 30 s is polite
            wait((int) (1000.0 * std::max(std::max((double) poll.load(), budget), state.load() == 2 ? std::max(10.0, untilRetry) : 0.0)));
        }
    }

    void fetchOnce()
    {
        using namespace juce;
        Place ap; { const ScopedLock l(placeLock); ap = place; } const uint64_t key = ap.dataKey();
        const double nowS = Time::currentTimeMillis() / 1000.0;
        String body; double bodyEpoch = nowS; bool fromNet = false;
        double shareWin;
        {   // several Aeola instances share one request per place: within the budget interval nobody asks again (was 12 s before 1.12)
            std::lock_guard<std::mutex> g(shared().m);
            shareWin = std::max(12.0, std::min(270.0, budgetIntervalSec(shared().quota.remaining, std::max(1.0, shared().quota.resetAt - nowS)) - 5.0));
            auto it = shared().bodies.find(key);
            if (it != shared().bodies.end() && nowS - it->second.first < shareWin) { body = it->second.second; bodyEpoch = it->second.first; }
            if (body.isEmpty() && nowS < shared().blockedUntil && !backup.load()) { retryEpoch.store(shared().blockedUntil); setError(limitMessage(shared().blockedUntil, nowS)); return; }
        }
        if (body.isEmpty())
        {   // another program (the standalone app, a second host) or a restart fetched this place a moment ago: use its copy on disk
            auto cf = cacheFile(key); const double age = nowS - (double) cf.getLastModificationTime().toMilliseconds() / 1000.0;
            if (cf.existsAsFile() && age >= 0.0 && age < shareWin) { body = cf.loadFileAsString(); bodyEpoch = nowS - age; }
        }
        const bool lolBlocked = [&] { std::lock_guard<std::mutex> g(shared().m); return nowS < shared().blockedUntil; }();
        const bool useFi = body.isEmpty() && lolBlocked && backup.load();     // adsb.lol's quota is used up and the user allowed the backup
        usingBackup.store(useFi);
        if (body.isEmpty())
        {
            const int dist = std::max(1, std::min(250, (int) std::ceil(ap.radiusNm * 1.2)));
            URL url(useFi ? "https://opendata.adsb.fi/api/v3/lat/" + String(ap.lat, 4) + "/lon/" + String(ap.lon, 4) + "/dist/" + String(dist)
                          : "https://api.adsb.lol/v2/lat/" + String(ap.lat, 4) + "/lon/" + String(ap.lon, 4) + "/dist/" + String(dist));
            int status = 0; StringPairArray headers;
            auto opts = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs(8000).withStatusCode(&status).withResponseHeaders(&headers)
                            .withExtraHeaders("User-Agent: Aeola/" JucePlugin_VersionString " (+https://github.com/orgfwd/aeola-vst)\r\nAccept: application/json\r\n");
            std::unique_ptr<InputStream> in = url.createInputStream(opts);
            if (in == nullptr) { retryEpoch.store(nowS + 60.0); setError("no connection"); return; }
            if (!useFi) countRequest();
            fromNet = true;
            if (!useFi) { std::lock_guard<std::mutex> g(shared().m); readQuota(headers, nowS, shared().quota); }
            if (status == 429 || status == 503)
            {
                double wait;
                { std::lock_guard<std::mutex> g(shared().m);
                  const double rst = shared().quota.resetAt - nowS;               // the server says when the quota refills (no Retry-After is sent)
                  wait = status == 429 && !useFi && rst > 0 ? (rst > 3600.0 ? rst : std::max(10.0, rst + 1.0)) : std::max(60.0, headers.getValue("Retry-After", "120").getDoubleValue());
                  if (!useFi) { shared().quota.remaining = 0; shared().blockedUntil = nowS + wait; } }
                retryEpoch.store(nowS + wait); setError(useFi ? juce::String("adsb.fi asked us to wait (rate limit)") : limitMessage(nowS + wait, nowS)); return;
            }
            if (status >= 400) { retryEpoch.store(nowS + 60.0); setError("adsb.lol answered " + String(status)); return; }
            body = in->readEntireStreamAsString();
            if (body.isEmpty() || threadShouldExit()) { retryEpoch.store(nowS + 60.0); setError("empty response"); return; }
            std::lock_guard<std::mutex> g(shared().m); shared().bodies[key] = { nowS, body };
            for (auto it = shared().bodies.begin(); it != shared().bodies.end();) { if (nowS - it->second.first > 300.0) it = shared().bodies.erase(it); else ++it; }
        }
        Frame* f = new Frame();
        if (!parseResponse(body, *f, ap)) { delete f; retryEpoch.store(nowS + 60.0); setError("unexpected response"); return; }
        f->epoch = bodyEpoch;                               // when the positions were fetched (a shared copy can be a few minutes old; it is flown on)
        // publish (seqlock)
        ver.fetch_add(1, std::memory_order_acq_rel);
        std::memcpy(&slot, f, sizeof(Frame));
        liveKey.store(ap.centreKey());
        ver.fetch_add(1, std::memory_order_release);
        haveLive.store(true); lastOk.store(f->epoch); state.store(1); retryEpoch.store(0.0);
        if (!useFi)
        {   // that was one of the last requests of the day: say so now and wait for the refill instead of running into a 429
            std::lock_guard<std::mutex> g(shared().m);
            if (shared().quota.remaining >= 0.0 && shared().quota.remaining <= 1.0 && shared().quota.resetAt > nowS + 5.0) { shared().blockedUntil = shared().quota.resetAt; retryEpoch.store(shared().quota.resetAt); setError(limitMessage(shared().quota.resetAt, nowS)); }
        }
        if (key == placeKey.load())
        {   // the disk copy is only for starting up offline: once a minute is enough
            if (fromNet && (nowS - lastCacheWrite > 60.0 || key != lastCacheKey)) { cacheFile(key).replaceWithText(body); lastCacheWrite = nowS; lastCacheKey = key; }
            if (onFrame) onFrame(*f, key);
        }
        delete f;
    }
    void setError(const juce::String& e) { state.store(2); const juce::ScopedLock l(errLock); err = e; }
public:
    // requests to adsb.lol made from this Mac today (UTC), all Aeolas and programs together: a small file next to the cache
    static juce::File requestsFile() { return cacheDir().getChildFile("requests.txt"); }
    static juce::String todayUtc() { return juce::Time::getCurrentTime().toISO8601(false).substring(0, 10); }
    static int requestsToday()
    {
        auto t = juce::StringArray::fromTokens(requestsFile().loadFileAsString(), " ", "");
        return t.size() == 2 && t[0] == todayUtc() ? t[1].getIntValue() : 0;
    }
private:
    static void countRequest()
    {
        static std::mutex m; std::lock_guard<std::mutex> g(m);
        requestsFile().replaceWithText(todayUtc() + " " + juce::String(requestsToday() + 1));
    }

    struct Shared { std::mutex m; std::map<uint64_t, std::pair<double, juce::String>> bodies; double blockedUntil = 0; Quota quota; };
    static Shared& shared() { static Shared s; return s; }
    static void pruneCache() { for (auto& f : cacheDir().findChildFiles(juce::File::findFiles, false, "cache_*.json")) if (f.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days(14)) f.deleteFile(); }
    bool updateChecked = false; double lastCacheWrite = 0; uint64_t lastCacheKey = 0; std::atomic<double> retryEpoch{ 0.0 };
    std::atomic<int> cacheIdx{ 0 };
    std::atomic<int> poll{ 20 }, state{ 0 };
    std::atomic<uint64_t> placeKey{ 0 }, liveKey{ 1 }, cacheKey{ 1 }; std::atomic<juce::int64> changedAt{ 0 }; juce::int64 lastFetchMs = 0;
    Place place; mutable juce::CriticalSection placeLock;
    std::atomic<bool> enabled{ true }, haveLive{ false }, haveCache{ false }, backup{ false }, usingBackup{ false };
    std::atomic<double> lastOk{ 0.0 };
    mutable std::atomic<uint32_t> ver{ 0 };
    Frame slot, cache[2];
    mutable juce::CriticalSection errLock; juce::String err;
};
} // namespace aeola
