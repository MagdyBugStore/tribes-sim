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
#include <cstring>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "data.hpp"
#include "money.hpp"
#include "person.hpp"
#include "activity.hpp"
#include "map.hpp"
#include "http_server.hpp"
#include "snapshot.hpp"
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

// A small strict JSON validator (recursive descent) for the snapshot tests.
struct JsonCheck {
    const std::string& s;
    size_t i = 0;
    explicit JsonCheck(const std::string& str) : s(str) {}
    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
    bool lit(const char* w) {
        size_t n = std::strlen(w);
        if (s.compare(i, n, w) != 0) return false;
        i += n;
        return true;
    }
    bool str() {
        if (i >= s.size() || s[i] != '"') return false;
        for (++i; i < s.size(); ++i) {
            if (s[i] == '\\') { ++i; continue; }
            if (s[i] == '"') { ++i; return true; }
            if ((unsigned char)s[i] < 0x20) return false;
        }
        return false;
    }
    bool num() {
        size_t st = i;
        if (i < s.size() && s[i] == '-') ++i;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' ||
                                s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        return i > st;
    }
    bool val() {
        ws();
        if (i >= s.size()) return false;
        char c = s[i];
        if (c == '{') {
            ++i; ws();
            if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) {
                ws(); if (!str()) return false;
                ws(); if (i >= s.size() || s[i++] != ':') return false;
                if (!val()) return false;
                ws(); if (i >= s.size()) return false;
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == '}') { ++i; return true; }
                return false;
            }
        }
        if (c == '[') {
            ++i; ws();
            if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) {
                if (!val()) return false;
                ws(); if (i >= s.size()) return false;
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == ']') { ++i; return true; }
                return false;
            }
        }
        if (c == '"') return str();
        if (c == 't') return lit("true");
        if (c == 'f') return lit("false");
        if (c == 'n') return lit("null");
        return num();
    }
    bool valid() { bool ok = val(); ws(); return ok && i == s.size(); }
};

