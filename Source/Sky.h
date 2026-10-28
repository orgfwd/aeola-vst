// Aeola — sky model: aircraft, airports, deterministic simulator and the musical mapper.
// Pure C++ (no JUCE) so it can be unit-tested. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include "Tuning.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>

namespace aeola
{
constexpr int kMaxAc = 64;
constexpr int kMaxGround = 12;
constexpr uint32_t kAcVoiceBit = 0x40000000u;     // voice id of an aircraft = kAcVoiceBit | its id (anchor and fill voices use small ids)
// Altitude -> 0..1 position in the pitch range. Reaches the top at 42,000 ft, and keeps cruise levels (30-41,000 ft) apart instead of
// collapsing them onto one note. Shared by the mapper and the Sound view.
inline double altFraction(double ft) { return std::pow(std::min(1.0, std::max(0.0, ft / 42000.0)), 0.7); }

// Aircraft classes. 0-5 are the v1.2 categories (kept so old projects and the Classic mapping stay identical).
enum AcCategory { kLight = 0, kSmall, kLarge, kHeavy, kHeli, kCargo, kBiz, kProp, kMil, kNumClasses };
inline const char* className(int c)
{
    static const char* n[] = { "Light / GA", "Small", "Narrowbody", "Heavy", "Helicopter", "Freight", "Business jet", "Turboprop", "Military" };
    return n[std::max(0, std::min(kNumClasses - 1, c))];
}

struct Aircraft
{
    uint32_t id = 0;
    char callsign[10] = {};
    char type[6] = {};
    int category = kLarge;            // class used by the Classes mapping
    int legacyCat = kLarge;           // v1.2 category (Classic mapping)
    double lat = 0, lon = 0;
    float altFt = 0, gsKt = 0, trackDeg = 0, vrateFpm = 0, distNm = 0;
};

struct Frame
{
    double epoch = 0;          // seconds since 1970 for which positions are valid
    int count = 0;
    Aircraft ac[kMaxAc];
    int nGround = 0; Aircraft ground[kMaxGround];      // parked / taxiing traffic: shown on the map, never sounded
};

struct Airport { const char* icao; const char* name; double lat, lon; double radiusNm; };
// One line on what to expect there (a hint to explore, not a statistic); index = airport list index.
inline const char* airportNote(int i)
{
    static const char* n[] = {
        "Nordic hub: steady airliners with long quiet gaps.", "A big cargo hub: parcel freighters through the night.", "Non-stop heavy long-haul jets, wave after wave.",
        "A dense mix of airliners, close together.", "Heavy long-haul jets plus a lot of cargo.", "A medium hub: regular airliners, calm skies.",
        "Nordic hub: regional turboprops mixed with jets.", "Hub for Asia routes: long-haul jets over the Baltic.", "Transatlantic heavies, and helicopters around the city.",
        "Pacific long-haul heavies and busy helicopters.", "One of the world's busiest: constant, orderly traffic.", "Long-haul from across the Pacific, quiet at night.",
        "A huge hub: heavies and freighters all day.", "A busy hub: airliners in tidy streams.", "Spain's hub: Atlantic routes and steady narrowbodies.",
        "Alpine hub: orderly airliners, tight approaches.", "Small and quiet: few aircraft, easy to hear each one.", "A giant US hub: constant domestic jets, and freight.",
        "Super-heavy A380s and long-haul, around the clock.", "Tropical hub: heavy long-haul and regional jets." };
    return (i >= 0 && i < (int) (sizeof n / sizeof n[0])) ? n[i] : "Your own spot: whatever happens to fly over it.";
}
// Airport codes shown to people (IATA); the ICAO code stays the internal identifier and is what projects store.
inline const char* iataOf(const char* icao)
{
    static const char* t[][2] = { { "ESSA", "ARN" }, { "EDDK", "CGN" }, { "EGLL", "LHR" }, { "EHAM", "AMS" }, { "EDDF", "FRA" }, { "EKCH", "CPH" }, { "ENGM", "OSL" }, { "EFHK", "HEL" },
        { "KJFK", "JFK" }, { "KLAX", "LAX" }, { "RJTT", "HND" }, { "YSSY", "SYD" }, { "LFPG", "CDG" }, { "EDDM", "MUC" }, { "LEMD", "MAD" }, { "LSZH", "ZRH" }, { "ESGG", "GOT" },
        { "KORD", "ORD" }, { "OMDB", "DXB" }, { "WSSS", "SIN" } };
    for (auto& e : t) if (!std::strcmp(e[0], icao)) return e[1];
    return icao;
}
inline const Airport* airports(int& n)
{
    // New entries are only ever appended: the list index is stored in projects.
    static const Airport a[] = {
        { "ESSA", "Stockholm Arlanda",   59.6519,  17.9186, 40 },
        { "EDDK", "Cologne Bonn",        50.8659,   7.1427, 40 },
        { "EGLL", "London Heathrow",     51.4700,  -0.4543, 40 },
        { "EHAM", "Amsterdam Schiphol",  52.3105,   4.7683, 40 },
        { "EDDF", "Frankfurt",           50.0379,   8.5622, 40 },
        { "EKCH", "Copenhagen",          55.6180,  12.6508, 40 },
        { "ENGM", "Oslo Gardermoen",     60.1976,  11.1004, 40 },
        { "EFHK", "Helsinki-Vantaa",     60.3172,  24.9633, 40 },
        { "KJFK", "New York JFK",        40.6413, -73.7781, 40 },
        { "KLAX", "Los Angeles",         33.9416,-118.4085, 40 },
        { "RJTT", "Tokyo Haneda",        35.5494, 139.7798, 40 },
        { "YSSY", "Sydney",             -33.9399, 151.1753, 40 },
        { "LFPG", "Paris Charles de Gaulle", 49.0097,  2.5479, 40 },
        { "EDDM", "Munich",              48.3538,  11.7861, 40 },
        { "LEMD", "Madrid Barajas",      40.4719,  -3.5626, 40 },
        { "LSZH", "Zurich",              47.4647,   8.5492, 40 },
        { "ESGG", "Gothenburg Landvetter", 57.6628, 12.2798, 40 },
        { "KORD", "Chicago O'Hare",      41.9742, -87.9073, 40 },
        { "OMDB", "Dubai",               25.2532,  55.3657, 40 },
        { "WSSS", "Singapore Changi",     1.3644, 103.9915, 40 },
    };
    n = (int) (sizeof a / sizeof a[0]);
    return a;
}

// Where the listener is: an airport from the list or any point on earth, plus the radius that is listened to.
struct Place
{
    double lat = 59.6519, lon = 17.9186, radiusNm = 40;
    char name[56] = "Stockholm Arlanda"; char code[8] = "ESSA";
    bool isAirport = true;
    // Live data is fetched for a radius bucket, not for every radius the user scrubs through: the listening radius is applied locally.
    double fetchRadius() const { static const double b[] = { 25, 50, 100, 150, 250 }; for (double x : b) if (radiusNm <= x) return x; return 250; }
    uint64_t centreKey() const { Place c = *this; c.radiusNm = 1.0; return c.key(); }      // same spot, any radius: live data stays usable while the radius is scrubbed
    uint64_t dataKey() const { Place c = *this; c.radiusNm = fetchRadius(); return c.key(); }
    uint64_t key() const    // identifies the data set (cache file, live data match): position to ~1 km, radius to 1 nm
    {
        long long a = (long long) std::llround(lat * 100.0), o = (long long) std::llround(lon * 100.0), r = (long long) std::llround(radiusNm);
        return (uint64_t) ((a + 9000) * 36001LL + (o + 18000)) * 1000ULL + (uint64_t) std::max(1LL, std::min(999LL, r));
    }
};
inline Place placeFromAirport(int idx, double radiusNm)
{
    int n; const Airport* a = airports(n); const Airport& ap = a[((idx % n) + n) % n];
    Place p; p.lat = ap.lat; p.lon = ap.lon; p.radiusNm = radiusNm; p.isAirport = true;
    std::snprintf(p.name, sizeof p.name, "%s", ap.name); std::snprintf(p.code, sizeof p.code, "%s", ap.icao);
    return p;
}
inline Airport airportOf(const Place& p) { return { p.code, p.name, p.lat, p.lon, p.radiusNm }; }   // views the Place as an Airport (pointers into p)

inline uint64_t mix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull; return x ^ (x >> 31);
}
inline double hash01(uint64_t a, uint64_t b = 0, uint64_t c = 0) { return (double) (mix64(a * 0x100000001B3ull ^ mix64(b * 31 + mix64(c))) >> 11) / 9007199254740992.0; }

