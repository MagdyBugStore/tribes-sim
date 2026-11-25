// snapshot.cpp: lock-short copy of the world, then JSON rendering.
#include "snapshot.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>

namespace tribes {

static const char* season_of(int doy) {
    // The sim's year is 360 days.
    static const char* names[] = {"winter", "spring", "summer", "autumn"};
    return names[(doy / 90) % 4];
}

static PersonRow row_of(const World& w, const Person& p) {
    PersonRow r;
    r.id = p.id;
    r.name = w.full_name(p);
    r.tribe = p.tribe;
    r.male = p.gender == Gender::Male;
    r.age = p.age();
    r.birth_day = w.tick - p.age_days;
    r.role = p.role;
    r.alive = p.alive;
    r.stage = p.stage_name();
    r.hunger = p.hunger;
    r.thirst = p.thirst;
    r.energy = p.energy;
    r.health = p.health;
    r.happiness = p.happiness;
    r.sick = p.sick;
    r.pouch = p.pouch;
    return r;
}

Snapshot take_snapshot(World& w, long long* lock_micros) {
    Snapshot s;
    auto t0 = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> g(w.lock);
        s.day = w.tick;
        s.day_of_year = w.tick % 360;
        s.season = season_of(s.day_of_year);
        s.seconds_per_day = w.tick_interval;
        s.paused = w.paused.load();
        {
            const long long last = w.last_step_ms.load();
            const long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            s.since_step_ms = last > 0 ? std::max(0LL, now_ms - last) : 0;
        }
        s.births = w.births_total;
        s.deaths = w.deaths_total;
        s.money_supply = total_money(w);
        s.gini = w.gini();
        s.tribe_names = w.tribe_names;
        const size_t nt = w.tribe_names.size();
        for (const auto& d : w.data.items) s.item_names.push_back(d.name);

        s.people.reserve(w.people.size());
        s.settlements.resize(nt);
        for (size_t t = 0; t < nt; ++t) {
            s.settlements[t].tribe = static_cast<int>(t);
            s.settlements[t].name = w.tribe_names[t];
        }
        std::vector<int> livestock;
        for (int i = 0; i < w.n_items; ++i)
            if (w.is_livestock(i)) {
                livestock.push_back(i);
                s.herd_items.push_back(w.data.items[static_cast<size_t>(i)].name);
            }
        s.herds.assign(nt, std::vector<long long>(livestock.size(), 0));
        for (const auto& p : w.people) {
            if (!p.alive) continue;
            s.people.push_back(row_of(w, p));
            const size_t t = static_cast<size_t>(p.tribe);
            if (t < nt) {
                ++s.settlements[t].population;
                for (size_t k = 0; k < livestock.size(); ++k) {
                    const size_t it = static_cast<size_t>(livestock[k]);
                    if (it < p.stock.size())
                        s.herds[t][k] += std::llround(p.stock[it]);
                }
            }
        }
        s.population = static_cast<int>(s.people.size());
        for (const auto& x : w.wells)
            if (static_cast<size_t>(x.tribe) < nt) ++s.settlements[static_cast<size_t>(x.tribe)].wells;
        for (const auto& x : w.groves)
            if (static_cast<size_t>(x.tribe) < nt) ++s.settlements[static_cast<size_t>(x.tribe)].groves;
        for (const auto& sh : w.shops)
            if (static_cast<size_t>(sh.tribe) < nt) {
                auto& st = s.settlements[static_cast<size_t>(sh.tribe)];
                ++st.shops;
                st.shop_ids.push_back(sh.id);
            }
        for (const auto& f : w.fields) {
            if (f.owner >= 0 && f.owner < static_cast<int>(w.people.size())) {
                const size_t t = static_cast<size_t>(w.people[static_cast<size_t>(f.owner)].tribe);
                if (t < nt) ++s.settlements[t].fields;
            }
        }
        for (const auto& c : w.caravans) {
            CaravanRow r;
            r.id = c.id;
            r.origin = c.origin;
            r.in_valley = c.state == CaravanState::InValley;
            r.days_in_valley = c.days_in_valley;
            r.next_arrival = c.next_arrival;
            r.trips = c.trips;
            r.failed = c.failed;
            r.purse = c.purse;
            double best = 0;
            for (size_t i = 0; i < c.cargo.size(); ++i) {
                if (c.cargo[i] > 0.5) ++r.cargo_kinds;
                r.cargo_units += c.cargo[i];
                if (c.cargo[i] > best) {
                    best = c.cargo[i];
                    r.top_cargo = w.data.items[i].name;
                }
            }
            s.caravans.push_back(r);
        }
        for (int i = 0; i < w.n_items; ++i) {
            const auto& d = w.data.items[static_cast<size_t>(i)];
            ItemPrice ip;
            ip.name = d.name;
            ip.unit = d.unit;
            ip.base = d.base_price;
            ip.lo = 1e300;
            for (const auto& sh : w.shops) {
                if (!sh.sells(i)) continue;
                const double p = sh.ask(i);
                ip.avg += p;
                ip.lo = std::min(ip.lo, p);
                ip.hi = std::max(ip.hi, p);
                ip.stock_in_shops += sh.units(i);
                ++ip.shops;
            }
            if (ip.shops) ip.avg /= ip.shops;
            else ip.lo = 0;
            s.prices.push_back(ip);
        }
    }
    if (lock_micros)
        *lock_micros = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now() - t0).count();
    return s;
}

