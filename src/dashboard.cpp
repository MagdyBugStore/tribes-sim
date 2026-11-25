// dashboard.cpp: see dashboard.hpp.
#include "dashboard.hpp"

#include <cstdio>

#include "money.hpp"

namespace tribes {

Dashboard& dashboard() {
    static Dashboard d;
    return d;
}

void Dashboard::clear() {
    std::lock_guard<std::mutex> g(mu_);
    days_.clear(); caravans_.clear(); events_.clear(); track_.clear();
    item_names_.clear();
    caravan_seq_ = event_seen_ = 0;
}

int Dashboard::last_day() const {
    std::lock_guard<std::mutex> g(mu_);
    return days_.empty() ? -1 : days_.back().day;
}

static double sum_matching(const std::map<std::string, double>& m, const char* needle) {
    double s = 0;
    for (const auto& kv : m)
        if (kv.first.find(needle) != std::string::npos) s += kv.second;
    return s;
}

void Dashboard::record(const World& w) {
    std::lock_guard<std::mutex> g(mu_);
    if (item_names_.empty()) {
        for (const auto& d : w.data.items) {
            item_names_.push_back(d.name);
            item_units_.push_back(d.unit);
            item_base_.push_back(d.base_price);
        }
        for (int i = 0; i < w.n_items; ++i)
            if (w.is_livestock(i)) {
                livestock_.push_back(i);
                livestock_names_.push_back(w.data.items[static_cast<size_t>(i)].name);
            }
        tribe_names_ = w.tribe_names;
        prev_born_.assign(livestock_.size(), 0);
        prev_slaughtered_.assign(livestock_.size(), 0);
        for (size_t k = 0; k < livestock_.size(); ++k) {
            const auto& b = w.books[static_cast<size_t>(livestock_[k])];
            auto it = b.created_by.find("birth");
            prev_born_[k] = it == b.created_by.end() ? 0 : it->second;
            prev_slaughtered_[k] = sum_matching(b.destroyed_by, "slaughter");
        }
    }
    if (!days_.empty() && days_.back().day >= w.tick) return;   // one row per day
    DayRec r;
    r.day = w.tick;
    for (const auto& p : w.people)
        if (p.alive) {
            ++r.pop;
            r.pouches += p.pouch;
            if (p.sick) ++r.sick;
        }
    for (const auto& s : w.shops) r.tills += s.till;
    for (const auto& c : w.caravans) r.purses += c.purse;
    r.bayt = w.bayt.balance;
    r.other = w.majlis.balance + w.mint.balance;
    r.money = total_money(w);
    r.gini = w.gini();
    r.hungry = w.hungry_today; r.thirsty = w.thirsty_today;
    r.births = w.births_today; r.deaths = w.deaths_today;
    r.wells = w.majlis.wells_dug;
    r.arrived = w.caravans_arrived; r.failed = w.caravans_failed;
    const size_t ni = item_names_.size();
    r.price.assign(ni, 0); r.stock.assign(ni, 0); r.use.assign(ni, 0);
    for (size_t i = 0; i < ni; ++i) {
        double sum = 0, st = 0;
        int n = 0;
        for (const auto& sh : w.shops) {
            if (sh.sells(static_cast<int>(i))) { sum += sh.ask(static_cast<int>(i)); ++n; }
            st += sh.units(static_cast<int>(i));
        }
        r.price[i] = n ? static_cast<float>(sum / n) : 0.f;
        r.stock[i] = static_cast<float>(st);
        r.use[i] = static_cast<float>(w.books[i].consumed_today);
    }
    const size_t nl = livestock_.size(), nt = tribe_names_.size();
    r.herd_total.assign(nl, 0); r.herd_born.assign(nl, 0); r.herd_slaughtered.assign(nl, 0);
    r.herd_tribe.assign(nt, std::vector<float>(nl, 0.f));
    for (const auto& p : w.people) {
        if (!p.alive || static_cast<size_t>(p.tribe) >= nt) continue;
        for (size_t k = 0; k < nl; ++k) {
            const size_t it = static_cast<size_t>(livestock_[k]);
            if (it < p.stock.size()) {
                r.herd_total[k] += static_cast<float>(p.stock[it]);
                r.herd_tribe[static_cast<size_t>(p.tribe)][k] += static_cast<float>(p.stock[it]);
            }
        }
    }
    for (size_t k = 0; k < nl; ++k) {
        const auto& b = w.books[static_cast<size_t>(livestock_[k])];
        auto it = b.created_by.find("birth");
        const double born = it == b.created_by.end() ? 0 : it->second;
        const double sl = sum_matching(b.destroyed_by, "slaughter");
        r.herd_born[k] = static_cast<float>(born - prev_born_[k]);
        r.herd_slaughtered[k] = static_cast<float>(sl - prev_slaughtered_[k]);
        prev_born_[k] = born; prev_slaughtered_[k] = sl;
    }
    days_.push_back(std::move(r));
    while (days_.size() > DASH_DAYS) days_.pop_front();

    // caravan log: one entry per visit, closed when the company leaves
    if (track_.size() < w.caravans.size()) track_.resize(w.caravans.size());
    for (size_t i = 0; i < w.caravans.size(); ++i) {
        const auto& c = w.caravans[i];
        Track& t = track_[i];
        const bool in = c.state == CaravanState::InValley;
        if (in && !t.in_valley) {
            CaravanLog l;
            l.seq = ++caravan_seq_;
            l.id = c.id; l.day = w.tick; l.origin = c.origin;
            double units = 0, best = 0;
            for (size_t k = 0; k < c.cargo.size(); ++k) {
                units += c.cargo[k];
                if (c.cargo[k] > best) { best = c.cargo[k]; l.top_cargo = item_names_[k]; }
            }
            l.cargo_units = units;
            l.failed = c.failed > t.failed0;
            t.sold0 = c.sold_value; t.bought0 = c.bought_value; t.failed0 = c.failed;   // failed0: after the comparison above
            caravans_.push_back(l);
            t.log_index = l.seq;
        }
        if (in) {
            for (auto& l : caravans_)
                if (l.seq == t.log_index) {
                    l.sold = c.sold_value - t.sold0;
                    l.bought = c.bought_value - t.bought0;
                    l.stay = w.tick - l.day + 1;
                }
        }
        t.in_valley = in;
    }
    while (caravans_.size() > 200) caravans_.pop_front();

    // events: whatever was added since last time
    size_t fresh = w.events.total - event_seen_;
    if (fresh > w.events.entries.size()) fresh = w.events.entries.size();
    for (size_t k = w.events.entries.size() - fresh; k < w.events.entries.size(); ++k)
        events_.push_back({event_seen_ + (k - (w.events.entries.size() - fresh)) + 1, w.events.entries[k], w.tick});
    event_seen_ = w.events.total;
    while (events_.size() > 400) events_.pop_front();
}

namespace {
void esc(std::string& o, const std::string& s) {
    o += '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += static_cast<char>(c); }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += static_cast<char>(c);
    }
    o += '"';
}
void nums(std::string& o, const std::vector<float>& v, const char* fmt) {
    char b[32];
    o += '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) o += ',';
        std::snprintf(b, sizeof b, fmt, static_cast<double>(v[i]));
        o += b;
    }
    o += ']';
}
}  // namespace

