// map.cpp: see map.hpp.
#include "map.hpp"

#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace tribes {

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double GOLDEN = 2.39996322972865332;

// The map's own generator: splitmix64. Not the simulation RNG.
struct MapRng {
    uint64_t s;
    explicit MapRng(uint64_t seed) : s(seed ^ 0x6D617056616C6C65ULL) {}
    uint64_t next() {
        uint64_t x = (s += 0x9E3779B97F4A7C15ULL);
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }
    double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
    double range(double a, double b) { return a + (b - a) * unit(); }
};

uint64_t mix(uint64_t a, uint64_t b) {
    uint64_t x = a * 0x9E3779B97F4A7C15ULL + b + 0x7F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
double hash01(uint64_t a, uint64_t b) {
    return static_cast<double>(mix(a, b) >> 11) / 9007199254740992.0;
}

// Smooth 1D value noise in [0,1], lattice every 220 units.
double noise1(uint64_t seed, int salt, double x) {
    const double u = x / 220.0;
    const int i = static_cast<int>(std::floor(u));
    const double f = u - i;
    const double s = f * f * (3 - 2 * f);
    const double a = hash01(seed + static_cast<uint64_t>(salt), static_cast<uint64_t>(i + 1000));
    const double b = hash01(seed + static_cast<uint64_t>(salt), static_cast<uint64_t>(i + 1001));
    return a + (b - a) * s;
}

double wadi_y(uint64_t seed, double x) {   // centre line of the wadi floor
    return 700.0 + 60.0 * (noise1(seed, 1, x) - 0.5) * 2.0;
}

const char* KIND_NAMES[] = {"home", "well", "field", "grove", "pasture", "suq", "shop",
                            "mosque", "majlis", "school", "clinic", "bayt_al_mal", "mill",
                            "oven", "smithy", "loom", "tannery", "dairy", "salt_pan", "gate",
                            "caravanserai"};
static_assert(sizeof(KIND_NAMES) / sizeof(KIND_NAMES[0]) == static_cast<size_t>(MapKind::Count_),
              "KIND_NAMES needs one row per MapKind");

struct Builder {
    ValleyMap& m;
    struct Disc { double x, y, r; };
    std::vector<Disc> discs;                         // settlement areas: obstacles only
    std::unordered_map<long long, std::vector<int>> grid;
    static constexpr double CELL = 50;

    explicit Builder(ValleyMap& map) : m(map) {}

    static long long key(int cx, int cy) { return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned>(cy); }

    bool free_at(double x, double y, double r, bool strict_bounds, bool vs_discs) const {
        const double lo_y = strict_bounds ? 270 : 40, hi_y = strict_bounds ? 1130 : MAP_H - 40;
        if (x - r < 30 || x + r > MAP_W - 30 || y - r < lo_y || y + r > hi_y) return false;
        const int reach = static_cast<int>(std::ceil((r + 90) / CELL));
        const int cx = static_cast<int>(x / CELL), cy = static_cast<int>(y / CELL);
        for (int i = cx - reach; i <= cx + reach; ++i)
            for (int j = cy - reach; j <= cy + reach; ++j) {
                auto it = grid.find(key(i, j));
                if (it == grid.end()) continue;
                for (int idx : it->second) {
                    const MapObject& o = m.objects[static_cast<size_t>(idx)];
                    const double d = std::hypot(o.x - x, o.y - y);
                    if (d < o.r + r + 2.0) return false;
                }
            }
        if (vs_discs)
            for (const auto& d : discs)
                if (std::hypot(d.x - x, d.y - y) < d.r + r + 2.0) return false;
        return true;
    }

    int add(MapKind k, int id, int tribe, double x, double y, double r, int aux = 0) {
        MapObject o;
        o.kind = k; o.id = id; o.tribe = tribe; o.x = x; o.y = y; o.r = r; o.aux = aux;
        m.objects.push_back(o);
        const int idx = static_cast<int>(m.objects.size()) - 1;
        grid[key(static_cast<int>(x / CELL), static_cast<int>(y / CELL))].push_back(idx);
        return idx;
    }

    // Spiral outward from the preferred point until the footprint is free.
    int place(MapKind k, int id, int tribe, double r, Pt pref, int aux = 0,
              bool strict = true, bool vs_discs = true) {
        const double step = std::max(6.0, r * 0.8);
        for (int i = 0; i < 20000; ++i) {
            const double rad = step * std::sqrt(static_cast<double>(i));
            const double a = i * GOLDEN;
            const double x = pref.x + rad * std::cos(a), y = pref.y + rad * std::sin(a);
            if (free_at(x, y, r, strict, vs_discs)) return add(k, id, tribe, x, y, r, aux);
        }
        // Last resort: give up strictness on bounds (never overlap).
        for (int i = 0; i < 20000; ++i) {
            const double rad = step * std::sqrt(static_cast<double>(i));
            const double a = i * GOLDEN;
            const double x = pref.x + rad * std::cos(a), y = pref.y + rad * std::sin(a);
            if (free_at(x, y, r, false, false)) return add(k, id, tribe, x, y, r, aux);
        }
        return add(k, id, tribe, pref.x, pref.y, r, aux);
    }
};

}  // namespace