// v1.2 categories (the base of the class detection; the plain mapping used by the Guard tests reads them)
inline int legacyCategory(const char* type, const char* callsign, const char* adsbCat)
{
    static const char* cargo[] = { "UPS", "FDX", "GTI", "GEC", "CLX", "CLI", "CKS", "ABW", "BOX", "BCS", "CAO", "SQC", "MPH", "DHK", "ICL", "DHX", "DAE", "SWN", "NCA", "ABR",
                                   "AHK", "TAY", "QAC", "CKK", "AJT", "PAC", "ATN", "MSX", "BDA", "LCO", "NPT", "RCF", "WGN", "FRH" };
    if (adsbCat && adsbCat[0] == 'A' && adsbCat[1] == '7') return kHeli;
    if (type[0] == 'E' && type[1] == 'C') return kHeli;
    {   static const char* heli[] = { "R22", "R44", "R66", "AS3", "AS5", "AS6", "H12", "H13", "H14", "H15", "H16", "H17", "A109", "A119", "A129", "A139", "A149", "A169", "A189",
                                      "AW", "B06", "B05", "B47", "B212", "B412", "B427", "B429", "B407", "B505", "S76", "S92", "S61", "H60", "UH1", "NH90", "MI8", "MI1", "CH47", "EH10", "BK17", "G2CA" };
        for (auto h : heli) if (!std::strncmp(type, h, std::strlen(h))) return kHeli; }
    for (auto c : cargo) if (!std::strncmp(callsign, c, 3)) return kCargo;
    static const char* heavy[] = { "B74", "B77", "B78", "A33", "A34", "A35", "A38", "B76", "MD1", "A30", "A31", "C5" };
    for (auto h : heavy) if (!std::strncmp(type, h, 3)) return kHeavy;
    if (adsbCat && adsbCat[0] == 'A' && adsbCat[1] == '5') return kHeavy;
    if (adsbCat && adsbCat[0] == 'A' && adsbCat[1] == '1') return kLight;
    if (adsbCat && adsbCat[0] == 'A' && adsbCat[1] == '2') return kSmall;
    return kLarge;
}

