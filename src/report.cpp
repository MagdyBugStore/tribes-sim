// report.cpp: everything the simulation says about itself - the daily
// summary, the end report with the full stock reconciliation, and the
// console descriptions (person, family, status, history, top, tribes,
// economy, market, trade, chain).
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>

#include "world.hpp"

namespace tribes {

static std::string fmt(const char* f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string World::daily_summary() {
    int alive = 0, sick = 0;
    double cash = 0;
    for (auto& p : people)
        if (p.alive) {
            ++alive;
            cash += p.pouch;
            if (p.sick) ++sick;
        }
    return fmt("Day %4d | pop %d | dirhams %s (constant OK) | pouches %s / "
               "bayt %s | gini %.3f | hungry %d thirsty %d | sick %d | "
               "births +%d (%d) | deaths -%d (%d) | wells %d | caravans %d "
               "(%d failed)",
               tick, alive, commas(total_money(*this)).c_str(),
               commas(cash).c_str(), commas(bayt.balance).c_str(), gini(),
               hungry_today, thirsty_today, sick, births_today, births_total,
               deaths_today, deaths_total, majlis.wells_dug,
               caravans_arrived, caravans_failed);
}

std::string World::describe_person(int pid) {
    Person* p = find_person(pid);
    if (!p) return "No person with id " + std::to_string(pid) + ".";
    if (!p->alive)
        return fmt("%d - the late %s (%s, %s)\n  died on day %d of %s, at "
                   "the age of %d.",
                   p->id, full_name(*p).c_str(), p->gender_label(),
                   tribe_names[static_cast<size_t>(p->tribe)].c_str(),
                   p->death_day, p->death_cause.c_str(), p->age());
    std::string spouse_txt = "single";
    if (p->spouse >= 0) {
        Person& s = people[static_cast<size_t>(p->spouse)];
        spouse_txt = fmt("%d (%s)", s.id, s.name.c_str());
    } else if (p->widowed_since >= 0) {
        spouse_txt = "widowed";
    }
    std::string kids;
    int shown = 0;
    for (int c : p->children)
        if (shown++ < 10) kids += std::to_string(c) + " ";
    if (kids.empty()) kids = "none";
    std::string parents = "unknown";
    if (p->father >= 0 || p->mother >= 0) {
        parents.clear();
        if (p->father >= 0)
            parents += fmt("%d (%s) ", p->father,
                           people[static_cast<size_t>(p->father)].name.c_str());
        if (p->mother >= 0)
            parents += fmt("%d (%s)", p->mother,
                           people[static_cast<size_t>(p->mother)].name.c_str());
    }
    std::string expecting;
    if (p->pregnant_since != -1)
        expecting = fmt("   expecting (day %d/%d)", tick - p->pregnant_since,
                        PREGNANCY_DAYS);
    return fmt("%d - %s\n"
               "  gender: %s   age: %d (%s)   role: %s%s   tribe: %s\n"
               "  spouse: %s%s%s\n"
               "  children (%d): %s\n"
               "  parents: %s%s",
               p->id, full_name(*p).c_str(), p->gender_label(), p->age(),
               p->stage_name(), p->role.c_str(),
               p->retired ? " (retired)" : "",
               tribe_names[static_cast<size_t>(p->tribe)].c_str(),
               spouse_txt.c_str(), expecting.c_str(),
               p->orphan ? "   ORPHAN" : "",
               static_cast<int>(p->children.size()), kids.c_str(),
               parents.c_str(), "");
}

std::string World::describe_family(int pid) {
    Person* p = find_person(pid);
    if (!p) return "No person with id " + std::to_string(pid) + ".";
    auto line = [&](int id) {
        if (id < 0) return std::string("  (none)");
        Person& q = people[static_cast<size_t>(id)];
        return fmt("  %-5d %-28s %-7s age %-3d %-12s %s%s", q.id,
                   q.name.c_str(), q.gender_label(), q.age(), q.role.c_str(),
                   tribe_names[static_cast<size_t>(q.tribe)].c_str(),
                   q.alive ? "" : "  [late]");
    };
    std::string out = "The household of " + std::to_string(p->id) + " - " +
                      full_name(*p) + ":\n Parents:\n";
    if (p->father < 0 && p->mother < 0) out += "  (unknown)\n";
    if (p->father >= 0) out += line(p->father) + "\n";
    if (p->mother >= 0) out += line(p->mother) + "\n";
    out += " Spouse:\n" + line(p->spouse) + "\n";
    // Siblings share a parent.
    std::set<int> sibs;
    for (int par : {p->father, p->mother})
        if (par >= 0)
            for (int c : people[static_cast<size_t>(par)].children)
                if (c != pid) sibs.insert(c);
    out += " Siblings (" + std::to_string(sibs.size()) + "):\n";
    int shown = 0;
    for (int s : sibs)
        if (shown++ < 12) out += line(s) + "\n";
    if (sibs.empty()) out += "  (none)\n";
    out += " Children (" + std::to_string(p->children.size()) + "):\n";
    if (p->children.empty()) out += "  (none)\n";
    for (int c : p->children) out += line(c) + "\n";
    return out;
}

std::string World::describe_status(int pid) {
    Person* p = find_person(pid);
    if (!p) return "No person with id " + std::to_string(pid) + ".";
    std::string goods;
    for (int i = 0; i < n_items; ++i)
        if (p->stock[static_cast<size_t>(i)] >= 0.5)
            goods += fmt("%s %.0f; ", data.items[static_cast<size_t>(i)].name.c_str(),
                         p->stock[static_cast<size_t>(i)]);
    if (goods.empty()) goods = "nothing";
    std::string spouse_txt = p->spouse >= 0
                                 ? people[static_cast<size_t>(p->spouse)].name
                                 : (p->widowed_since >= 0 ? "widowed" : "single");
    return fmt("%d - %s (%s, %s, %s)\n"
               "  spouse: %s\n"
               "  pouch: %.0f   wadiah: %.0f   qard: %.0f\n"
               "  hunger: %.0f   thirst: %.0f   energy: %.0f\n"
               "  health: %.0f   happiness: %.0f   honor: %.1f   skill: %.1f\n"
               "  sick: %s\n"
               "  owns: %s",
               p->id, full_name(*p).c_str(), p->gender_label(),
               p->role.c_str(), tribe_names[static_cast<size_t>(p->tribe)].c_str(),
               spouse_txt.c_str(), p->pouch, p->wadiah, p->qard, p->hunger,
               p->thirst, p->energy, p->health, p->happiness, p->honor,
               p->skill, p->sick ? "yes" : "no", goods.c_str());
}

std::string World::describe_history(int pid, int n) {
    Person* p = find_person(pid);
    if (!p) return "No person with id " + std::to_string(pid) + ".";
    if (p->history.empty())
        return std::to_string(pid) + " has no recorded events yet.";
    std::string out = "Last events for " + std::to_string(pid) + " (" +
                      full_name(*p) + "):\n";
    int shown = 0;
    for (auto it = p->history.rbegin();
         it != p->history.rend() && shown < n; ++it, ++shown)
        out += "  " + *it + "\n";
    return out;
}

std::string World::describe_top(int n) {
    std::vector<const Person*> alive;
    for (auto& p : people)
        if (p.alive) alive.push_back(&p);
    std::sort(alive.begin(), alive.end(),
              [](const Person* a, const Person* b) {
                  return a->wealth() > b->wealth();
              });
    std::string out = "Top " + std::to_string(std::min<int>(n, alive.size())) +
                      " richest people:\n";
    for (int i = 0; i < n && i < (int)alive.size(); ++i)
        out += fmt("  %-6d %-30s %10s\n", alive[static_cast<size_t>(i)]->id,
                   alive[static_cast<size_t>(i)]->name.c_str(),
                   commas(alive[static_cast<size_t>(i)]->wealth()).c_str());
    return out;
}

std::string World::describe_tribes() {
    std::string out = "The tribes of the valley:\n";
    for (size_t t = 0; t < tribe_names.size(); ++t) {
        int n = 0;
        double wealth = 0, honor = 0;
        for (auto& p : people)
            if (p.alive && p.tribe == (int)t) {
                ++n;
                wealth += p.wealth();
                honor += p.honor;
            }
        if (!n) continue;
        std::string allies;
        for (size_t o = 0; o < tribe_names.size(); ++o)
            if (o != t && allied((int)t, (int)o))
                allies += tribe_names[o] + " ";
        out += fmt("  %-16s %4d people   avg wealth %8s   avg honor %5.1f   "
                   "allies: %s\n",
                   tribe_names[t].c_str(), n,
                   commas(wealth / n).c_str(), honor / n,
                   allies.empty() ? "none" : allies.c_str());
    }
    return out;
}

// economy: the stock table - item, total, per owner type, price, trend.
std::string World::describe_economy() {
    std::string out = "The valley's economy (item | total | households | "
                      "shops | institutions | ground | price | trend):\n";
    for (int i = 0; i < n_items; ++i) {
        const ItemDef& d = data.items[static_cast<size_t>(i)];
        if (d.kind == "land") continue;
        double hh = 0, sh = 0, tot = total_holdings(*this, i);
        if (tot < 0.5) continue;
        for (auto& p : people)
            if (p.alive) hh += p.stock[static_cast<size_t>(i)];
        double price = 0, prev = 0;
        int np = 0;
        for (auto& s : shops) {
            sh += s.stock[static_cast<size_t>(i)];
            if (s.sells(i)) {
                price += s.ask(i);
                auto& h = s.price_hist[static_cast<size_t>(i)];
                if (h.size() > 5) prev += h.front();
                ++np;
            }
        }
        if (np) {
            price /= np;
            prev /= np;
        }
        std::string trend = "=";
        if (prev > 0 && price > prev * 1.03) trend = "rising";
        if (prev > 0 && price < prev * 0.97) trend = "falling";
        double inst = bayt.stock[static_cast<size_t>(i)] +
                      majlis.stock[static_cast<size_t>(i)];
        double ground = tot - hh - sh - inst;
        for (auto& c : caravans) ground -= c.cargo[static_cast<size_t>(i)];
        out += fmt("  %-10s %9s %-5s | hh %9s | shops %8s | inst %7s | "
                   "other %8s | %6.2f %s\n",
                   d.name.c_str(), commas(tot).c_str(), d.unit.c_str(),
                   commas(hh).c_str(), commas(sh).c_str(),
                   commas(inst).c_str(), commas(std::max(0.0, ground)).c_str(),
                   price, trend.c_str());
    }
    out += fmt("  Wells: %d dug, yielding %.0f L/day | caravans arrived %d "
               "(%d failed)\n",
               majlis.wells_dug,
               [&] { double y = 0; for (auto& x : wells) y += x.yield_day; return y; }(),
               caravans_arrived, caravans_failed);
    return out;
}

std::string World::describe_market() {
    std::string out = "The suq (average across the markets; change over 30 "
                      "days):\n";
    for (int i = 0; i < n_items; ++i) {
        const ItemDef& d = data.items[static_cast<size_t>(i)];
        double price = 0, old = 0, stock = 0;
        int n = 0;
        for (auto& s : shops) {
            if (!s.sells(i)) continue;
            price += s.ask(i);
            auto& h = s.price_hist[static_cast<size_t>(i)];
            old += h.size() > 5 ? h.front() : s.ask(i);
            stock += s.units(i);
            ++n;
        }
        if (!n) continue;
        price /= n;
        old /= n;
        out += fmt("  %-10s stock %8s %-6s price %6.2f (%+.0f%%)  [%4.2f .. "
                   "%5.2f]\n",
                   d.name.c_str(), commas(stock).c_str(), d.unit.c_str(),
                   price, old > 0 ? (price / old - 1) * 100 : 0,
                   d.base_price * d.price_floor,
                   d.base_price * d.price_ceil);
    }
    return out;
}

std::string World::describe_trade() {
    std::string out = "The trade of the valley:\n  Caravans:\n";
    for (auto& c : caravans) {
        out += fmt("    %-8s %s, trips %d (%d failed), sold %s, bought %s, "
                   "purse %s%s\n",
                   c.origin.c_str(),
                   c.state == CaravanState::InValley
                       ? "IN THE VALLEY"
                       : fmt("next in %d days", c.next_arrival - tick).c_str(),
                   c.trips, c.failed, commas(c.sold_value).c_str(),
                   commas(c.bought_value).c_str(), commas(c.purse).c_str(),
                   "");
    }
    out += "  Trade balance (dirhams): imports " +
           commas([&] { double v = 0; for (auto& c : caravans) v += c.sold_value; return v; }()) +
           " vs exports " +
           commas([&] { double v = 0; for (auto& c : caravans) v += c.bought_value; return v; }()) +
           "\n  Top imports (units):\n";
    std::vector<std::pair<double, std::string>> im, ex;
    for (int i = 0; i < n_items; ++i) {
        if (books[static_cast<size_t>(i)].imported > 0)
            im.push_back({books[static_cast<size_t>(i)].imported,
                          data.items[static_cast<size_t>(i)].name});
        if (books[static_cast<size_t>(i)].exported > 0)
            ex.push_back({books[static_cast<size_t>(i)].exported,
                          data.items[static_cast<size_t>(i)].name});
    }
    std::sort(im.rbegin(), im.rend());
    std::sort(ex.rbegin(), ex.rend());
    for (size_t k = 0; k < im.size() && k < 8; ++k)
        out += fmt("    %-10s %s\n", im[k].second.c_str(),
                   commas(im[k].first).c_str());
    out += "  Top exports (units):\n";
    for (size_t k = 0; k < ex.size() && k < 8; ++k)
        out += fmt("    %-10s %s\n", ex[k].second.c_str(),
                   commas(ex[k].first).c_str());
    return out;
}

// chain <good>: each stage for the good now - owners, stock, price, margin,
// workers, daily throughput.
std::string World::describe_chain(const std::string& good) {
    int item = data.item_index(good);
    if (item < 0) return "No such good. Try: bread, dates, milk, meat, wool, "
                         "leather, iron, salt, firewood.";
    const ItemDef& d = data.items[static_cast<size_t>(item)];
    std::string out = "The chain of " + good + " (" + d.unit +
                      ", base price " + fmt("%.2f", d.base_price) + "):\n";
    // Upstream stages: the recipes that make it, and what makes their
    // inputs, one level up.
    auto stage_line = [&](const RecipeDef& r, bool is_target) {
        std::string s = fmt("  stage %-16s (%s, worker %s): in ",
                            r.stage.c_str(), r.process.c_str(),
                            r.role.c_str());
        for (size_t k = 0; k < r.in_name.size(); ++k)
            s += fmt("%.1f %s ", r.in_qty[k], r.in_name[k].c_str());
        if (r.in_name.empty()) s += "(the land) ";
        s += "-> out ";
        for (size_t k = 0; k < r.out_name.size(); ++k)
            s += fmt("%.1f %s ", r.out_qty[k], r.out_name[k].c_str());
        // Workers and their stock of the target item.
        int workers = 0;
        double held = 0;
        for (auto& p : people)
            if (p.alive && p.role == r.role) {
                ++workers;
                held += p.stock[static_cast<size_t>(item)];
            }
        int out_item = is_target ? item : data.item_index(r.out_name.empty()
                                                              ? good
                                                              : r.out_name[0]);
        s += fmt("| %d workers, holding %.0f %s, made today %.1f", workers,
                 held, d.unit.c_str(),
                 out_item >= 0
                     ? books[static_cast<size_t>(out_item)].produced_today
                     : 0.0);
        return s + "\n";
    };
    // Land stages first.
    if (item == ix.bread || item == ix.flour || item == ix.wheat) {
        double standing = 0;
        int growing = 0;
        for (auto& f : fields) {
            standing += f.stock[static_cast<size_t>(ix.wheat)];
            if (f.growing) ++growing;
        }
        out += fmt("  stage fields          (farm, worker Fallah): %d fields "
                   "(%d growing), standing crop %.0f kg wheat\n",
                   (int)fields.size(), growing, standing);
    }
    if (item == ix.dates) {
        for (auto& g : groves)
            out += fmt("  stage grove            (harvest, worker Nakhali): "
                       "%.0f palms, store %.0f kg\n",
                       g.palms, g.stock[static_cast<size_t>(ix.dates)]);
    }
    if (item == ix.water)
        for (auto& wl : wells)
            out += fmt("  stage well             (draw, worker WellKeeper): "
                       "cistern %.0f L, yield %.0f L/day\n",
                       wl.stock[static_cast<size_t>(ix.water)], wl.yield_day);
    if (item == ix.milk || item == ix.meat || item == ix.wool ||
        item == ix.hide) {
        double heads = total_holdings(*this, ix.goat) +
                       total_holdings(*this, ix.sheep) +
                       total_holdings(*this, ix.camel);
        out += fmt("  stage herds            (breeding/milking): %.0f head "
                   "in the flocks\n",
                   heads);
    }
    bool any = false;
    for (auto& r : data.recipes) {
        bool makes = false;
        for (auto& o : r.out_name)
            if (o == good) makes = true;
        if (makes) {
            out += stage_line(r, true);
            any = true;
        }
    }
    if (!any && d.local && d.kind != "land")
        out += "  (made by the land and its keepers; see above)\n";
    if (!d.local) {
        out += "  not made in the valley - the caravans bring it: imported " +
               commas(books[static_cast<size_t>(item)].imported) + " " +
               d.unit + " so far\n";
    }
    // Retail: stock and price at the suq.
    double stock = 0, price = 0;
    int n = 0;
    for (auto& s : shops)
        if (s.sells(item)) {
            stock += s.units(item);
            price += s.ask(item);
            ++n;
        }
    if (n)
        out += fmt("  retail                 : suq stock %.0f %s at %.2f "
                   "dirhams (sold today: see market)\n",
                   stock, d.unit.c_str(), price / n);
    return out;
}

std::string World::end_report() {
    std::string out = "\n====================================================================\n";
    out += "END REPORT after " + std::to_string(tick) + " days of the tribes\n";
    out += "====================================================================\n";
    int alive = 0;
    for (auto& p : people)
        if (p.alive) ++alive;
    out += fmt("Population: %d in %d tribes | dirham supply %.2f (constant)\n",
               alive, (int)tribe_names.size(), total_money(*this));
    out += fmt("Gini: %.3f | weddings %d | births %d | deaths %d | wells %d "
               "| caravans %d (%d failed)\n\n",
               gini(), weddings_total, births_total, deaths_total,
               majlis.wells_dug, caravans_arrived, caravans_failed);
    out += describe_tribes() + "\n";
    out += describe_trade() + "\n";
    // The full stock reconciliation of section 4b.
    out += "Stock reconciliation (initial + imported + produced - exported - "
           "consumed - spoiled - used = holdings):\n";
    for (int i = 0; i < n_items; ++i) {
        const StockBook& b = books[static_cast<size_t>(i)];
        double holdings = total_holdings(*this, i);
        if (b.initial < 0.5 && holdings < 0.5 && b.produced < 0.5) continue;
        const ItemDef& d = data.items[static_cast<size_t>(i)];
        out += fmt("  %-10s %9.0f + %8.0f + %9.0f - %8.0f - %9.0f - %8.0f "
                   "- %9.0f = %10.1f %s (holdings %.1f OK)\n",
                   d.name.c_str(), b.initial, b.imported, b.produced,
                   b.exported, b.consumed, b.spoiled, b.used,
                   b.expected(), d.unit.c_str(), holdings);
    }
    out += "\nValue added by chain stage (dirhams):\n";
    std::vector<std::pair<double, std::string>> sv;
    for (auto& kv : stage_value) sv.push_back({kv.second, kv.first});
    std::sort(sv.rbegin(), sv.rend());
    for (size_t k = 0; k < sv.size() && k < 15; ++k)
        out += fmt("  %-28s %12s\n", sv[k].second.c_str(),
                   commas(sv[k].first).c_str());
    out += "\nDirham flow by reason:\n";
    std::vector<std::pair<double, std::string>> fl;
    for (auto& kv : money_flows) fl.push_back({kv.second, kv.first});
    std::sort(fl.rbegin(), fl.rend());
    for (size_t k = 0; k < fl.size() && k < 18; ++k)
        out += fmt("  %-30s %14s\n", fl[k].second.c_str(),
                   commas(fl[k].first).c_str());
    return out;
}

}  // namespace tribes