static bool json_ok(const std::string& s) { return JsonCheck(s).valid(); }

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

    // -- T14: snapshot + API (issue 3): valid JSON, world untouched ----------------
    {
        World w(3000, 11, 1.0, data);
        for (int d = 0; d < 20; ++d) w.step();
        const uint64_t before = w.state_hash();
        long long lock_us = 0;
        Snapshot s = take_snapshot(w, &lock_us);
        std::vector<std::pair<std::string, std::string>> docs = {
            {"state", json_state(s)},
            {"people", json_people(s, false, 0)},
            {"people since", json_people(s, true, 10)},
            {"market", json_market(s)},
            {"herds", json_herds(s)},
            {"caravans", json_caravans(s)},
            {"map", json_map(s)},
            {"person", json_person(take_person(w, 0))}};
        bool all_valid = true;
        std::string bad;
        for (auto& d : docs)
            if (!json_ok(d.second)) { all_valid = false; bad += d.first + " "; }
        char buf[200];
        std::snprintf(buf, sizeof buf, "%zu docs%s%s", docs.size(),
                      bad.empty() ? "" : ", invalid: ", bad.c_str());
        check(all_valid, "snapshot JSON is valid", buf);
        const uint64_t after = w.state_hash();
        std::snprintf(buf, sizeof buf, "hash %016llx -> %016llx",
                      (unsigned long long)before, (unsigned long long)after);
        check(before == after, "snapshot leaves world unchanged", buf);
        const std::string people = json_people(s, false, 0);
        std::snprintf(buf, sizeof buf, "%d people, %zu bytes (limit 409600), lock held %lld us",
                      s.population, people.size(), lock_us);
        check(people.size() < 400 * 1024 && s.population > 2500,
              "3000-person payload under 400 KB", buf);
        check(json_ok("{\"a\":[1,2,{\"b\":null}]}") && !json_ok("{\"a\":[1,2}") &&
                  !json_ok("{\"a\" 1}"),
              "validator sanity", "accepts good, rejects broken JSON");
        // The sim keeps stepping identically after snapshots (determinism).
        // (the RNG is global and re-seeded by each World constructor, so run
        // one world to completion before building the next)
        World a(300, 5, 1.0, data);
        for (int d = 0; d < 30; ++d) a.step();
        const uint64_t ha = a.state_hash();
        World b(300, 5, 1.0, data);
        for (int d = 0; d < 30; ++d) { b.step(); take_snapshot(b); }
        const uint64_t hb = b.state_hash();
        check(ha == hb, "snapshots do not perturb the run",
              "30 days, with and without snapshots");
    }

    // -- T15: static file path safety --------------------------------------------------
    {
        std::string out;
        const char* bad[] = {"/../src/main.cpp", "/%2e%2e/src/main.cpp",
                             "/..%2fsrc/main.cpp", "/a/../../src/main.cpp",
                             "/..\\src\\main.cpp", "/web/../../etc/passwd"};
        bool refused = true;
        for (const char* p : bad) {
            int st = resolve_static("web", p, &out);
            if (st != 403 && st != 404) refused = false;
        }
        check(refused, "path traversal refused", "6 traversal variants -> 403/404");
        check(resolve_static("web", "/", &out) == 200, "web/index.html is served",
              "GET / resolves under web/");
        HttpRequest r;
        r.method = "GET";
        r.target = "/../src/main.cpp";
        r.host = "localhost:8080";
        HttpResponse resp = handle_request(*std::make_unique<World>(100, 1, 1.0, data), r, "web");
        check(resp.status == 403 || resp.status == 404, "router refuses /../src/main.cpp",
              "status " + std::to_string(resp.status));
    }

    // -- T16: the activity layer (issue 4) ------------------------------------------
    {
        World w(1000, 21, 1.0, data);
        for (int d = 0; d < 9; ++d) w.step();   // day 9 is a Tuesday (day % 7 == 2)
        const int tue = 9, fri = 12;            // 12 % 7 == 5
        // determinism: the world hash and the simulation RNG are untouched
        const uint64_t before = w.state_hash();
        const auto rng_before = RNG;
        long long calls = 0;
        std::vector<std::vector<Activity>> tue_sched, fri_sched;
        for (const auto& p : w.people) {
            tue_sched.push_back(schedule_for(w, p, tue));
            fri_sched.push_back(schedule_for(w, p, fri));
            calls += 2;
        }
        char buf[200];
        std::snprintf(buf, sizeof buf, "%lld schedules, hash %016llx -> %016llx", calls,
                      (unsigned long long)before, (unsigned long long)w.state_hash());
        check(before == w.state_hash() && RNG == rng_before,
              "activity layer is read-only", buf);
        // same inputs, same output
        bool same = true;
        for (size_t i = 0; i < w.people.size() && same; i += 7) {
            auto again = schedule_for(w, w.people[i], tue);
            for (int h = 0; h < 24; ++h)
                same = same && std::strcmp(again[h].name, tue_sched[i][h].name) == 0;
        }
        check(same, "schedules are deterministic", "recomputed rows match");

        int alive = 0, asleep3 = 0, adults = 0, dawn_ok = 0, farmers = 0, farm_rest = 0;
        int fri_adults = 0, fri_mosque = 0;
        for (size_t i = 0; i < w.people.size(); ++i) {
            const Person& p = w.people[i];
            if (!p.alive) continue;
            ++alive;
            if (tue_sched[i][3].kind == ActKind::Sleep || tue_sched[i][3].kind == ActKind::Dead ||
                tue_sched[i][3].kind == ActKind::Rest)
                ++asleep3;
            if (p.age() >= 12) {
                ++adults;
                if (tue_sched[i][5].kind == ActKind::Prayer || tue_sched[i][5].zone == Zone::Mosque) ++dawn_ok;
            }
            if (p.role == "Fallah" && !p.sick && p.age() >= 16) {
                ++farmers;
                if (tue_sched[i][12].kind == ActKind::Rest) ++farm_rest;
            }
            if (p.age() >= 12 && !p.sick) {
                ++fri_adults;
                if (fri_sched[i][12].zone == Zone::Mosque && fri_sched[i][13].zone == Zone::Mosque)
                    ++fri_mosque;
            }
        }
        std::snprintf(buf, sizeof buf, "%d of %d asleep at 03:00", asleep3, alive);
        check(asleep3 >= alive * 95 / 100, "hour 3: almost everyone asleep", buf);
        std::snprintf(buf, sizeof buf, "%d of %d people 12+ praying or at the mosque at 05:00", dawn_ok, adults);
        check(dawn_ok == adults, "dawn prayer for everyone 12+", buf);
        std::snprintf(buf, sizeof buf, "%d of %d healthy farmers resting at 12:00", farm_rest, farmers);
        check(farmers > 0 && farm_rest == farmers, "noon: farmers rest", buf);
        std::snprintf(buf, sizeof buf, "%d of %d healthy people 12+ at the mosque, Friday 12:00-13:00",
                      fri_mosque, fri_adults);
        check(fri_adults > 0 && fri_mosque == fri_adults, "Friday midday: Jumu'ah", buf);

        // every role in roles.csv has a defined work block that really works
        std::string missing, idle;
        for (const auto& r : w.data.roles) {
            if (!has_work_block(r.name)) { missing += r.name + " "; continue; }
            if (r.name == "Child" || r.name == "Tilmidh" || r.name == "Elder") continue;
            Person p;
            p.id = 1;
            p.alive = true;
            p.role = r.name;
            p.gender = r.female_only ? Gender::Female : Gender::Male;
            p.age_days = 30 * 365;
            auto s = schedule_for(w, p, 1);
            int work = 0;
            for (auto& a : s) work += a.kind == ActKind::Work;
            if (work < 3) idle += r.name + " ";
        }
        std::snprintf(buf, sizeof buf, "%zu roles; missing: [%s] no work hours: [%s]",
                      w.data.roles.size(), missing.c_str(), idle.c_str());
        check(missing.empty() && idle.empty(), "every roles.csv role has a work block", buf);
        // labels
        bool labels = true;
        for (auto& row : tue_sched)
            for (auto& a : row)
                labels = labels && a.name[0] && a.en[0] && a.ar[0];
        check(labels, "every activity has name, English and Arabic labels", "all slots");
    }

    // -- T17: the valley map (issue 5) ----------------------------------------------
    {
        char buf[240];
        std::string json_a, json_b, json_other;
        size_t n_objects = 0;
        bool no_overlap = true, in_bounds = true, counts_ok = true, pos_ok = true;
        std::string counts_detail, overlap_detail;
        {
            World w(1000, 33, 1.0, data);
            for (int d = 0; d < 5; ++d) w.step();
            const uint64_t h0 = w.state_hash();
            const auto rng0 = RNG;
            ValleyMap m = build_map(w);
            ValleyMap m2 = build_map(w);
            json_a = json_valley_map(m);
            json_b = json_valley_map(m2);
            n_objects = m.objects.size();
            // overlap: every pair of objects is at least r1 + r2 apart
            double worst = 1e9;
            for (size_t i = 0; i < m.objects.size(); ++i) {
                const auto& a = m.objects[i];
                if (a.x - a.r < 0 || a.x + a.r > MAP_W || a.y - a.r < 0 || a.y + a.r > MAP_H) in_bounds = false;
                for (size_t j = i + 1; j < m.objects.size(); ++j) {
                    const auto& b2 = m.objects[j];
                    const double gap = std::hypot(a.x - b2.x, a.y - b2.y) - a.r - b2.r;
                    worst = std::min(worst, gap);
                    if (gap < 0) no_overlap = false;
                }
            }
            std::snprintf(buf, sizeof buf, "%zu objects, smallest gap %.2f units, all inside %.0fx%.0f",
                          n_objects, worst, MAP_W, MAP_H);
            check(no_overlap && in_bounds, "map objects do not overlap", buf);
            // counts match the simulation
            auto cnt = [&](MapKind k) { return m.of(k).size(); };
            counts_ok = cnt(MapKind::Well) == w.wells.size() && cnt(MapKind::Field) == w.fields.size() &&
                        cnt(MapKind::Shop) == w.shops.size() && cnt(MapKind::Grove) == w.groves.size() &&
                        m.settlements.size() == w.tribe_names.size() &&
                        cnt(MapKind::Gate) == w.tribe_names.size();
            std::snprintf(buf, sizeof buf, "wells %zu/%zu fields %zu/%zu shops %zu/%zu groves %zu/%zu tribes %zu/%zu",
                          cnt(MapKind::Well), w.wells.size(), cnt(MapKind::Field), w.fields.size(),
                          cnt(MapKind::Shop), w.shops.size(), cnt(MapKind::Grove), w.groves.size(),
                          m.settlements.size(), w.tribe_names.size());
            check(counts_ok, "map counts match the simulation", buf);
            // every person gets a point inside the map for every zone; own home/field are stable
            size_t checked = 0;
            for (const auto& p : w.people) {
                if (!p.alive) continue;
                for (int z = 0; z < static_cast<int>(Zone::Count_); ++z) {
                    Pt q = position_of(w, m, p, static_cast<Zone>(z));
                    Pt q2 = position_of(w, m, p, static_cast<Zone>(z));
                    if (!(q.x >= 0 && q.x <= MAP_W && q.y >= 0 && q.y <= MAP_H) || q.x != q2.x || q.y != q2.y)
                        pos_ok = false;
                    ++checked;
                }
            }
            // a field owner stands on his own field; spouses share a home
            int owners_ok = 0, owners = 0, couples = 0, couples_ok = 0;
            for (size_t f = 0; f < w.fields.size(); ++f) {
                const Person* o = w.find_person(w.fields[f].owner);
                if (!o) continue;
                ++owners;
                Pt q = position_of(w, m, *o, Zone::Field);
                const MapObject& fo = m.objects[static_cast<size_t>(m.of(MapKind::Field)[f])];
                if (q.x == fo.x && q.y == fo.y) ++owners_ok;
            }
            for (const auto& p : w.people)
                if (p.alive && p.spouse > p.id) {
                    const Person* s = w.find_person(p.spouse);
                    if (!s || !s->alive) continue;
                    ++couples;
                    Pt a = position_of(w, m, p, Zone::Home), b2 = position_of(w, m, *s, Zone::Home);
                    if (a.x == b2.x && a.y == b2.y) ++couples_ok;
                }
            std::snprintf(buf, sizeof buf, "%zu positions in bounds; owners on own field %d/%d; couples share a home %d/%d",
                          checked, owners_ok, owners, couples_ok, couples);
            check(pos_ok && owners_ok == owners && couples_ok == couples, "position_of is stable and sensible", buf);
            check(h0 == w.state_hash() && RNG == rng0, "map is read-only",
                  "world hash and simulation RNG unchanged");
            // a new well appears, everything else stays where it was
            w.wells.push_back(w.wells[0]);
            ValleyMap m3 = build_map(w);
            bool prefix = m3.objects.size() == m.objects.size() + 1;
            for (size_t i = 0; prefix && i < m.objects.size(); ++i)
                prefix = m.objects[i].x == m3.objects[i].x && m.objects[i].y == m3.objects[i].y &&
                         m.objects[i].kind == m3.objects[i].kind;
            std::snprintf(buf, sizeof buf, "wells %zu -> %zu, all %zu earlier objects unmoved",
                          m.of(MapKind::Well).size(), m3.of(MapKind::Well).size(), m.objects.size());
            check(prefix && m3.of(MapKind::Well).size() == m.of(MapKind::Well).size() + 1,
                  "a newly dug well appears on the map", buf);
            w.wells.pop_back();
        }
        { World w(1000, 33, 1.0, data); json_other = json_valley_map(build_map(w)); }
        { World w(1000, 34, 1.0, data); std::string other = json_valley_map(build_map(w));
          std::snprintf(buf, sizeof buf, "seed 33 twice identical (%zu bytes), seed 34 differs", json_a.size());
          check(json_a == json_b && json_a == json_other && json_a != other && json_ok(json_a),
                "same seed gives the same map", buf); }
        // a big world still has no overlap
        {
            World w(3000, 5, 1.0, data);
            ValleyMap m = build_map(w);
            bool ok = true;
            double worst = 1e9;
            for (size_t i = 0; i < m.objects.size(); ++i)
                for (size_t j = i + 1; j < m.objects.size(); ++j) {
                    const auto& a = m.objects[i];
                    const auto& b2 = m.objects[j];
                    const double gap = std::hypot(a.x - b2.x, a.y - b2.y) - a.r - b2.r;
                    worst = std::min(worst, gap);
                    if (gap < 0) ok = false;
                }
            std::snprintf(buf, sizeof buf, "3000 people: %zu objects, smallest gap %.2f", m.objects.size(), worst);
            check(ok, "no overlap with 3000 people", buf);
        }
    }

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