std::string Dashboard::json(int since_day, size_t since_caravan, size_t since_event) const {
    std::lock_guard<std::mutex> g(mu_);
    std::string o;
    o.reserve(1 << 18);
    char b[160];
    auto strs = [&](const std::vector<std::string>& v) {
        o += '[';
        for (size_t i = 0; i < v.size(); ++i) { if (i) o += ','; esc(o, v[i]); }
        o += ']';
    };
    o += "{\"items\":"; strs(item_names_);
    o += ",\"units\":"; strs(item_units_);
    o += ",\"base\":[";
    for (size_t i = 0; i < item_base_.size(); ++i) { if (i) o += ','; std::snprintf(b, sizeof b, "%.2f", item_base_[i]); o += b; }
    o += "],\"livestock\":"; strs(livestock_names_);
    o += ",\"tribes\":"; strs(tribe_names_);
    o += ",\"days\":[";
    bool first = true;
    for (const auto& r : days_) {
        if (r.day <= since_day) continue;
        if (!first) o += ',';
        first = false;
        std::snprintf(b, sizeof b, "{\"d\":%d,\"pop\":%d,\"hungry\":%d,\"thirsty\":%d,\"sick\":%d,\"births\":%d,\"deaths\":%d,\"wells\":%d,\"arrived\":%d,\"failed\":%d,",
                      r.day, r.pop, r.hungry, r.thirsty, r.sick, r.births, r.deaths, r.wells, r.arrived, r.failed);
        o += b;
        std::snprintf(b, sizeof b, "\"money\":%.2f,\"pouches\":%.2f,\"bayt\":%.2f,\"tills\":%.2f,\"purses\":%.2f,\"other\":%.2f,\"gini\":%.4f,",
                      r.money, r.pouches, r.bayt, r.tills, r.purses, r.other, r.gini);
        o += b;
        o += "\"price\":"; nums(o, r.price, "%.2f");
        o += ",\"stock\":"; nums(o, r.stock, "%.0f");
        o += ",\"use\":"; nums(o, r.use, "%.1f");
        o += ",\"ht\":"; nums(o, r.herd_total, "%.0f");
        o += ",\"hb\":"; nums(o, r.herd_born, "%.0f");
        o += ",\"hs\":"; nums(o, r.herd_slaughtered, "%.0f");
        o += ",\"hx\":[";
        for (size_t t = 0; t < r.herd_tribe.size(); ++t) { if (t) o += ','; nums(o, r.herd_tribe[t], "%.0f"); }
        o += "]}";
    }
    o += "],\"caravans\":[";
    first = true;
    for (const auto& c : caravans_) {
        if (c.seq <= since_caravan) continue;
        if (!first) o += ',';
        first = false;
        std::snprintf(b, sizeof b, "{\"seq\":%zu,\"id\":%d,\"day\":%d,\"stay\":%d,\"cargo_units\":%.0f,\"sold\":%.2f,\"bought\":%.2f,\"failed\":%s,\"origin\":",
                      c.seq, c.id, c.day, c.stay, c.cargo_units, c.sold, c.bought, c.failed ? "true" : "false");
        o += b;
        esc(o, c.origin);
        o += ",\"top\":"; esc(o, c.top_cargo);
        o += '}';
    }
    o += "],\"events\":[";
    first = true;
    for (const auto& e : events_) {
        if (e.seq <= since_event) continue;
        if (!first) o += ',';
        first = false;
        std::snprintf(b, sizeof b, "{\"seq\":%zu,\"day\":%d,\"text\":", e.seq, e.day);
        o += b;
        esc(o, e.text);
        o += '}';
    }
    std::snprintf(b, sizeof b, "],\"last_day\":%d}", days_.empty() ? -1 : days_.back().day);
    o += b;
    return o;
}

}  // namespace tribes