// v1.3 classes: adds business jets, turboprops and military; everything else as above.
inline int categoryFromType(const char* type, const char* callsign, const char* adsbCat, int dbFlags = 0)
{
    int legacy = legacyCategory(type, callsign, adsbCat);
    if (dbFlags & 1) return kMil;                                         // adsb.lol database flag: military
    if (legacy == kHeli || legacy == kCargo || legacy == kHeavy) return legacy;
    static const char* biz[] = { "C25", "C51", "C52", "C55", "C56", "C68", "C70", "C75", "CL3", "CL6", "GLF", "G28", "GAL", "GL5", "GL7", "GLE", "LJ3", "LJ4", "LJ6", "LJ7",
                                 "E50", "E55", "FA5", "FA6", "FA7", "FA8", "F90", "F2T", "H25", "PRM", "PC24", "SF50", "EA50", "HDJT", "ASTR", "BE40", "LEGA", "FA20", "C680", "CL35" };
    static const char* prop[] = { "AT4", "AT5", "AT7", "DH8", "SF3", "B190", "PC12", "C208", "E120", "JS3", "JS4", "D328", "F50", "F27", "P180", "TBM", "BE20", "BE30", "BE9", "B350", "DHC", "SW4", "SH6" };
    char t4[5] = {}; std::strncpy(t4, type, 4);
    for (auto b : biz)  { size_t l = std::strlen(b); if (!std::strncmp(t4, b, l)) return kBiz; }
    for (auto q : prop) { size_t l = std::strlen(q); if (!std::strncmp(t4, q, l)) return kProp; }
    return legacy;
}