const char* map_kind_name(MapKind k) { return KIND_NAMES[static_cast<int>(k)]; }

MapInputs map_inputs_unlocked(const World& w) {
    MapInputs in;
    in.seed = w.seed;
    in.initial_people = w.initial_people;
    in.tribe_names = w.tribe_names;
    for (const auto& x : w.wells) in.well_tribes.push_back(x.tribe);
    for (const auto& f : w.fields) {
        in.field_owner.push_back(f.owner);
        int t = 0;
        if (f.owner >= 0 && f.owner < static_cast<int>(w.people.size()))
            t = w.people[static_cast<size_t>(f.owner)].tribe;
        in.field_tribe.push_back(t);
    }
    for (const auto& g : w.groves) in.grove_tribes.push_back(g.tribe);
    for (const auto& s : w.shops) in.shops.push_back({s.id, s.tribe, static_cast<int>(s.kind)});
    return in;
}

MapInputs take_map_inputs(World& w) {
    std::lock_guard<std::mutex> g(w.lock);
    return map_inputs_unlocked(w);
}

ValleyMap build_map(const MapInputs& in) {
    ValleyMap m;
    m.seed = in.seed;
    m.by_kind.assign(static_cast<size_t>(MapKind::Count_), {});
    const int nt = std::max(1, static_cast<int>(in.tribe_names.size()));
    m.homes_of_tribe.assign(static_cast<size_t>(nt), {});
    m.well_tribe = in.well_tribes;
    const uint64_t seed = in.seed;
    Builder b(m);

    // ---- terrain ---------------------------------------------------------------------
    {
        TerrainPoly hills{"hills", {}}, dunes{"dunes", {}}, wadi{"wadi", {}};
        for (int x = 0; x <= 2000; x += 50) {
            hills.pts.push_back({static_cast<double>(x), 190 + 80 * noise1(seed, 2, x)});
            dunes.pts.push_back({static_cast<double>(x), 1170 - 70 * noise1(seed, 3, x)});
        }
        hills.pts.push_back({2000, 0}); hills.pts.push_back({0, 0});
        dunes.pts.push_back({2000, 1400}); dunes.pts.push_back({0, 1400});
        std::vector<Pt> top, bottom;
        for (int x = 0; x <= 2000; x += 50) {
            const double yc = wadi_y(seed, x), hw = 90 + 50 * noise1(seed, 4, x);
            top.push_back({static_cast<double>(x), yc - hw});
            bottom.push_back({static_cast<double>(x), yc + hw});
        }
        wadi.pts = top;
        for (auto it = bottom.rbegin(); it != bottom.rend(); ++it) wadi.pts.push_back(*it);
        TerrainPoly west{"hills", {{0, 0}, {60 + 50 * noise1(seed, 5, 0), 0}, {40 + 60 * noise1(seed, 5, 700), 700},
                                   {60 + 50 * noise1(seed, 5, 1400), 1400}, {0, 1400}}};
        TerrainPoly east{"hills", {{2000, 0}, {1940 - 50 * noise1(seed, 6, 0), 0}, {1960 - 60 * noise1(seed, 6, 700), 700},
                                   {1940 - 50 * noise1(seed, 6, 1400), 1400}, {2000, 1400}}};
        m.terrain = {hills, dunes, west, east, wadi};
        for (int x = 2000; x >= 0; x -= 100) m.road.push_back({static_cast<double>(x), wadi_y(seed, x)});
    }
    const Pt center{1000.0, wadi_y(seed, 1000)};
    m.suq = center;

    // ---- the centre: suq, mosque, majlis, civic buildings, workshops, shops -------------
    b.add(MapKind::Suq, 0, -1, center.x, center.y, 60);
    b.place(MapKind::Mosque, 0, -1, 22, {center.x - 110, center.y - 70});
    b.place(MapKind::Majlis, 0, -1, 22, {center.x - 110, center.y + 10});
    b.place(MapKind::BaytAlMal, 0, -1, 14, {center.x - 90, center.y + 80});
    b.place(MapKind::School, 0, -1, 14, {center.x - 60, center.y - 120});
    b.place(MapKind::Clinic, 0, -1, 12, {center.x + 60, center.y - 120});
    b.place(MapKind::Mill, 0, -1, 14, {center.x + 110, center.y - 60});
    b.place(MapKind::Oven, 0, -1, 12, {center.x + 110, center.y - 10});
    b.place(MapKind::Smithy, 0, -1, 12, {center.x + 110, center.y + 40});
    b.place(MapKind::Loom, 0, -1, 12, {center.x + 70, center.y + 100});
    b.place(MapKind::Tannery, 0, -1, 12, {center.x + 20, center.y + 120});
    b.place(MapKind::Dairy, 0, -1, 12, {center.x - 30, center.y + 120});
    b.place(MapKind::Caravanserai, 0, -1, 26, {center.x + 190, center.y + 20});
    for (const auto& s : in.shops) {
        const double a = 2 * PI * (s.id + 0.5) / std::max<size_t>(1, in.shops.size());
        b.place(MapKind::Shop, s.id, s.tribe, 6, {center.x + 46 * std::cos(a), center.y + 46 * std::sin(a)}, s.kind);
    }
    {   // keep a clear disc around the whole centre
        b.discs.push_back({center.x, center.y, 250});
    }

    // ---- settlements and homes ------------------------------------------------------------
    m.home_slots_per_tribe = std::max(10, static_cast<int>(std::ceil(1.4 * in.initial_people / nt / 2.5)));
    const int slots = m.home_slots_per_tribe;
    const double c = std::max(4.5, std::min(9.0, 100.0 / std::sqrt(static_cast<double>(slots))));
    const double R = c * std::sqrt(static_cast<double>(slots)) + 10;
    std::vector<Pt> tribe_center(static_cast<size_t>(nt));
    for (int t = 0; t < nt; ++t) {
        const double a = 2 * PI * (t + 0.5) / nt + (hash01(seed, 100 + t) - 0.5) * 0.25;
        tribe_center[static_cast<size_t>(t)] = {center.x + 720 * std::cos(a), center.y + 300 * std::sin(a)};
    }
    // Relax: push settlements apart until no two areas touch, and keep them
    // clear of the centre and inside the valley floor.
    for (int iter = 0; iter < 200; ++iter) {
        for (int i = 0; i < nt; ++i) {
            Pt& p = tribe_center[static_cast<size_t>(i)];
            for (int j = 0; j < nt; ++j) {
                if (i == j) continue;
                const Pt& q = tribe_center[static_cast<size_t>(j)];
                const double d = std::hypot(p.x - q.x, p.y - q.y), want = 2 * R + 40;
                if (d < want && d > 1e-6) { p.x += (p.x - q.x) / d * (want - d) * 0.5; p.y += (p.y - q.y) / d * (want - d) * 0.5; }
            }
            const double dc = std::hypot(p.x - center.x, p.y - center.y), wc = 250 + R + 10;
            if (dc < wc && dc > 1e-6) { p.x = center.x + (p.x - center.x) / dc * wc; p.y = center.y + (p.y - center.y) / dc * wc; }
            p.x = std::min(std::max(p.x, R + 60), MAP_W - R - 60);
            p.y = std::min(std::max(p.y, 270 + R + 5), 1130 - R - 5);
        }
    }
    for (int t = 0; t < nt; ++t) {
        SettlementInfo si;
        si.tribe = t;
        si.name = t < static_cast<int>(in.tribe_names.size()) ? in.tribe_names[static_cast<size_t>(t)] : "tribe";
        si.center = tribe_center[static_cast<size_t>(t)];
        si.radius = R;
        si.homes = slots;
        m.settlements.push_back(si);
        b.discs.push_back({si.center.x, si.center.y, R});
    }
    for (int t = 0; t < nt; ++t) {
        const Pt tc = tribe_center[static_cast<size_t>(t)];
        for (int k = 0; k < slots; ++k) {
            const double rad = c * std::sqrt(k + 0.5), a = k * GOLDEN + hash01(seed, 200 + t) * 6.28;
            const int idx = b.add(MapKind::Home, t * slots + k, t, tc.x + rad * std::cos(a),
                                  tc.y + rad * std::sin(a), 2.5, k);
            m.homes_of_tribe[static_cast<size_t>(t)].push_back(idx);
        }
    }

    // ---- pastures, fields, groves, salt pans, gates -----------------------------------------
    auto away = [&](int t, double d, double rot) -> Pt {   // from the settlement, away from the suq
        const Pt tc = tribe_center[static_cast<size_t>(t)];
        const double a = std::atan2(tc.y - center.y, tc.x - center.x) + rot;
        return {tc.x + d * std::cos(a), tc.y + d * std::sin(a)};
    };
    for (int t = 0; t < nt; ++t) {
        b.place(MapKind::Pasture, t * 2, t, 70, away(t, R + 120, 0.7));
        b.place(MapKind::Pasture, t * 2 + 1, t, 70, away(t, R + 120, -0.7));
    }
    for (size_t i = 0; i < in.field_owner.size(); ++i) {
        const int t = std::min(std::max(in.field_tribe[i], 0), nt - 1);
        const double sgn = (i % 2) ? 1.0 : -1.0;
        const double x = tribe_center[static_cast<size_t>(t)].x + sgn * (R + 25 + hash01(seed, 300 + i) * 300);
        const double y = wadi_y(seed, x) + (hash01(seed, 400 + i) - 0.5) * 140;
        b.place(MapKind::Field, static_cast<int>(i), t, 14, {x, y});
    }
    for (size_t i = 0; i < in.grove_tribes.size(); ++i) {
        const int t = std::min(std::max(in.grove_tribes[i], 0), nt - 1);
        const Pt tc = tribe_center[static_cast<size_t>(t)];
        const double x = tc.x + ((i % 2) ? 1.0 : -1.0) * (R + 40 + hash01(seed, 500 + i) * 120);
        b.place(MapKind::Grove, static_cast<int>(i), t, 30, {x, wadi_y(seed, x) + (hash01(seed, 600 + i) - 0.5) * 120});
    }
    b.place(MapKind::SaltPan, 0, -1, 30, {150, 1100}, 0, false);
    b.place(MapKind::SaltPan, 1, -1, 30, {260, 1130}, 0, false);
    for (int t = 0; t < nt; ++t) {
        const Pt tc = tribe_center[static_cast<size_t>(t)];
        const double a = std::atan2(center.y - tc.y, center.x - tc.x);
        b.place(MapKind::Gate, t, t, 4, {tc.x + (R + 8) * std::cos(a), tc.y + (R + 8) * std::sin(a)}, 0, true, false);
    }
    // ---- wells last, in id order (new wells append) ---------------------------------------
    for (size_t i = 0; i < in.well_tribes.size(); ++i) {
        const int t = std::min(std::max(in.well_tribes[i], 0), nt - 1);
        const int nth = static_cast<int>(i) / nt;   // 0 for the founding well of the tribe
        b.place(MapKind::Well, static_cast<int>(i), t, 8, away(t, R + 40 + 30 * nth, 0.0 + 0.9 * (nth % 2 ? 1 : -1) * (nth > 0)));
    }

    // ---- indexes ---------------------------------------------------------------------------------
    for (size_t i = 0; i < m.objects.size(); ++i)
        m.by_kind[static_cast<size_t>(m.objects[i].kind)].push_back(static_cast<int>(i));
    int max_owner = -1;
    for (int o : in.field_owner) max_owner = std::max(max_owner, o);
    m.field_of_owner.assign(static_cast<size_t>(max_owner + 1), -1);
    for (size_t f = 0; f < in.field_owner.size(); ++f)
        if (in.field_owner[f] >= 0) m.field_of_owner[static_cast<size_t>(in.field_owner[f])] = static_cast<int>(f);
    return m;
}