PersonDetail take_person(World& w, int id) {
    PersonDetail d;
    std::lock_guard<std::mutex> g(w.lock);
    const Person* p = w.find_person(id);
    if (!p) return d;
    d.found = true;
    d.row = row_of(w, *p);
    d.death_cause = p->death_cause;
    d.death_day = p->death_day;
    d.wadiah = p->wadiah;
    d.qard = p->qard;
    d.honor = p->honor;
    d.skill = p->skill;
    d.retired = p->retired;
    d.orphan = p->orphan;
    d.spouse = p->spouse;
    d.father = p->father;
    d.mother = p->mother;
    auto name_of = [&](int pid) -> std::string {
        const Person* q = w.find_person(pid);
        return q ? w.full_name(*q) : std::string();
    };
    d.spouse_name = name_of(p->spouse);
    d.father_name = name_of(p->father);
    d.mother_name = name_of(p->mother);
    for (int c : p->children) d.children.push_back({c, name_of(c)});
    for (int parent : {p->father, p->mother}) {
        const Person* q = w.find_person(parent);
        if (!q) continue;
        for (int c : q->children) {
            if (c == p->id) continue;
            bool dup = false;
            for (auto& sb : d.siblings) dup = dup || sb.first == c;
            if (!dup) d.siblings.push_back({c, name_of(c)});
        }
    }
    for (const auto& h : p->history) d.history.push_back(h);
    while (d.history.size() > 60) d.history.erase(d.history.begin());
    return d;
}

// --- JSON ------------------------------------------------------------------------

namespace {
struct J {
    std::string o;
    void raw(const char* s) { o += s; }
    void str(const std::string& v) {
        o += '"';
        for (unsigned char c : v) {
            switch (c) {
                case '"': o += "\\\""; break;
                case '\\': o += "\\\\"; break;
                case '\n': o += "\\n"; break;
                case '\r': o += "\\r"; break;
                case '\t': o += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char b[8];
                        std::snprintf(b, sizeof b, "\\u%04x", c);
                        o += b;
                    } else o += static_cast<char>(c);
            }
        }
        o += '"';
    }
    void num(double v) {
        if (!std::isfinite(v)) { o += "null"; return; }
        char b[48];
        if (v == std::floor(v) && std::fabs(v) < 1e15)
            std::snprintf(b, sizeof b, "%.0f", v);
        else
            std::snprintf(b, sizeof b, "%.2f", v);
        o += b;
    }
    void integer(long long v) { o += std::to_string(v); }
    void boolean(bool v) { o += v ? "true" : "false"; }
    void key(const char* k) { str(k); o += ':'; }
};
}  // namespace

std::string json_state(const Snapshot& s) {
    J j;
    j.raw("{");
    j.key("day"); j.integer(s.day); j.raw(",");
    j.key("day_of_year"); j.integer(s.day_of_year); j.raw(",");
    j.key("year"); j.integer(s.day / 360); j.raw(",");
    j.key("season"); j.str(s.season); j.raw(",");
    j.key("seconds_per_day"); j.num(s.seconds_per_day); j.raw(",");
    j.key("paused"); j.boolean(s.paused); j.raw(",");
    j.key("since_step_ms"); j.integer(s.since_step_ms); j.raw(",");
    j.key("population"); j.integer(s.population); j.raw(",");
    j.key("births"); j.integer(s.births); j.raw(",");
    j.key("deaths"); j.integer(s.deaths); j.raw(",");
    j.key("money_supply"); j.num(s.money_supply); j.raw(",");
    j.key("gini"); j.num(std::round(s.gini * 1000) / 1000); j.raw(",");
    j.key("tribes"); j.raw("[");
    for (size_t t = 0; t < s.settlements.size(); ++t) {
        if (t) j.raw(",");
        j.raw("{"); j.key("id"); j.integer(static_cast<long long>(t)); j.raw(",");
        j.key("name"); j.str(s.settlements[t].name); j.raw(",");
        j.key("population"); j.integer(s.settlements[t].population); j.raw("}");
    }
    j.raw("]}");
    return j.o;
}