// ---- deterministic traffic simulator -------------------------------------------------------
// The same (airport, seed, time) always returns the same sky. Arrivals follow a ~3 degree glide slope,
// departures climb out; density follows a daily curve for the airport's local time.
inline double trafficDensity(double epoch, double lonDeg)
{
    double hour = std::fmod(epoch / 3600.0 + lonDeg / 15.0, 24.0); if (hour < 0) hour += 24.0;
    double morning = std::exp(-std::pow((hour - 8.0) / 2.5, 2)), eve = std::exp(-std::pow((hour - 17.5) / 3.0, 2));
    double mid = std::exp(-std::pow((hour - 13.0) / 3.5, 2)) * 0.6;
    return std::min(1.0, 0.12 + 0.75 * morning + 0.8 * eve + mid);
}
inline double nightFactor(double epoch, double lonDeg)
{
    double hour = std::fmod(epoch / 3600.0 + lonDeg / 15.0, 24.0); if (hour < 0) hour += 24.0;
    return (hour < 5.0 || hour > 22.5) ? 1.0 : 0.0;
}

inline void offsetLatLon(double lat, double lon, double bearingDeg, double nm, double& outLat, double& outLon)
{
    double b = bearingDeg * M_PI / 180.0;
    outLat = lat + nm * std::cos(b) / 60.0;
    outLon = lon + nm * std::sin(b) / (60.0 * std::cos(lat * M_PI / 180.0));
}

