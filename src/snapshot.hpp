#pragma once
// Read-only snapshot of the live world for the HTTP API (issue 3).
// take_snapshot() holds World::lock only while it copies plain values;
// all JSON text is built afterwards, outside the lock.

#include <string>
#include <vector>

#include "world.hpp"

namespace tribes {

struct PersonRow {
    int id = 0;
    std::string name;
    int tribe = 0;
    bool male = true;
    int age = 0;
    int birth_day = 0;          // tick - age_days; negative = born before founding
    std::string role;
    bool alive = true;
    const char* stage = "adult";
    double hunger = 0, thirst = 0, energy = 0, health = 0, happiness = 0;
    bool sick = false;
    double pouch = 0;
    // Filled from issue 2 (activity layer) once it exists; null until then.
    std::string activity, zone;
};

struct SettlementRow {
    int tribe = 0;
    std::string name;
    int population = 0;
    int wells = 0, groves = 0, fields = 0, shops = 0;
    std::vector<int> shop_ids;
};

struct CaravanRow {
    int id = 0;
    std::string origin;
    bool in_valley = false;
    int days_in_valley = 0, next_arrival = 0, trips = 0, failed = 0;
    double purse = 0;
    double cargo_units = 0;     // goods on board for sale
    int cargo_kinds = 0;
    std::string top_cargo;      // the largest cargo item
};

struct ItemPrice {
    std::string name, unit;
    double avg = 0, lo = 0, hi = 0, base = 0, stock_in_shops = 0;
    int shops = 0;
};

struct Snapshot {
    int day = 0;
    int day_of_year = 0;
    std::string season;
    double seconds_per_day = 1.0;   // the "time scale"
    bool paused = false;
    int population = 0, births = 0, deaths = 0;
    double money_supply = 0, gini = 0;
    std::vector<std::string> tribe_names;
    std::vector<std::string> item_names;
    std::vector<PersonRow> people;          // living people only
    std::vector<SettlementRow> settlements;
    std::vector<CaravanRow> caravans;
    std::vector<ItemPrice> prices;
    // herds[tribe][k] = head count of livestock item k
    std::vector<std::string> herd_items;
    std::vector<std::vector<long long>> herds;
};

struct PersonDetail {
    bool found = false;
    PersonRow row;
    std::string death_cause;
    int death_day = -1;
    double wadiah = 0, qard = 0, honor = 0, skill = 0;
    bool retired = false, orphan = false;
    int spouse = -1, father = -1, mother = -1;
    std::vector<std::pair<int, std::string>> children, siblings;
    std::string spouse_name, father_name, mother_name;
    std::vector<std::string> history;       // last 60 lines
};

// Copy the world under the lock. lock_micros (optional) receives how long
// the lock was held.
Snapshot take_snapshot(World& w, long long* lock_micros = nullptr);
PersonDetail take_person(World& w, int id);

// JSON renderers (no locking; pure functions of the copies).
// since_day < INT_MIN/2 style "no filter" is passed as has_since=false.
std::string json_state(const Snapshot& s);
std::string json_people(const Snapshot& s, bool has_since, int since_day);
std::string json_person(const PersonDetail& d);
std::string json_market(const Snapshot& s);
std::string json_herds(const Snapshot& s);
std::string json_caravans(const Snapshot& s);
std::string json_map(const Snapshot& s);

}  // namespace tribes