// --- positions ------------------------------------------------------------------------------------

namespace {

int household_key(const World& w, const Person& p) {
    auto key_of = [&](const Person& q) { return q.spouse >= 0 ? std::min(q.id, q.spouse) : q.id; };
    if (p.age() < COMING_OF_AGE) {
        const int parent = p.father >= 0 ? p.father : p.mother;
        if (parent >= 0 && parent < static_cast<int>(w.people.size()))
            return key_of(w.people[static_cast<size_t>(parent)]);
    }
    return key_of(p);
}

Pt jitter(const Person& p, int salt, double amount, Pt c) {
    const double a = hash01(static_cast<uint64_t>(p.id) + 7, static_cast<uint64_t>(salt)) * 2 * PI;
    const double r = amount * std::sqrt(hash01(static_cast<uint64_t>(p.id) + 11, static_cast<uint64_t>(salt)));
    return {c.x + r * std::cos(a), c.y + r * std::sin(a)};
}

}  // namespace

Pt position_of(const World& w, const ValleyMap& m, const Person& p, Zone z) {
    const int nt = static_cast<int>(m.homes_of_tribe.size());
    const int t = std::min(std::max(p.tribe, 0), nt - 1);
    auto obj = [&](MapKind k, int i) -> Pt {
        const auto& v = m.of(k);
        if (v.empty()) return m.suq;
        const MapObject& o = m.objects[static_cast<size_t>(v[static_cast<size_t>(i) % v.size()])];
        return {o.x, o.y};
    };
    auto of_tribe = [&](MapKind k, int salt) -> Pt {   // an object of the person's tribe
        std::vector<int> mine;
        for (int idx : m.of(k))
            if (m.objects[static_cast<size_t>(idx)].tribe == t) mine.push_back(idx);
        if (mine.empty()) return obj(k, static_cast<int>(mix(static_cast<uint64_t>(p.id), static_cast<uint64_t>(salt)) % 1024));
        const MapObject& o = m.objects[static_cast<size_t>(mine[mix(static_cast<uint64_t>(p.id), static_cast<uint64_t>(salt)) % mine.size()])];
        return {o.x, o.y};
    };
    switch (z) {
        case Zone::Home: {
            const auto& homes = m.homes_of_tribe[static_cast<size_t>(t)];
            const int slot = static_cast<int>(mix(m.seed, static_cast<uint64_t>(household_key(w, p))) % homes.size());
            const MapObject& o = m.objects[static_cast<size_t>(homes[static_cast<size_t>(slot)])];
            return {o.x, o.y};
        }
        case Zone::Field: {
            for (int owner : {p.id, p.spouse, p.father, p.mother})
                if (owner >= 0 && owner < static_cast<int>(m.field_of_owner.size()) &&
                    m.field_of_owner[static_cast<size_t>(owner)] >= 0)
                    return obj(MapKind::Field, m.field_of_owner[static_cast<size_t>(owner)]);
            return of_tribe(MapKind::Field, 1);
        }
        case Zone::Well: return of_tribe(MapKind::Well, 2);
        case Zone::Grove: return jitter(p, 3, 18, of_tribe(MapKind::Grove, 3));
        case Zone::Pasture: return jitter(p, 4, 45, of_tribe(MapKind::Pasture, 4));
        case Zone::Suq: return jitter(p, 5, 45, m.suq);
        case Zone::Mosque: return jitter(p, 6, 14, obj(MapKind::Mosque, 0));
        case Zone::Majlis: return jitter(p, 7, 14, obj(MapKind::Majlis, 0));
        case Zone::School: return jitter(p, 8, 8, obj(MapKind::School, 0));
        case Zone::Clinic: return jitter(p, 9, 6, obj(MapKind::Clinic, 0));
        case Zone::BaytAlMal: return jitter(p, 10, 6, obj(MapKind::BaytAlMal, 0));
        case Zone::Workshop: {
            MapKind k = MapKind::Smithy;
            if (p.role == "Miller") k = MapKind::Mill;
            else if (p.role == "Baker") k = MapKind::Oven;
            else if (p.role == "Weaver") k = MapKind::Loom;
            else if (p.role == "Tanner") k = MapKind::Tannery;
            else if (p.role == "Dairyman") k = MapKind::Dairy;
            return jitter(p, 11, 5, obj(k, 0));
        }
        case Zone::SaltPan: return jitter(p, 12, 20, obj(MapKind::SaltPan, static_cast<int>(mix(p.id, 12) % 2)));
        case Zone::Gate: return of_tribe(MapKind::Gate, 13);
        case Zone::CaravanRoad: {
            // a stable point on the road, east of the suq (or the caravanserai)
            const double f = hash01(static_cast<uint64_t>(p.id) + 3, 14);
            const size_t n = m.road.size();
            const size_t i = static_cast<size_t>(f * (n / 2 - 1));   // east half
            Pt q = m.road[i];
            if (q.x < m.suq.x + 230) return obj(MapKind::Caravanserai, 0);
            return q;
        }
        case Zone::Desert: {
            const double a = hash01(static_cast<uint64_t>(p.id) + 5, 15), b2 = hash01(static_cast<uint64_t>(p.id) + 9, 16);
            return {200 + 1600 * a, b2 < 0.5 ? 60 + 100 * b2 : 1250 + 100 * (b2 - 0.5)};   // hills or dunes
        }
        default: return m.suq;
    }
}

