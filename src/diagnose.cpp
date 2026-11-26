// diagnose.cpp: see diagnose.hpp. Plain counting over the finished run; changes nothing in the model.
#include "diagnose.hpp"
#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <vector>
namespace tribes {
namespace {
const char* AGE_BANDS[] = {"0-4", "5-14", "15-44", "45-64", "65+"};
int band(int age) { return age < 5 ? 0 : age < 15 ? 1 : age < 45 ? 2 : age < 65 ? 3 : 4; }
std::string f(const char* fmt, double a = 0, double b = 0, double c = 0, double d = 0) { char buf[256]; std::snprintf(buf, sizeof buf, fmt, a, b, c, d); return buf; }
double avg_price(const World& w, int item) { double s = 0; int n = 0; for (auto& sh : w.shops) if (sh.sells(item)) { s += sh.ask(item); ++n; } return n ? s / n : 0; }
double pouches(const World& w) { double c = 0; for (auto& p : w.people) if (p.alive) c += p.pouch; return c; }
double tills(const World& w) { double c = 0; for (auto& s : w.shops) c += s.till; return c; }
}  // namespace

std::string diagnose_run(World& w, int days) {
    std::ostringstream o;
    const int start_pop = static_cast<int>(std::count_if(w.people.begin(), w.people.end(), [](const Person& p) { return p.alive; }));
    std::vector<std::vector<double>> prices;   // first 90 days: bread, water, dates, wheat
    long hungry_sum = 0, thirsty_sum = 0;
    struct Snap { int day; double pouch, bayt, majlis, till; };
    std::vector<Snap> snaps; snaps.push_back({0, pouches(w), w.bayt.balance, w.majlis.balance, tills(w)});
    for (int d = 0; d < days; ++d) {
        w.step();
        if (d < 90) prices.push_back({avg_price(w, w.ix.bread), avg_price(w, w.ix.water), avg_price(w, w.ix.dates), avg_price(w, w.ix.wheat)});
        hungry_sum += w.hungry_today; thirsty_sum += w.thirsty_today;
        if ((d + 1) % 30 == 0) snaps.push_back({d + 1, pouches(w), w.bayt.balance, w.majlis.balance, tills(w)});
    }
    const double years = days / 360.0;   // the sim year is 360 days
    int alive = 0; for (auto& p : w.people) if (p.alive) ++alive;
    o << "DIAGNOSE: " << days << " days (" << f("%.2f", years) << " sim years of 360 days), start pop " << start_pop << ", end pop " << alive << "\n";
    o << f("births %.0f, deaths %.0f", w.births_total, w.deaths_total) << ", weddings " << w.weddings_total << "\n";
    const double mean_pop = (start_pop + alive) / 2.0;
    o << f("crude birth rate %.1f per 1000/yr, crude death rate %.1f per 1000/yr", w.births_total / mean_pop * 1000 / years, w.deaths_total / mean_pop * 1000 / years) << "\n";
    o << f("mean hungry per day %.1f, mean thirsty per day %.1f", hungry_sum / double(days), thirsty_sum / double(days)) << "\n\n";
    std::map<std::string, int> by_cause, by_role, by_tribe; int by_band[5] = {0}; int dead = 0;
    std::map<std::string, int> role_pop;
    for (auto& p : w.people) {
        role_pop[p.role]++;
        if (p.alive) continue;
        ++dead; by_cause[p.death_cause.empty() ? "(none)" : p.death_cause]++; by_role[p.role]++; by_tribe[w.tribe_names[static_cast<size_t>(p.tribe)]]++;
        by_band[band(p.age_days / 360)]++;   // age in 360-day years
    }
    o << "Deaths by cause:\n"; for (auto& kv : by_cause) o << "  " << kv.first << ": " << kv.second << "\n";
    o << "Deaths by age group (360-day years):\n"; for (int i = 0; i < 5; ++i) o << "  " << AGE_BANDS[i] << ": " << by_band[i] << f(" (%.0f%%)", dead ? 100.0 * by_band[i] / dead : 0) << "\n";
    o << "Deaths by role (deaths / people ever with that role at end):\n"; for (auto& kv : by_role) o << "  " << kv.first << ": " << kv.second << "\n";
    o << "Deaths by tribe:\n"; for (auto& kv : by_tribe) o << "  " << kv.first << ": " << kv.second << "\n";
    // births by mother's age at the child's birth (mother's age_days now minus child's age days; only exact while the mother is alive)
    int bm[5] = {0}; int bm_n = 0;
    for (auto& c : w.people) if (c.mother >= 0 && c.age_days < days) { const Person& m = w.people[static_cast<size_t>(c.mother)]; if (!m.alive) continue; int age = (m.age_days - c.age_days) / 360; bm[band(age)]++; ++bm_n; }
    o << "\nBirths by mother's age (mothers still alive, " << bm_n << " births):\n"; for (int i = 0; i < 5; ++i) o << "  " << AGE_BANDS[i] << ": " << bm[i] << "\n";
    o << "\nFirst 90 days, average shop price (bread, water, dates, wheat) every 10 days:\n";
    for (size_t d = 0; d < prices.size(); d += 10) o << f("  day %.0f: bread %.2f  water %.3f", double(d + 1), prices[d][0], prices[d][1]) << f("  dates %.2f  wheat %.2f", prices[d][2], prices[d][3]) << "\n";
    // income by role is not modelled as a number in the model; report pouch by role at end instead
    std::map<std::string, std::pair<double, int>> pouch_role;
    std::vector<std::pair<double, const Person*>> wealth;
    for (auto& p : w.people) if (p.alive) { pouch_role[p.role].first += p.pouch; pouch_role[p.role].second++; wealth.push_back({p.pouch, &p}); }
    o << "\nMean pouch by role at the end (dirhams):\n"; for (auto& kv : pouch_role) o << "  " << kv.first << " (" << kv.second.second << "): " << f("%.1f", kv.second.first / kv.second.second) << "\n";
    std::sort(wealth.begin(), wealth.end(), [](auto& a, auto& b) { return a.first < b.first; });
    o << "Hungry now (hunger < 30) by wealth decile (1 = poorest):\n";
    for (int dcl = 0; dcl < 10 && !wealth.empty(); ++dcl) { size_t a = wealth.size() * dcl / 10, b = wealth.size() * (dcl + 1) / 10; int h = 0, n = 0; double lo = wealth[a].first, hi = wealth[b - 1].first; for (size_t i = a; i < b; ++i) { ++n; if (wealth[i].second->hunger < 30) ++h; } o << f("  decile %.0f: %.0f of ", dcl + 1.0, h) << n << f(" hungry (pouch %.1f to %.1f)", lo, hi) << "\n"; }
    std::map<std::string, std::pair<int, int>> hr; for (auto& p : w.people) if (p.alive) { hr[p.role].second++; if (p.hunger < 30) hr[p.role].first++; }
    o << "Hungry now by role:\n"; for (auto& kv : hr) if (kv.second.first) o << "  " << kv.first << ": " << kv.second.first << "/" << kv.second.second << "\n";
    std::map<int, std::pair<int, int>> ht; for (auto& p : w.people) if (p.alive) { ht[p.tribe].second++; if (p.hunger < 30) ht[p.tribe].first++; }
    o << "Hungry now by tribe:\n"; for (auto& kv : ht) o << "  " << w.tribe_names[static_cast<size_t>(kv.first)] << ": " << kv.second.first << "/" << kv.second.second << "\n";
    o << "\nWhere the money sits every 30 days (pouches, Bayt al-Mal, Majlis, shop tills):\n";
    for (auto& s : snaps) o << f("  day %.0f: pouches %.0f  bayt %.0f  majlis %.0f", s.day, s.pouch, s.bayt, s.majlis) << f("  tills %.0f", s.till) << "\n";
    o << "\nMoney flow by reason (dirhams, whole run):\n";
    std::vector<std::pair<double, std::string>> fl; double tot = 0; for (auto& kv : w.money_flows) { fl.push_back({kv.second, kv.first}); tot += kv.second; }
    std::sort(fl.rbegin(), fl.rend()); for (size_t i = 0; i < fl.size() && i < 20; ++i) o << "  " << fl[i].second << ": " << f("%.0f (%.1f%%)", fl[i].first, tot ? 100 * fl[i].first / tot : 0) << "\n";
    return o.str();
}
}  // namespace tribes
