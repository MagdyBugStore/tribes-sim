#pragma once
// The activity layer (issue 4): a deterministic, read-only derivation of an
// hourly day for each person from the state the simulation already keeps
// (role, age, gender, season, needs, sickness, pregnancy, caravans).
//
// It never changes the World and never touches the simulation RNG: small
// per-person variations come from a hash of (person id, day, hour).
//
// Conventions: 24 slots, slot h covers hour h (0 = midnight). The week has
// 7 days and day 0 is a Sunday, so Friday is day % 7 == 5. The year has 360
// days (as in the economy code). State-dependent inputs (needs, whether a
// caravan is in the valley) describe "now", so they are applied only when
// `day == world.tick`; for any other day the schedule is the routine one.

#include <string>
#include <vector>

#include "world.hpp"

namespace tribes {

// One table of zones. Extend by adding a row to ZONE_TABLE in activity.cpp.
enum class Zone {
    Home, Well, Field, Grove, Pasture, Suq, Mosque, Majlis, Workshop,
    CaravanRoad, SaltPan, School, Clinic, Gate, Desert, BaytAlMal, Count_
};

enum class ActKind { Sleep, Prayer, Meal, Rest, Social, Study, Care, Work, Need, Dead };

struct Activity {
    const char* name = "";   // stable machine name, e.g. "fetch_water"
    const char* en = "";     // English label
    const char* ar = "";     // Arabic label
    Zone zone = Zone::Home;
    ActKind kind = ActKind::Rest;
};

const char* zone_name(Zone z);       // "well", "caravan_road", ...
const char* zone_label_en(Zone z);
const char* zone_label_ar(Zone z);

// Pseudo-randomness without the simulation RNG (splitmix64 of the inputs).
uint64_t activity_hash(int person_id, int day, int hour);

constexpr int HOURS_PER_DAY = 24;
inline bool is_friday(int day) { return day % 7 == 5; }

// True when roles.csv role `role` has a defined work block.
bool has_work_block(const std::string& role);

// The 24 hourly slots for one person on `day`.
std::vector<Activity> schedule_for(const World& w, const Person& p, int day);

// Console helpers.
std::string describe_schedule(const World& w, int pid, int day);
std::string describe_activity_counts(const World& w, int hour);
// Markdown table: each role's main activity at hours 6, 12 and 18 on an
// ordinary winter weekday (used for the issue-4 PR and docs).
std::string role_activity_table(const World& w);

}  // namespace tribes