// --- output ------------------------------------------------------------------------------------------

std::string json_valley_map(const ValleyMap& m) {
    std::string o;
    char b[160];
    auto pt = [&](const Pt& p) {
        std::snprintf(b, sizeof b, "[%.1f,%.1f]", p.x, p.y);
        o += b;
    };
    o += "{\"width\":2000,\"height\":1400,\"seed\":" + std::to_string(m.seed) + ",\"terrain\":[";
    for (size_t i = 0; i < m.terrain.size(); ++i) {
        if (i) o += ",";
        o += "{\"type\":\"" + m.terrain[i].type + "\",\"points\":[";
        for (size_t k = 0; k < m.terrain[i].pts.size(); ++k) { if (k) o += ","; pt(m.terrain[i].pts[k]); }
        o += "]}";
    }
    o += "],\"road\":[";
    for (size_t k = 0; k < m.road.size(); ++k) { if (k) o += ","; pt(m.road[k]); }
    o += "],\"suq\":"; pt(m.suq);
    o += ",\"settlements\":[";
    for (size_t i = 0; i < m.settlements.size(); ++i) {
        const auto& s = m.settlements[i];
        if (i) o += ",";
        std::string nm;
        for (char ch : s.name) { if (ch == '"' || ch == '\\') nm += '\\'; nm += ch; }
        std::snprintf(b, sizeof b, "{\"tribe\":%d,\"x\":%.1f,\"y\":%.1f,\"radius\":%.1f,\"homes\":%d,\"name\":\"", s.tribe,
                      s.center.x, s.center.y, s.radius, s.homes);
        o += b; o += nm; o += "\"}";
    }
    o += "],\"home_slots_per_tribe\":" + std::to_string(m.home_slots_per_tribe);
    o += ",\"fields\":[\"kind\",\"id\",\"tribe\",\"x\",\"y\",\"r\",\"aux\"],\"objects\":[";
    for (size_t i = 0; i < m.objects.size(); ++i) {
        const auto& ob = m.objects[i];
        if (i) o += ",";
        std::snprintf(b, sizeof b, "[\"%s\",%d,%d,%.1f,%.1f,%.1f,%d]", map_kind_name(ob.kind), ob.id, ob.tribe,
                      ob.x, ob.y, ob.r, ob.aux);
        o += b;
    }
    o += "]}";
    return o;
}