inline void simulateSky(const Airport& ap, uint32_t seed, double epoch, double activity, Frame& out, bool classMix = true)
{
    out.epoch = epoch; out.count = 0;
    const double slot = 70.0, life = 520.0;                 // one potential flight per 70 s, ~8.7 min inside the circle
    const long long kEnd = (long long) std::floor(epoch / slot), kStart = kEnd - (long long) (life / slot) - 1;
    const double dens = trafficDensity(epoch, ap.lon) * activity;
    const double night = nightFactor(epoch, ap.lon);
    const double rwy = 360.0 * hash01(seed, 7);              // approach bearing for the day (seeded)
    static const char* airlines[] = { "SAS", "KLM", "BAW", "DLH", "AFR", "NOZ", "FIN", "RYR", "EZY", "THY", "UAE", "LOT" };
    static const char* types[] = { "A320", "B738", "A321", "E190", "AT76", "B38M", "A20N", "CRJ9", "A333", "B77W", "B788", "A359" };
    static const char* cargoTypes[] = { "B744", "B77L", "A306", "B763" };
    static const char* cargoLines[] = { "UPS", "FDX", "GTI", "CLX" };
    static const char* bizTypes[] = { "C56X", "GLF5", "E55P", "C25B", "CL60", "F2TH" };
    static const char* propTypes[] = { "AT76", "DH8D", "AT75", "SF34" };
    static const char* milTypes[] = { "C130", "A400", "F16", "C17" };
    for (long long k = kStart; k <= kEnd && out.count < kMaxAc; ++k)
    {
        double born = (double) k * slot + hash01(seed, (uint64_t) k, 1) * slot;
        double age = epoch - born;
        if (age < 0 || age > life) continue;
        double isCargo = hash01(seed, (uint64_t) k, 2) < (0.08 + 0.45 * night);
        if (hash01(seed, (uint64_t) k, 3) > dens * (isCargo ? 0.9 : 1.0) * 1.15) continue;
        bool arrival = hash01(seed, (uint64_t) k, 4) < 0.5;
        double u = age / life;
        double bearing = arrival ? rwy + (hash01(seed, (uint64_t) k, 5) - 0.5) * 50.0
                                 : rwy + 180.0 + (hash01(seed, (uint64_t) k, 5) - 0.5) * 90.0;
        double d = arrival ? ap.radiusNm * (1.0 - u) : ap.radiusNm * u;
        double alt = arrival ? std::min(11000.0, 318.0 * d + 200.0) : std::min(16000.0, 420.0 * d + 500.0);
        double gs = arrival ? 140.0 + 130.0 * (d / ap.radiusNm) : 170.0 + 190.0 * u;
        Aircraft& a = out.ac[out.count++];
        a = Aircraft();
        a.id = (uint32_t) (mix64(seed * 977 + (uint64_t) k) & 0xFFFFFF) | 0x1000000u;
        int ai = (int) (hash01(seed, (uint64_t) k, 6) * 12) % 12, ti = (int) (hash01(seed, (uint64_t) k, 8) * 12) % 12;
        if (isCargo) { std::snprintf(a.callsign, sizeof a.callsign, "%s%d", cargoLines[ai % 4], 100 + (int) (hash01(seed, (uint64_t) k, 9) * 899)); std::snprintf(a.type, sizeof a.type, "%s", cargoTypes[ti % 4]); }
        else         { std::snprintf(a.callsign, sizeof a.callsign, "%s%d", airlines[ai], 100 + (int) (hash01(seed, (uint64_t) k, 9) * 899)); std::snprintf(a.type, sizeof a.type, "%s", types[ti]); }
        a.legacyCat = categoryFromType(a.type, a.callsign, nullptr) == kCargo ? kCargo : legacyCategory(a.type, a.callsign, nullptr);
        a.category = categoryFromType(a.type, a.callsign, nullptr);
        {   // a realistic mix of other classes (the sim must exercise every class voice)
            double h = hash01(seed, (uint64_t) k, 10), pick = hash01(seed, (uint64_t) k, 11);
            if (classMix && !isCargo && h < 0.30)
            {
                if (h < 0.025)      { a.category = kMil;   a.legacyCat = kLarge; std::snprintf(a.type, sizeof a.type, "%s", milTypes[(int) (pick * 4) % 4]); std::snprintf(a.callsign, sizeof a.callsign, "RCH%d", 100 + (int) (pick * 800)); }
                else if (h < 0.05)  { a.category = kHeli;  a.legacyCat = kHeli;  std::snprintf(a.type, sizeof a.type, "EC35"); std::snprintf(a.callsign, sizeof a.callsign, "SAR%d", 10 + (int) (pick * 80)); }
                else if (h < 0.10)  { a.category = kLight; a.legacyCat = kLight; std::snprintf(a.type, sizeof a.type, "C172"); std::snprintf(a.callsign, sizeof a.callsign, "SE-K%c%c", 'A' + (int) (pick * 26), 'A' + (int) (pick * 677) % 26); }
                else if (h < 0.19)  { a.category = kBiz;   a.legacyCat = kSmall; std::snprintf(a.type, sizeof a.type, "%s", bizTypes[(int) (pick * 6) % 6]); std::snprintf(a.callsign, sizeof a.callsign, "N%d%c%c", 100 + (int) (pick * 800), 'A' + (int) (pick * 26), 'A' + (int) (pick * 677) % 26); }
                else                { a.category = kProp;  a.legacyCat = kLarge; std::snprintf(a.type, sizeof a.type, "%s", propTypes[(int) (pick * 4) % 4]); }
            }
        }
        offsetLatLon(ap.lat, ap.lon, bearing, d, a.lat, a.lon);
        a.altFt = (float) alt; a.gsKt = (float) gs; a.distNm = (float) d;
        a.trackDeg = (float) std::fmod(arrival ? bearing + 180.0 : bearing, 360.0);
        a.vrateFpm = arrival ? -(float) (gs * 318.0 / 60.0 * 0.0 + 700.0) : 1800.0f;
    }
}

