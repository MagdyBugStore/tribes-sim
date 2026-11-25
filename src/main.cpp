// main.cpp: flags, the headless run, the live run, the CSV export.
#include <cstring>
#include <fstream>
#include <iostream>

#include "activity.hpp"
#include "world.hpp"

namespace tribes {
std::mt19937_64 RNG;  // the one source of randomness, seeded by --seed
void run_console(World& w, int max_days, int serve_port,
                 const std::string& web_root);
}  // namespace tribes

using namespace tribes;

int main(int argc, char** argv) {
    int people = 1000, days = 365;
    uint64_t seed = 42;
    double interval = 1.0;
    bool headless = false;
    int serve_port = 0, schedule_id = -1;
    bool role_table = false;
    std::string csv, data_dir = "data", web_root = "web";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--people") people = std::atoi(next());
        else if (a == "--days") days = std::atoi(next());
        else if (a == "--seed") seed = std::strtoull(next(), nullptr, 10);
        else if (a == "--tick-interval") interval = std::atof(next());
        else if (a == "--headless") headless = true;
        else if (a == "--csv") csv = next();
        else if (a == "--data") data_dir = next();
        else if (a == "--serve") serve_port = std::atoi(next());
        else if (a == "--web") web_root = next();
        else if (a == "--schedule") schedule_id = std::atoi(next());
        else if (a == "--role-table") role_table = true;
    }
    Data data;
    try {
        data = load_data(data_dir);
    } catch (const std::exception& e) {
        std::cerr << "failed to load the data files: " << e.what() << "\n";
        return 1;
    }
    World w(people, seed, interval, data);
    if (role_table) { std::cout << role_activity_table(w); return 0; }
    if (schedule_id >= 0) {
        std::cout << describe_schedule(w, schedule_id, w.tick) << "\n";
        return 0;
    }

    std::cout << "Valley founded: " << people << " people in "
              << w.tribe_names.size() << " tribes, " << w.shops.size()
              << " shops, " << w.wells.size() << " wells, " << w.fields.size()
              << " fields, seed " << seed << ", dirham supply "
              << commas(w.initial_supply) << "\n";
    // Print the resource table actually in force (section 4d).
    std::cout << "Resource table (per 1000 people, from " << data_dir
              << "/resources.csv):\n";
    for (auto& d : data.items)
        std::cout << "  " << d.name << " (" << d.unit << "): start "
                  << d.start_total << ", base price " << d.base_price
                  << " - " << d.reason << "\n";
    run_balance_checks(w);

    std::ofstream out;
    if (!csv.empty()) {
        out.open(csv);
        out << "day,population,gini,pouches,bayt,majlis,hungry,thirsty,"
               "births,deaths,caravans,wheat_stock,bread_stock,bread_price\n";
    }
    if (headless) {
        for (int d = 0; d < days; ++d) {
            w.step();
            std::cout << w.daily_summary() << "\n";
            if (out) {
                double bp = 0;
                int n = 0;
                for (auto& s : w.shops)
                    if (s.sells(w.ix.bread)) {
                        bp += s.ask(w.ix.bread);
                        ++n;
                    }
                int alive = 0;
                double cash = 0;
                for (auto& p : w.people)
                    if (p.alive) {
                        ++alive;
                        cash += p.pouch;
                    }
                out << w.tick << "," << alive << "," << w.gini() << "," << cash
                    << "," << w.bayt.balance << "," << w.majlis.balance << ","
                    << w.hungry_today << "," << w.thirsty_today << ","
                    << w.births_total << "," << w.deaths_total << ","
                    << w.caravans_arrived << ","
                    << total_holdings(w, w.ix.wheat) << ","
                    << total_holdings(w, w.ix.bread) << ","
                    << (n ? bp / n : 0) << "\n";
            }
        }
    } else {
        run_console(w, days, serve_port, web_root);
    }
    std::cout << w.end_report() << "\n";
    return 0;
}