std::string svg_valley_map(const ValleyMap& m) {
    std::string o;
    char b[256];
    o += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 2000 1400\" width=\"1400\" height=\"980\">\n";
    o += "<rect width=\"2000\" height=\"1400\" fill=\"#e8d9b0\"/>\n";
    for (const auto& t : m.terrain) {
        const char* fill = t.type == "hills" ? "#a39a8a" : t.type == "dunes" ? "#f0c978" : "#cdb98a";
        o += std::string("<polygon fill=\"") + fill + "\" points=\"";
        for (const auto& p : t.pts) { std::snprintf(b, sizeof b, "%.0f,%.0f ", p.x, p.y); o += b; }
        o += "\"/>\n";
    }
    o += "<polyline fill=\"none\" stroke=\"#7a5a2c\" stroke-width=\"6\" stroke-dasharray=\"18 10\" points=\"";
    for (const auto& p : m.road) { std::snprintf(b, sizeof b, "%.0f,%.0f ", p.x, p.y); o += b; }
    o += "\"/>\n";
    for (const auto& s : m.settlements) {
        std::snprintf(b, sizeof b, "<circle cx=\"%.0f\" cy=\"%.0f\" r=\"%.0f\" fill=\"#d9c28f\" stroke=\"#8a6d3b\" stroke-dasharray=\"6 6\"/>\n",
                      s.center.x, s.center.y, s.radius);
        o += b;
    }
    auto colour = [](MapKind k) -> const char* {
        switch (k) {
            case MapKind::Home: return "#5a3a1a";
            case MapKind::Well: return "#1c6fd1";
            case MapKind::Field: return "#6aa84f";
            case MapKind::Grove: return "#2e7d32";
            case MapKind::Pasture: return "#b5cc7a";
            case MapKind::Suq: return "#e0a030";
            case MapKind::Shop: return "#c0392b";
            case MapKind::Mosque: return "#ffffff";
            case MapKind::Majlis: return "#8e44ad";
            case MapKind::SaltPan: return "#f5f5f5";
            case MapKind::Gate: return "#000000";
            case MapKind::Caravanserai: return "#7a5a2c";
            default: return "#444444";
        }
    };
    for (const auto& ob : m.objects) {
        std::snprintf(b, sizeof b, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"%s\" fill-opacity=\"%s\" stroke=\"#333\" stroke-width=\"0.6\"><title>%s %d</title></circle>\n",
                      ob.x, ob.y, ob.r, colour(ob.kind), ob.kind == MapKind::Pasture ? "0.5" : "1", map_kind_name(ob.kind), ob.id);
        o += b;
    }
    for (const auto& s : m.settlements) {
        std::string nm;
        for (char ch : s.name) { if (ch != '<' && ch != '&') nm += ch; }
        std::snprintf(b, sizeof b, "<text x=\"%.0f\" y=\"%.0f\" font-size=\"22\" text-anchor=\"middle\" fill=\"#222\">%s</text>\n",
                      s.center.x, s.center.y - s.radius - 8, nm.c_str());
        o += b;
    }
    o += "<text x=\"1000\" y=\"610\" font-size=\"22\" text-anchor=\"middle\" fill=\"#222\">Suq, mosque, majlis</text>\n</svg>\n";
    return o;
}

}  // namespace tribes
