#pragma once
// Dashboard history (issue 8): a rolling per-day record of the valley's
// numbers (money split, Gini, hunger, births and deaths, prices, shop stock,
// herds), a caravan log and the event ticker. record() runs once per day
// under World::lock; json() only takes the dashboard's own mutex, so the
// HTTP thread never waits for the simulation.

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "world.hpp"

namespace tribes {

constexpr size_t DASH_DAYS = 365;      // rolling window

class Dashboard {
public:
    // Append today's numbers (call after World::step, with World::lock held).
    void record(const World& w);
    // JSON with the rows newer than the given day / sequence numbers.
    std::string json(int since_day, size_t since_caravan, size_t since_event) const;
    int last_day() const;
    void clear();

private:
    struct DayRec {
        int day = 0, pop = 0, hungry = 0, thirsty = 0, sick = 0, births = 0, deaths = 0, wells = 0;
        int arrived = 0, failed = 0;
        double money = 0, pouches = 0, bayt = 0, tills = 0, purses = 0, other = 0, gini = 0;
        std::vector<float> price, stock, use;   // per item (average ask, units in shops, consumed today)
        std::vector<float> herd_total, herd_born, herd_slaughtered;   // per livestock item
        std::vector<std::vector<float>> herd_tribe;                   // [tribe][livestock] head count
    };
    struct CaravanLog {
        size_t seq = 0;
        int id = 0, day = 0, stay = 0;
        std::string origin, top_cargo;
        bool failed = false;
        double cargo_units = 0, sold = 0, bought = 0;
        bool open = true;
    };
    struct EventRec { size_t seq; std::string text; int day; };
    struct Track { bool in_valley = false; double sold0 = 0, bought0 = 0; int failed0 = 0; size_t log_index = 0; };

    mutable std::mutex mu_;
    std::vector<std::string> item_names_, item_units_, livestock_names_, tribe_names_;
    std::vector<double> item_base_;
    std::vector<int> livestock_;
    std::deque<DayRec> days_;
    std::deque<CaravanLog> caravans_;
    std::deque<EventRec> events_;
    std::vector<Track> track_;
    std::vector<double> prev_born_, prev_slaughtered_;
    size_t caravan_seq_ = 0, event_seen_ = 0;
};

Dashboard& dashboard();   // the process-wide instance used by the server

}  // namespace tribes