// Traffic over an arbitrary point that is not an airport: mostly en-route overflights (straight lines across the circle at cruise or
// climbing/descending), a few low aircraft. Same contract as simulateSky: the same (place, seed, time) always gives the same sky.
inline void simulateOverflights(const Place& pl, uint32_t seed, double epoch, double activity, Frame& out)
{
    out.epoch = epoch; out.count = 0;
    const double R = std::max(5.0, pl.radiusNm);
    const double perHour = 16.0 * std::pow(R / 40.0, 1.3) * activity * (0.35 + trafficDensity(epoch, pl.lon));
    const double slot = 3600.0 / std::max(2.0, std::min(600.0, perHour));
    const double maxLife = 2.0 * R / 230.0 * 3600.0;                     // slowest crossing of the full diameter
    const long long kEnd = (long long) std::floor(epoch / slot), kStart = kEnd - (long long) (maxLife / slot) - 1;
    static const char* airlines[] = { "SAS", "KLM", "BAW", "DLH", "AFR", "NOZ", "FIN", "RYR", "EZY", "THY", "UAE", "LOT" };
    static const char* types[] = { "A320", "B738", "A321", "E190", "B38M", "A20N", "A333", "B77W", "B788", "A359" };
    static const char* cargoTypes[] = { "B744", "B77L", "A306", "B763" };
    static const char* cargoLines[] = { "UPS", "FDX", "GTI", "CLX" };
    static const char* bizTypes[] = { "C56X", "GLF5", "E55P", "C25B", "CL60", "F2TH" };
    static const char* propTypes[] = { "AT76", "DH8D", "AT75", "SF34" };
    const double night = nightFactor(epoch, pl.lon);
    const double mainBearing = 360.0 * hash01(seed, 21);                 // the busy airway of the day
    // candidate list (nearest to the centre first would need a sort; the mapper does that): keep the newest kMaxAc
    for (long long k = kEnd; k >= kStart && out.count < kMaxAc; --k)
    {
        double born = (double) k * slot + hash01(seed, (uint64_t) k, 1) * slot;
        double kind = hash01(seed, (uint64_t) k, 2);                    // class mix
        bool cargo = hash01(seed, (uint64_t) k, 3) < 0.07 + 0.35 * night;
        bool low = kind < 0.14;                                         // light, helicopter, business, turboprop: low and slow
        double speed = low ? 90.0 + 190.0 * hash01(seed, (uint64_t) k, 4) : 380.0 + 110.0 * hash01(seed, (uint64_t) k, 4);   // knots
        double offset = (hash01(seed, (uint64_t) k, 5) * 2.0 - 1.0) * R * 0.95;          // closest-approach distance to the listener, signed
        double chord = 2.0 * std::sqrt(std::max(0.0, R * R - offset * offset));
        double life = chord / speed * 3600.0, age = epoch - born;
        if (age < 0 || age > life || chord < 1.0) continue;
        double bearing = hash01(seed, (uint64_t) k, 6) < 0.7 ? mainBearing + (hash01(seed, (uint64_t) k, 7) - 0.5) * 40.0 + (hash01(seed, (uint64_t) k, 8) < 0.5 ? 0.0 : 180.0)
                                                              : 360.0 * hash01(seed, (uint64_t) k, 7);
        double along = -chord / 2.0 + speed * age / 3600.0;              // nm along the track from the point of closest approach
        double b = bearing * M_PI / 180.0, px = offset * std::cos(b) + along * std::sin(b), py = -offset * std::sin(b) + along * std::cos(b);   // east, north (nm)
        Aircraft& a = out.ac[out.count]; a = Aircraft();
        a.id = (uint32_t) (mix64(seed * 1299709ull + (uint64_t) k) & 0xFFFFFF) | 0x2000000u;
        a.lat = pl.lat + py / 60.0; a.lon = pl.lon + px / (60.0 * std::max(0.05, std::cos(pl.lat * M_PI / 180.0)));
        a.distNm = (float) std::sqrt(px * px + py * py); a.gsKt = (float) speed; a.trackDeg = (float) std::fmod(bearing + 360.0, 360.0);
        double ai = hash01(seed, (uint64_t) k, 9), ti = hash01(seed, (uint64_t) k, 12);
        if (low)
        {
            a.altFt = (float) (900.0 + 7500.0 * hash01(seed, (uint64_t) k, 13)); a.vrateFpm = 0;
            if (kind < 0.03)      { a.category = kHeli;  a.legacyCat = kHeli;  std::snprintf(a.type, sizeof a.type, "EC35"); std::snprintf(a.callsign, sizeof a.callsign, "SAR%d", 10 + (int) (ai * 80)); a.gsKt = (float) (80 + 60 * ai); a.altFt = (float) (500 + 1500 * ti); }
            else if (kind < 0.07) { a.category = kLight; a.legacyCat = kLight; std::snprintf(a.type, sizeof a.type, "C172"); std::snprintf(a.callsign, sizeof a.callsign, "SE-K%c%c", 'A' + (int) (ai * 26), 'A' + (int) (ti * 26)); }
            else if (kind < 0.11) { a.category = kBiz;   a.legacyCat = kSmall; std::snprintf(a.type, sizeof a.type, "%s", bizTypes[(int) (ti * 6) % 6]); std::snprintf(a.callsign, sizeof a.callsign, "N%d%c%c", 100 + (int) (ai * 800), 'A' + (int) (ti * 26), 'A' + (int) (ai * 26)); a.gsKt = (float) (280 + 120 * ai); a.altFt = (float) (14000 + 24000 * ti); }
            else                  { a.category = kProp;  a.legacyCat = kLarge; std::snprintf(a.type, sizeof a.type, "%s", propTypes[(int) (ti * 4) % 4]); std::snprintf(a.callsign, sizeof a.callsign, "%s%d", airlines[(int) (ai * 12) % 12], 100 + (int) (ti * 800)); a.gsKt = (float) (210 + 60 * ai); a.altFt = (float) (6000 + 14000 * ti); }
        }
        else if (cargo)
        {
            a.altFt = (float) (31000 + 8000 * hash01(seed, (uint64_t) k, 13));
            std::snprintf(a.callsign, sizeof a.callsign, "%s%d", cargoLines[(int) (ai * 4) % 4], 100 + (int) (ti * 899)); std::snprintf(a.type, sizeof a.type, "%s", cargoTypes[(int) (ti * 4) % 4]);
            a.category = categoryFromType(a.type, a.callsign, nullptr); a.legacyCat = a.category;
        }
        else
        {
            a.altFt = (float) (hash01(seed, (uint64_t) k, 14) < 0.15 ? 9000.0 + 14000.0 * hash01(seed, (uint64_t) k, 13) : 31000.0 + 9000.0 * hash01(seed, (uint64_t) k, 13));
            std::snprintf(a.callsign, sizeof a.callsign, "%s%d", airlines[(int) (ai * 12) % 12], 100 + (int) (ti * 899)); std::snprintf(a.type, sizeof a.type, "%s", types[(int) (ti * 10) % 10]);
            a.category = categoryFromType(a.type, a.callsign, nullptr); a.legacyCat = a.category;
            if (hash01(seed, (uint64_t) k, 15) < 0.03) { a.category = kMil; a.legacyCat = kLarge; std::snprintf(a.type, sizeof a.type, "C17"); std::snprintf(a.callsign, sizeof a.callsign, "RCH%d", 100 + (int) (ai * 800)); a.altFt = 24000; }
        }
        ++out.count;
    }
}