std::string json_people(const Snapshot& s, bool has_since, int since_day) {
    J j;
    j.raw("{");
    j.key("day"); j.integer(s.day); j.raw(",");
    j.key("fields"); j.raw("[\"id\",\"name\",\"tribe\",\"gender\",\"age\",\"role\","
                           "\"stage\",\"hunger\",\"thirst\",\"energy\",\"health\","
                           "\"happiness\",\"sick\",\"pouch\",\"activity\",\"zone\"]");
    j.raw(",");
    j.key("rows"); j.raw("[");
    bool first = true;
    for (const auto& p : s.people) {
        if (has_since && p.birth_day < since_day) continue;
        if (!first) j.raw(",");
        first = false;
        j.raw("[");
        j.integer(p.id); j.raw(","); j.str(p.name); j.raw(",");
        j.integer(p.tribe); j.raw(",");
        j.raw(p.male ? "\"m\"" : "\"f\""); j.raw(",");
        j.integer(p.age); j.raw(","); j.str(p.role); j.raw(",");
        j.str(p.stage); j.raw(",");
        j.integer(std::llround(p.hunger)); j.raw(",");
        j.integer(std::llround(p.thirst)); j.raw(",");
        j.integer(std::llround(p.energy)); j.raw(",");
        j.integer(std::llround(p.health)); j.raw(",");
        j.integer(std::llround(p.happiness)); j.raw(",");
        j.integer(p.sick ? 1 : 0); j.raw(",");
        j.integer(std::llround(p.pouch)); j.raw(",");
        if (p.activity.empty()) j.raw("null"); else j.str(p.activity);
        j.raw(",");
        if (p.zone.empty()) j.raw("null"); else j.str(p.zone);
        j.raw("]");
    }
    j.raw("]}");
    return j.o;
}

std::string json_person(const PersonDetail& d) {
    J j;
    if (!d.found) return "{\"error\":\"no such person\"}";
    const PersonRow& r = d.row;
    auto link = [&](int id, const std::string& name) {
        if (id < 0) { j.raw("null"); return; }
        j.raw("{"); j.key("id"); j.integer(id); j.raw(",");
        j.key("name"); j.str(name); j.raw("}");
    };
    auto list = [&](const std::vector<std::pair<int, std::string>>& v) {
        j.raw("[");
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) j.raw(",");
            link(v[i].first, v[i].second);
        }
        j.raw("]");
    };
    j.raw("{");
    j.key("id"); j.integer(r.id); j.raw(",");
    j.key("name"); j.str(r.name); j.raw(",");
    j.key("tribe"); j.integer(r.tribe); j.raw(",");
    j.key("gender"); j.str(r.male ? "male" : "female"); j.raw(",");
    j.key("age"); j.integer(r.age); j.raw(",");
    j.key("stage"); j.str(r.stage); j.raw(",");
    j.key("role"); j.str(r.role); j.raw(",");
    j.key("alive"); j.boolean(r.alive); j.raw(",");
    j.key("retired"); j.boolean(d.retired); j.raw(",");
    j.key("orphan"); j.boolean(d.orphan); j.raw(",");
    j.key("death_day"); if (d.death_day >= 0) j.integer(d.death_day); else j.raw("null"); j.raw(",");
    j.key("death_cause"); if (d.death_cause.empty()) j.raw("null"); else j.str(d.death_cause); j.raw(",");
    j.key("needs"); j.raw("{");
    j.key("hunger"); j.num(r.hunger); j.raw(",");
    j.key("thirst"); j.num(r.thirst); j.raw(",");
    j.key("energy"); j.num(r.energy); j.raw(",");
    j.key("health"); j.num(r.health); j.raw(",");
    j.key("happiness"); j.num(r.happiness); j.raw("},");
    j.key("sick"); j.boolean(r.sick); j.raw(",");
    j.key("pouch"); j.num(r.pouch); j.raw(",");
    j.key("wadiah"); j.num(d.wadiah); j.raw(",");
    j.key("qard"); j.num(d.qard); j.raw(",");
    j.key("honor"); j.num(d.honor); j.raw(",");
    j.key("skill"); j.num(d.skill); j.raw(",");
    j.key("activity"); j.raw("null,");
    j.key("zone"); j.raw("null,");
    j.key("family"); j.raw("{");
    j.key("spouse"); link(d.spouse, d.spouse_name); j.raw(",");
    j.key("father"); link(d.father, d.father_name); j.raw(",");
    j.key("mother"); link(d.mother, d.mother_name); j.raw(",");
    j.key("children"); list(d.children); j.raw(",");
    j.key("siblings"); list(d.siblings); j.raw("},");
    j.key("history"); j.raw("[");
    for (size_t i = 0; i < d.history.size(); ++i) {
        if (i) j.raw(",");
        j.str(d.history[i]);
    }
    j.raw("]}");
    return j.o;
}

