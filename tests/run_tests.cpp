// tests/run_tests.cpp - the section 7 test suite. Plain asserts, no
// framework. Build from the project root:
//   g++ -O2 -std=c++17 -Isrc tests/run_tests.cpp $(ls src/*.cpp | grep -v main.cpp) -o tests/run_tests
// Run from the project root (it reads data/):  ./tests/run_tests
//
// Every check prints a PASS line with its measured numbers; a failure
// prints FAIL and exits non-zero. Note on the Gini test: the spec's
// original 0.35-0.55 band was dropped by the user's explicit decision -
// he wants the wealth spread to emerge realistically, not be steered -
// so the Gini test measures and reports, and asserts only validity.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"
#include "money.hpp"
#include "person.hpp"
#include "stock.hpp"
#include "world.hpp"

namespace tribes {
std::mt19937_64 RNG;
// economy.cpp keeps its day-end shop routine file-local to the economy
// translation unit; world.cpp calls it through this same declaration.
extern void shop_close_day(World& w, Shop& s);
}  // namespace tribes

using namespace tribes;

static int failures = 0;

static void check(bool ok, const std::string& name, const std::string& detail) {
    if (ok) {
        std::printf("PASS  %-34s %s\n", name.c_str(), detail.c_str());
    } else {
        std::printf("FAIL  %-34s %s\n", name.c_str(), detail.c_str());
        ++failures;
    }
}

static bool bad_word(const std::string& s) {
    std::string t;
    for (char c : s) t += static_cast<char>(std::tolower(c));
    return t.find("raid") != std::string::npos ||
           t.find("ghazw") != std::string::npos ||
           t.find("plunder") != std::string::npos ||
           t.find("loot") != std::string::npos;
}

