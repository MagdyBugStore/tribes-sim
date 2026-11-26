#pragma once
// The World: people, tribes, land, herds, markets, caravans, institutions,
// the money ledger and the stock books. Owns the founding of the valley
// and the one-day tick.

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "common.hpp"
#include "data.hpp"
#include "events.hpp"
#include "money.hpp"
#include "person.hpp"
#include "stock.hpp"

namespace tribes {

// --- markets -----------------------------------------------------------------

enum class ShopKind { FoodStall, WaterStall, CraftShop, CaravanSouk };

struct Shop {
    int id = -1;
    ShopKind kind = ShopKind::FoodStall;
    int tribe = 0;                  // the market (= tribe settlement) it sits in
    int keeper = -1;                // person id of the Tajir running it
    double till = 0.0;              // the shop's money account
    double deyn_grove = 0;      // what the shop owes the groves (bayt/majlis)
    std::vector<double> stock;      // units per item
    std::vector<double> price;      // current retail price per item
    std::vector<double> target;     // comfortable stock per item
    std::vector<char> trades;       // whether the shop trades the item
    std::map<int, double> deyn;     // customer credit: person id -> owed
    double deyn_total = 0;          // sum of deyn (kept with every change)
    std::vector<std::deque<double>> price_hist;  // last 30 closes per item
    bool sells(int item) const { return trades[static_cast<size_t>(item)] != 0; }
    double units(int item) const { return stock[static_cast<size_t>(item)]; }
    double ask(int item) const { return price[static_cast<size_t>(item)]; }
    double deyn_owed() const {
        double d = 0;
        for (const auto& kv : deyn) d += kv.second;
        return d;
    }
};

// --- land and water ------------------------------------------------------------

struct Well {
    int id = -1;
    int tribe = 0;
    double yield_day = 0;           // liters the aquifer recharges per day
    std::vector<double> stock;      // the cistern (water only)
};

struct Grove {
    int id = -1;
    int tribe = 0;
    double palms = 0;
    double kg_per_palm_year = 0;
    std::vector<double> stock;      // harvested, packed dates (Majlis-owned)
};

struct Field {
    int id = -1;
    int owner = -1;                 // the Fallah household
    double ha = 0;
    double yield_kg_ha = 0;
    bool growing = false;
    int sown_day = -1;
    int harvests = 0;
    std::vector<double> stock;      // the standing crop (wheat) once ripe
};

// One herd: the animals of one species owned by one herder family. The
// heads themselves live in the owner's household stock; this tracks the
// breeding state.
struct Herd {
    int owner = -1;
    int item = -1;                  // the livestock item
    int pregnant = 0;               // females currently carrying
    int due_day = -1;               // when the current carrying batch drops
    double health = 1.0;            // 0.5 starving .. 1.2 thriving
};

// --- caravans ---------------------------------------------------------------------

enum class CaravanState { Away, InValley };

struct CaravanCo {
    int id = -1;
    std::string origin;
    double purse = 0;               // the company's dirham account
    CaravanState state = CaravanState::Away;
    int next_arrival = 0;
    int days_in_valley = 0;
    std::vector<double> cargo;      // imported goods for sale (a holder)
    std::vector<double> bought;     // local goods heading home (a holder)
    int trips = 0, failed = 0;
    double sold_value = 0, bought_value = 0;
};

// --- institutions --------------------------------------------------------------

struct BaytAlMal {
    double balance = 0;
    std::vector<double> stock;      // the sadaqah stores
    double deposits = 0;            // total wadiah claims (amanah)
    double zakat_in = 0, sadaqah_out = 0;
    double own_funds() const { return balance - deposits; }
};

struct Majlis {
    double balance = 0;
    std::vector<double> stock;
    int wells_dug = 0;
    double well_progress = 0;
    double well_cost = 1500.0;
};

struct Mint {
    double balance = 0;
    double coined = 0;
    bool enabled = false;
};

// --- the world ----------------------------------------------------------------

// Named item indices, resolved once from the data file.
struct Idx {
    int water, bread, wheat, flour, bran, dates, milk, laban, cheese, samn, meat,
        salt, hide, leather, sandals, waterskin, wool, yarn, rug, tent, iron,
        tools, blade, pottery, cloth, silk, incense, spice, sugar, coffee,
        oil, firewood;
    int camel, horse, cattle, goat, sheep, donkey, chicken;
    int well, palm, field, saltpan;
};

struct World {
    World(int num_people, uint64_t seed, double tick_interval_s,
          const Data& data);