std::string json_market(const Snapshot& s) {
    J j;
    j.raw("{"); j.key("day"); j.integer(s.day); j.raw(",");
    j.key("items"); j.raw("[");
    bool first = true;
    for (const auto& p : s.prices) {
        if (!p.shops) continue;
        if (!first) j.raw(",");
        first = false;
        j.raw("{"); j.key("name"); j.str(p.name); j.raw(",");
        j.key("unit"); j.str(p.unit); j.raw(",");
        j.key("price_avg"); j.num(p.avg); j.raw(",");
        j.key("price_min"); j.num(p.lo); j.raw(",");
        j.key("price_max"); j.num(p.hi); j.raw(",");
        j.key("base_price"); j.num(p.base); j.raw(",");
        j.key("shop_stock"); j.num(p.stock_in_shops); j.raw(",");
        j.key("shops"); j.integer(p.shops); j.raw("}");
    }
    j.raw("]}");
    return j.o;
}

std::string json_herds(const Snapshot& s) {
    J j;
    j.raw("{"); j.key("day"); j.integer(s.day); j.raw(",");
    j.key("animals"); j.raw("[");
    for (size_t k = 0; k < s.herd_items.size(); ++k) {
        if (k) j.raw(",");
        j.str(s.herd_items[k]);
    }
    j.raw("],"); j.key("tribes"); j.raw("[");
    for (size_t t = 0; t < s.herds.size(); ++t) {
        if (t) j.raw(",");
        j.raw("{"); j.key("tribe"); j.integer(static_cast<long long>(t)); j.raw(",");
        j.key("name"); j.str(s.tribe_names[t]); j.raw(",");
        j.key("counts"); j.raw("{");
        for (size_t k = 0; k < s.herd_items.size(); ++k) {
            if (k) j.raw(",");
            j.key(s.herd_items[k].c_str()); j.integer(s.herds[t][k]);
        }
        j.raw("}}");
    }
    j.raw("]}");
    return j.o;
}

std::string json_caravans(const Snapshot& s) {
    J j;
    j.raw("{"); j.key("day"); j.integer(s.day); j.raw(",");
    j.key("caravans"); j.raw("[");
    for (size_t i = 0; i < s.caravans.size(); ++i) {
        const auto& c = s.caravans[i];
        if (i) j.raw(",");
        j.raw("{"); j.key("id"); j.integer(c.id); j.raw(",");
        j.key("origin"); j.str(c.origin); j.raw(",");
        j.key("state"); j.str(c.in_valley ? "in_valley" : "away"); j.raw(",");
        // The sim has no coordinates: position is the valley or away.
        j.key("position"); j.str(c.in_valley ? "valley" : "away"); j.raw(",");
        j.key("days_in_valley"); j.integer(c.days_in_valley); j.raw(",");
        j.key("next_arrival_day"); j.integer(c.next_arrival); j.raw(",");
        j.key("trips"); j.integer(c.trips); j.raw(",");
        j.key("failed"); j.integer(c.failed); j.raw(",");
        j.key("purse"); j.num(c.purse); j.raw(",");
        j.key("cargo"); j.raw("{"); j.key("units"); j.num(c.cargo_units); j.raw(",");
        j.key("kinds"); j.integer(c.cargo_kinds); j.raw(",");
        j.key("largest"); j.str(c.top_cargo); j.raw("}}");
    }
    j.raw("]}");
    return j.o;
}

std::string json_map(const Snapshot& s) {
    J j;
    j.raw("{"); j.key("day"); j.integer(s.day); j.raw(",");
    j.key("settlements"); j.raw("[");
    for (size_t i = 0; i < s.settlements.size(); ++i) {
        const auto& t = s.settlements[i];
        if (i) j.raw(",");
        j.raw("{"); j.key("tribe"); j.integer(t.tribe); j.raw(",");
        j.key("name"); j.str(t.name); j.raw(",");
        j.key("population"); j.integer(t.population); j.raw(",");
        j.key("zones"); j.raw("{"); j.key("wells"); j.integer(t.wells); j.raw(",");
        j.key("groves"); j.integer(t.groves); j.raw(",");
        j.key("fields"); j.integer(t.fields); j.raw(",");
        j.key("market_shops"); j.integer(t.shops); j.raw("}}");
    }
    j.raw("]}");
    return j.o;
}

}  // namespace tribes