int main() {
    Data data;
    try {
        data = load_data("data");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "cannot load data/: %s\n", e.what());
        return 2;
    }

    // -- T1: money supply conservation, 3 seeds x 2 sizes --------------------
    for (int people : {300, 1000}) {
        for (uint64_t seed : {1, 2, 3}) {
            World w(people, seed, 1.0, data);
            const double m0 = total_money(w);
            for (int d = 0; d < 30; ++d) w.step();
            const double m1 = total_money(w);
            char buf[128];
            std::snprintf(buf, sizeof buf, "people=%d seed=%llu: %.2f -> %.2f",
                          people, (unsigned long long)seed, m0, m1);
            check(std::fabs(m1 - m0) < 1e-6 * std::max(1.0, m0),
                  "money supply constant", buf);
        }
    }

    // -- T2 + T6 + T11: one watched run (stock formula, needs, price band) ---
    {
        World w(300, 1, 1.0, data);
        bool stock_ok = true, needs_ok = true, band_ok = true;
        std::string stock_bad, needs_bad, band_bad;
        for (int d = 0; d < 60 && stock_ok && needs_ok && band_ok; ++d) {
            w.step();
            for (int i = 0; i < w.n_items; ++i) {
                const double exp = w.books[static_cast<size_t>(i)].expected();
                const double got = total_holdings(w, i);
                if (std::fabs(exp - got) > 1e-6 * std::max(1.0, exp)) {
                    stock_ok = false;
                    stock_bad = w.data.items[static_cast<size_t>(i)].name +
                                " expected " + std::to_string(exp) + " held " +
                                std::to_string(got);
                    break;
                }
            }
            for (const auto& p : w.people) {
                if (!p.alive) continue;
                const double vals[5] = {p.hunger, p.thirst, p.energy,
                                        p.health, p.happiness};
                for (double v : vals) {
                    if (v < 0.0 || v > 100.0) {
                        needs_ok = false;
                        needs_bad = "person " + std::to_string(p.id) +
                                    " need out of [0,100]: " +
                                    std::to_string(v);
                        break;
                    }
                }
                if (!needs_ok) break;
            }
            for (const auto& s : w.shops) {
                for (int i = 0; i < w.n_items; ++i) {
                    if (!s.sells(i)) continue;
                    const ItemDef& it = w.data.items[static_cast<size_t>(i)];
                    const double lo = it.base_price * it.price_floor - 1e-9;
                    const double hi = it.base_price * it.price_ceil + 1e-9;
                    const double pr = s.ask(i);
                    if (pr < lo || pr > hi) {
                        band_ok = false;
                        band_bad = it.name + " at " + std::to_string(pr) +
                                   " outside [" + std::to_string(lo) + ", " +
                                   std::to_string(hi) + "]";
                        break;
                    }
                }
                if (!band_ok) break;
            }
        }
        check(stock_ok, "stock conservation formula", stock_ok ? "60 days, all items reconcile" : stock_bad);
        check(needs_ok, "needs within [0,100]", needs_ok ? "60 days, all living people" : needs_bad);
        check(band_ok, "prices within floor/ceiling", band_ok ? "60 days, every shop and item" : band_bad);
    }

    // -- T3: move_stock / transfer reject bad quantities ----------------------
    {
        World w(200, 7, 1.0, data);
        const int bread = w.ix.bread;
        // give person 0 some bread through the legal channel
        stock_create(w, h_person(0), bread, 5.0, "harvest");
        const double before0 = stock_of(w, h_person(0), bread);
        const double before1 = stock_of(w, h_person(1), bread);
        bool neg = move_stock(w, bread, -1.0, h_person(0), h_person(1), "gift");
        bool over = move_stock(w, bread, before0 + 100.0, h_person(0),
                               h_person(1), "gift");
        const double after0 = stock_of(w, h_person(0), bread);
        const double after1 = stock_of(w, h_person(1), bread);
        check(!neg && !over && after0 == before0 && after1 == before1,
              "move_stock rejects bad quantities",
              "negative and overdraw both refused, holdings unchanged");
        Acc a = person_acc(0), b = person_acc(1);
        const double m0 = total_money(w);
        bool mneg = transfer(w, a, b, -5.0, "gift");
        bool mover = transfer(w, a, b, 1e12, "gift");
        check(!mneg && !mover && total_money(w) == m0,
              "transfer rejects bad amounts",
              "negative and overdraw both refused, supply unchanged");
    }

    // -- T4 + T9 + T10: herds, no raids, local production ---------------------
    {
        World w(300, 2, 1.0, data);
        bool herds_ok = true;
        for (int d = 0; d < 60; ++d) {
            w.step();
            for (int i = 0; i < w.n_items; ++i) {
                if (!w.is_livestock(i)) continue;
                if (total_holdings(w, i) < -1e-9) herds_ok = false;
            }
            for (const auto& h : w.herds)
                if (h.owner >= 0 &&
                    stock_of(w, h_person(h.owner), h.item) < -1e-9)
                    herds_ok = false;
        }
        check(herds_ok, "animal counts never negative", "60 days, all herds");
        // breeding is the only local source of livestock (imports aside)
        bool breed_only = true;
        std::string breed_bad;
        for (int i = 0; i < w.n_items; ++i) {
            if (!w.is_livestock(i)) continue;
            for (const auto& kv :
                 w.books[static_cast<size_t>(i)].created_by) {
                if (kv.first != "birth" && kv.first != "import") {
                    breed_only = false;
                    breed_bad = w.data.items[static_cast<size_t>(i)].name +
                                " created by '" + kv.first + "'";
                }
            }
        }
        check(breed_only, "herds grow only by breeding",
              breed_only ? "livestock created_by = {birth, import} only"
                         : breed_bad);
        // no raid code reachable: no money flow, stock process or event
        // mentions raiding
        bool raid_free = true;
        std::string raid_bad;
        for (const auto& kv : w.money_flows)
            if (bad_word(kv.first)) { raid_free = false; raid_bad = kv.first; }
        for (int i = 0; i < w.n_items; ++i) {
            for (const auto& kv : w.books[static_cast<size_t>(i)].created_by)
                if (bad_word(kv.first)) { raid_free = false; raid_bad = kv.first; }
            for (const auto& kv : w.books[static_cast<size_t>(i)].destroyed_by)
                if (bad_word(kv.first)) { raid_free = false; raid_bad = kv.first; }
        }
        for (const auto& e : w.events.entries)
            if (bad_word(e)) { raid_free = false; raid_bad = e; }
        check(raid_free, "no raid code reachable",
              raid_free ? "no raid/ghazw flow, process or event"
                        : "found: " + raid_bad);
        // goods the valley cannot make are never produced locally
        bool local_ok = true;
        std::string local_bad;
        for (int i = 0; i < w.n_items; ++i) {
            const ItemDef& it = w.data.items[static_cast<size_t>(i)];
            if (it.kind != "good" || it.local) continue;
            if (w.books[static_cast<size_t>(i)].produced > 1e-9) {
                local_ok = false;
                local_bad = it.name + " produced locally";
            }
        }
        // ... and no recipe outputs a non-local good or a live animal
        for (const auto& r : w.data.recipes)
            for (const auto& out : r.out_name) {
                const int oi = w.data.item_index(out);
                if (oi < 0) continue;
                const ItemDef& def = w.data.items[static_cast<size_t>(oi)];
                if ((def.kind == "good" && !def.local) || def.kind == "livestock") {
                    local_ok = false;
                    local_bad = "recipe " + r.stage + " outputs " + out;
                }
            }
        check(local_ok, "non-local goods never produced",
              local_ok ? "runtime books and recipe table agree" : local_bad);
    }

    // -- T5: price direction on stock shocks ----------------------------------
    {
        World w(200, 3, 1.0, data);
        const int dates = w.ix.dates;
        Shop* shop = nullptr;
        for (auto& s : w.shops)
            if (s.sells(dates)) { shop = &s; break; }
        if (!shop) {
            check(false, "prices respond to shocks", "no shop sells dates");
        } else {
            const ItemDef& it = w.data.items[static_cast<size_t>(dates)];
            const double base = it.base_price;
            // scarcity: almost empty shelf -> price must rise
            shop->price[static_cast<size_t>(dates)] = base;
            double tgt = std::max(1.0, shop->target[static_cast<size_t>(dates)]);
            double have = stock_of(w, h_shop(shop->id), dates);
            if (have > 0.05 * tgt)
                stock_destroy(w, h_shop(shop->id), dates, have - 0.05 * tgt,
                              "spoilage");
            shop_close_day(w, *shop);
            const double scarce = shop->ask(dates);
            // glut: triple the target stock -> price must fall
            shop->price[static_cast<size_t>(dates)] = base;
            stock_create(w, h_shop(shop->id), dates, 3.0 * tgt, "import");
            shop_close_day(w, *shop);
            const double glut = shop->ask(dates);
            char buf[160];
            std::snprintf(buf, sizeof buf,
                          "scarce %.3f > base %.3f, glut %.3f < base",
                          scarce, base, glut);
            check(scarce > base && glut < base,
                  "prices respond to shocks", buf);
        }
    }

    // -- T7 + T8: family symmetry and marriage genders ------------------------
    {
        World w(300, 4, 1.0, data);
        for (int d = 0; d < 120; ++d) w.step();
        bool sym = true, gender_ok = true;
        std::string sym_bad;
        for (const auto& p : w.people) {
            if (!p.alive) continue;
            if (p.spouse >= 0) {
                const Person& s = w.people[static_cast<size_t>(p.spouse)];
                if (!s.alive || s.spouse != p.id) {
                    sym = false;
                    sym_bad = "spouse link broken at person " +
                              std::to_string(p.id);
                }
                if (s.gender == p.gender) gender_ok = false;
            }
            for (int c : p.children) {
                const Person& ch = w.people[static_cast<size_t>(c)];
                if (!ch.alive) continue;
                if (ch.father != p.id && ch.mother != p.id) {
                    sym = false;
                    sym_bad = "child " + std::to_string(c) +
                              " does not point back to " + std::to_string(p.id);
                }
            }
            if (p.father >= 0) {
                const Person& f = w.people[static_cast<size_t>(p.father)];
                if (f.alive &&
                    std::find(f.children.begin(), f.children.end(), p.id) ==
                        f.children.end()) {
                    sym = false;
                    sym_bad = "father of " + std::to_string(p.id) +
                              " missing back-link";
                }
            }
            if (p.mother >= 0) {
                const Person& m = w.people[static_cast<size_t>(p.mother)];
                if (m.alive &&
                    std::find(m.children.begin(), m.children.end(), p.id) ==
                        m.children.end()) {
                    sym = false;
                    sym_bad = "mother of " + std::to_string(p.id) +
                              " missing back-link";
                }
            }
        }
        check(sym, "family links symmetric", sym ? "120 days, both directions" : sym_bad);
        check(gender_ok, "no same-gender marriage",
              gender_ok ? "every living couple is male+female" : "found one");
    }

    // -- T12: Gini after 2 years, seeds 1-3 (measure + report; the user
    //        dropped the 0.35-0.55 target band - realism over stability) -----
    std::vector<double> ginis;
    for (uint64_t seed : {1, 2, 3}) {
        World w(300, seed, 1.0, data);
        for (int d = 0; d < 730; ++d) w.step();
        const double g = w.gini();
        ginis.push_back(g);
        int alive = 0;
        for (const auto& p : w.people) alive += p.alive ? 1 : 0;
        char buf[200];
        std::snprintf(buf, sizeof buf,
                      "seed=%llu: gini %.3f, alive %d/%d, births %d, deaths %d, "
                      "money supply %.0f",
                      (unsigned long long)seed, g, alive, w.initial_people,
                      w.births_total, w.deaths_total, total_money(w));
        check(g >= 0.0 && g <= 1.0, "gini after 2 years (report)", buf);
    }

    // -- T13: determinism ------------------------------------------------------
    {
        // the RNG is re-seeded by each World constructor, so build and run
        // each world to completion before constructing the next one
        World a(300, 5, 1.0, data);
        for (int d = 0; d < 60; ++d) a.step();
        World b(300, 5, 1.0, data);
        for (int d = 0; d < 60; ++d) b.step();
        World c(300, 6, 1.0, data);
        for (int d = 0; d < 60; ++d) c.step();
        char buf[160];
        std::snprintf(buf, sizeof buf, "seed5 twice: %016llx vs %016llx; seed6: %016llx",
                      (unsigned long long)a.state_hash(),
                      (unsigned long long)b.state_hash(),
                      (unsigned long long)c.state_hash());
        check(a.state_hash() == b.state_hash() &&
                  a.state_hash() != c.state_hash(),
              "determinism (state hash)", buf);
    }

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