    // Configuration.
    int initial_people;
    uint64_t seed;
    double tick_interval;
    Data data;
    Idx ix{};
    int n_items = 0;
    double scale = 1.0;             // --people / 1000

    // State.
    std::vector<Person> people;
    int tick = 0;
    std::vector<std::string> tribe_names;
    std::vector<std::string> nisba_m, nisba_f;
    std::set<std::pair<int, int>> alliances;

    std::vector<Shop> shops;
    std::vector<Well> wells;
    std::vector<Grove> groves;
    std::vector<Field> fields;
    std::vector<Herd> herds;
    std::vector<CaravanCo> caravans;
    std::vector<double> ground_stock;   // wells/palms/fields/saltpans counts

    BaytAlMal bayt;
    Majlis majlis;
    Mint mint;
    Ledger ledger;
    EventLog events;

    // Books.
    std::vector<StockBook> books;            // one per item
    std::vector<double> sales_units;         // retail units sold per item
    std::vector<double> sales_value;         // retail dirhams per item
    std::map<std::string, double> stage_value;  // value added per chain stage
    std::map<std::string, double> money_flows;  // dirham totals by transfer reason
    // Performance: the day's cheapest shop per (tribe, item) for pick_shop
    // (cleared every step; prices only move at day end), and the interned
    // ledger reasons so a purchase does not rebuild strings.
    std::map<std::pair<int, int>, int> shop_pick_cache;
    std::vector<std::string> reason_purchase, reason_sale;
    int weddings_total = 0, births_total = 0, deaths_total = 0;
    int births_today = 0, deaths_today = 0;
    int hungry_today = 0, thirsty_today = 0;
    int caravans_arrived = 0, caravans_failed = 0;
    double initial_supply = 0;

    // Threading (live mode).
    std::mutex lock;
    std::atomic<bool> stop{false};
    std::atomic<bool> paused{false};
    // Steady-clock milliseconds when the last day finished (live mode only;
    // 0 = no step yet). Lets the viewer play the day over tick_interval.
    std::atomic<long long> last_step_ms{0};

    // Helpers.
    bool allied(int a, int b) const {
        if (a == b) return true;
        return alliances.count({std::min(a, b), std::max(a, b)}) > 0;
    }
    void set_allied(int a, int b) {
        alliances.insert({std::min(a, b), std::max(a, b)});
    }
    int tribe_size(int t) const;
    std::string full_name(const Person& p) const;
    Person* find_person(int pid);
    int shops_per_tribe() const { return 4; }
    int market_shop(int tribe, int k) const {  // k-th shop of a tribe's suq
        return tribe * shops_per_tribe() + k;
    }
    bool is_livestock(int item) const {
        return data.items[static_cast<size_t>(item)].kind == "livestock";
    }
    bool is_land(int item) const {
        return data.items[static_cast<size_t>(item)].kind == "land";
    }

    // The one-day tick.
    void step();

    // Day-end checks (4b): money conservation + stock reconciliation.
    void assert_money_supply() const;

    double gini() const;
    uint64_t state_hash() const;

    // Founding (world.cpp).
private:
    void resolve_indices();
    void found_land();
    void found_people(int num_people);
    void found_families();
    void found_economy();
    void distribute_starting_stock();
public:
    void assign_trade(Person& p, bool from_labor = false);   // coming-of-age role choice

    // Reports (report.cpp).
    std::string daily_summary();
    std::string end_report();
    std::string describe_person(int pid);
    std::string describe_family(int pid);
    std::string describe_status(int pid);
    std::string describe_history(int pid, int n);
    std::string describe_top(int n);
    std::string describe_tribes();
    std::string describe_economy();
    std::string describe_market();
    std::string describe_trade();
    std::string describe_chain(const std::string& good);
};

// Behavior, one concern per file:
void economy_daily(World& w);        // economy.cpp: production, consumption
void demographics_daily(World& w);   // demographics.cpp: the human fabric
void herds_daily(World& w);          // herds.cpp: breeding, milking, grazing
void caravans_daily(World& w);       // caravan.cpp: the trade routes
void majlis_daily(World& w);         // institutions.cpp: council work
void bayt_daily(World& w);           // institutions.cpp: treasury work
void zakat_annual(World& w);         // institutions.cpp: the yearly zakat
void run_balance_checks(const World& w);  // world.cpp: 4d startup checks

}  // namespace tribes