// One entry point for the simulator: airports get arrivals/departures, any other place gets overflights.
inline void simulatePlace(const Place& pl, uint32_t seed, double epoch, double activity, Frame& out, bool classMix = true)
{
    if (pl.isAirport)
    {
        Place c = pl; c.radiusNm = std::max(10.0, std::min(60.0, pl.radiusNm));       // the arrival/departure pattern lives within ~60 nm
        simulateSky(airportOf(c), seed, epoch, activity, out, classMix);
        const double cx = std::cos(pl.lat * M_PI / 180.0);
        int w = 0;
        for (int i = 0; i < out.count; ++i)
        {
            Aircraft& a = out.ac[i]; double dN = (a.lat - pl.lat) * 60.0, dE = (a.lon - pl.lon) * 60.0 * cx;
            a.distNm = (float) std::sqrt(dN * dN + dE * dE);
            if (a.distNm <= pl.radiusNm) out.ac[w++] = a;
        }
        out.count = w;
    }
    else simulateOverflights(pl, seed, epoch, activity, out);
}

// ---- musical mapper -----------------------------------------------------------------------
// What a class sounds like. Edited in the Aircraft tab; scaled by "Class contrast".
// 1.12: the map shows traffic beyond the listening radius (approaching and leaving). Data is kept up to this distance; only aircraft
// inside the radius sound.
// minutes until an aircraft on a straight course crosses the circle of radius R around the place (enters, or leaves when inside); -1 = never.
// dE / dN: its offset east / north of the place in nm.
inline double crossEtaMin(double dE, double dN, double trackDeg, double gsKt, double R, bool inside)
{
    const double t = trackDeg * 3.14159265358979 / 180.0, vE = gsKt * std::sin(t) / 60.0, vN = gsKt * std::cos(t) / 60.0;
    const double a = vE * vE + vN * vN, b = 2.0 * (dE * vE + dN * vN), c = dE * dE + dN * dN - R * R, disc = b * b - 4.0 * a * c;
    if (a < 1e-6 || disc < 0.0) return -1.0;
    const double s = std::sqrt(disc), t1 = (-b - s) / (2.0 * a), t2 = (-b + s) / (2.0 * a);
    if (inside) return t2 > 0.0 ? t2 : -1.0;
    return t1 > 0.0 ? t1 : -1.0;
}
inline double mapAreaNm(double radiusNm) { return std::min(250.0, std::max(radiusNm * 2.0, radiusNm + 30.0)); }

