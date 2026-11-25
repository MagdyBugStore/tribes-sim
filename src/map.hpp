#pragma once
// The valley map (issue 5): a read-only, deterministic 2D layout of the
// world. It is generated from the seed with its own hash-based generator
// (never the simulation's mt19937_64) and never changes the World.
//
// Prefix stability: wells are placed last and in id order, so when the
// simulation digs a new well the rebuilt map is identical to the old one
// plus the new well.
//
// The `Zone` type comes from the activity layer (src/activity.hpp).

#include <string>
#include <vector>

#include "activity.hpp"
#include "world.hpp"

namespace tribes {

constexpr double MAP_W = 2000.0, MAP_H = 1400.0;

struct Pt { double x = 0, y = 0; };

enum class MapKind {
    Home, Well, Field, Grove, Pasture, Suq, Shop, Mosque, Majlis, School,
    Clinic, BaytAlMal, Mill, Oven, Smithy, Loom, Tannery, Dairy, SaltPan,
    Gate, Caravanserai, Count_
};
const char* map_kind_name(MapKind k);

struct MapObject {
    MapKind kind = MapKind::Home;
    int id = 0;        // index within its kind (well id, field id, shop id...)
    int tribe = -1;
    double x = 0, y = 0, r = 0;   // centre and footprint radius
    int aux = 0;       // shop kind for shops, household slot for homes
};

struct TerrainPoly { std::string type; std::vector<Pt> pts; };  // hills, dunes, wadi

struct SettlementInfo {
    int tribe = 0;
    std::string name;
    Pt center;
    double radius = 0;
    int homes = 0;
};

// What the map needs from the world, copied once under the lock.
struct MapInputs {
    uint64_t seed = 0;
    int initial_people = 0;
    std::vector<std::string> tribe_names;
    std::vector<int> well_tribes;                  // per well id
    std::vector<int> field_owner, field_tribe;     // per field id
    std::vector<int> grove_tribes;                 // per grove id
    struct ShopIn { int id, tribe, kind; };
    std::vector<ShopIn> shops;
};
MapInputs take_map_inputs(World& w);          // locks World::lock briefly
MapInputs map_inputs_unlocked(const World& w); // caller already has exclusive access

struct ValleyMap {
    double width = MAP_W, height = MAP_H;
    uint64_t seed = 0;
    std::vector<TerrainPoly> terrain;
    std::vector<Pt> road;                          // east edge -> suq -> west edge
    std::vector<SettlementInfo> settlements;
    std::vector<MapObject> objects;
    std::vector<std::vector<int>> by_kind;         // object indices per MapKind
    std::vector<std::vector<int>> homes_of_tribe;  // object indices of Homes
    std::vector<int> field_of_owner;               // person id -> field id, or -1 (dense)
    Pt suq;
    int home_slots_per_tribe = 0;
    std::vector<int> well_tribe;                   // per well id (copy of the input)
    const std::vector<int>& of(MapKind k) const { return by_kind[static_cast<size_t>(k)]; }
};

ValleyMap build_map(const MapInputs& in);
inline ValleyMap build_map(const World& w) { return build_map(map_inputs_unlocked(w)); }

// A stable spot for this person in this zone: their own home, their own
// field, their tribe's well, and so on. Pure function of (map, person).
Pt position_of(const World& w, const ValleyMap& m, const Person& p, Zone z);

std::string json_valley_map(const ValleyMap& m);
std::string svg_valley_map(const ValleyMap& m);

}  // namespace tribes