struct ClassStyle
{
    int timbre = -1;           // -1 = the plugin's own Source A/B voice
    float levelDb = 0;         // relative level
    int octave = 0;            // register offset in octaves (-2..+2)
    float attackMul = 1;       // attack/release time multiplier (0.25..4)
    float chop = 0;            // amplitude chop depth 0..1 (rotor / propeller beating)
};

struct SkySettings
{
    int rootPc = 2, scale = 0;               // D Dorian
    int baseNote = 48;                       // MIDI note at altitude 0
    float rangeOct = 2.5f;
    float consonance = 0.7f;                 // 0..1
    int maxAircraft = 8;
    int voiceCap = 24;                       // the engine's voice limit: never order more voices than it can hold
    bool anchor = true, anchorFifth = true;
    int ghostMin = 3; float ghostLevel = 0.5f;
    float evolveDepth = 0.5f, evolvePeriodMin = 12.0f;
    float loopStretch = 1.0f;
    uint32_t seed = 1;
    // Timbre awareness: for inharmonic sources the Guard measures real roughness from the partials instead of assuming harmonics
    float harmonicity = 1.0f; int nPart = 0; float partRatio[24] = {}, partAmp[24] = {};
    double apLat = 59.65, apLon = 17.92, radiusNm = 40;
    // v1.3
    bool classes = false;                    // the plugin always uses classes; false = plain voices (used by the Guard tests)
    float contrast = 0.7f;                   // 0..1: how far the classes differ from the default voice
    bool swell = true;                       // an aircraft passing close to the listener blooms
    uint32_t soloMask = 0, muteMask = 0;     // bit per class: solo isolates classes (anchor and quiet-sky voices are silenced too), mute silences them
    ClassStyle cls[kNumClasses];
};

struct VoiceOrder
{
    uint32_t id; int note; float level, pan, brightOff, breath, vib; bool anchor, ghost;
    // v1.3 per-aircraft character (defaults = the plain voice)
    int cls = kLarge; int timbre = -1; float blend = 0; float attackMul = 1, releaseMul = 1; float chopRate = 0, chopDepth = 0;
};

constexpr int kMaxOrders = 24;
struct MapResult
{
    int n = 0; VoiceOrder v[kMaxOrders];
    int accepted = 0, rejected = 0, shifted = 0, ghosts = 0;
    float roughness = 0.0f;                  // 0..1 average penalty among sounding pairs
    int regShiftDeg = 0;
    // what happened to each aircraft (for the Profile view): status 1 sounding, 2 shifted by the Guard, 3 rejected (clash), 4 over the voice limit, 5 muted / not soloed
    struct AcOutcome { uint32_t id = 0; uint8_t status = 0; int want = 0, note = 0; float swell = 0; };
    int nOut = 0; AcOutcome outcome[kMaxAc];
};
} // namespace aeola
