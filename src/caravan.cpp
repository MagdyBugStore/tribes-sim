// caravan.cpp: the trade routes. The caravans are the valley's ONLY source
// of grain (beyond the small fields), cloth, iron, spice, sugar, coffee,
// oil, pottery and silk. They buy the local surplus: dates, salt, wool,
// leather, cheese, rugs, camels. They can be late, small, or fail - and a
// failed caravan is felt in every tent.
#include <cmath>
#include <cstdio>

#include "world.hpp"

namespace tribes {

static constexpr double CARAVAN_TOLL = 0.05;

// Load cargo abroad: an import appears (a named process) - the company paid
// for it in foreign coin on the road, outside the valley's dirham books.
static void load_cargo(World& w, CaravanCo& c, bool small, bool lost) {
    // The company can only sell what its purse can later buy back in valley
    // goods: cargo value is capped by the purse, so a caravan never drains
    // more dirhams from the valley than it brings. Abroad, the valley's
    // exports were sold for foreign coin and turned into this cargo.
    double cap_value = c.purse * 0.9 + 300.0 * w.scale;
    double total = 0;
    for (const auto& d : w.data.items) {
        double qty = d.import_per_caravan * w.scale;
        if (qty <= 0) continue;
        if (small) qty *= 0.5;
        if (lost) qty *= 0.1;
        total += qty * d.base_price * 1.5;  // expected sale price
    }
    double k = total > cap_value ? cap_value / total : 1.0;
    for (int i = 0; i < w.n_items; ++i) {
        c.cargo[static_cast<size_t>(i)] = 0;
        const ItemDef& d = w.data.items[static_cast<size_t>(i)];
        double qty = d.import_per_caravan * w.scale * k;
        if (qty <= 0) continue;
        if (small) qty *= 0.5;
        if (lost) qty *= 0.1;
        stock_create(w, h_caravan(c.id), i, qty, "import");
    }
}

// Sell the cargo to the caravan souks of every market.
static void sell_cargo(World& w, CaravanCo& c) {
    for (auto& s : w.shops) {
        if (s.kind != ShopKind::CaravanSouk) continue;
        for (int i = 0; i < w.n_items; ++i) {
            double qty = c.cargo[static_cast<size_t>(i)];
            if (qty <= 0 || !s.sells(i)) continue;
            const ItemDef& d = w.data.items[static_cast<size_t>(i)];
            // Import price: valley base plus freight and the trader's margin.
            double unit = fils(d.base_price * 1.30 * 1.15 *
                               randd(0.95, 1.05));
            unit = clampd(unit, d.base_price * d.price_floor,
                          d.base_price * d.price_ceil);
            // Each souk takes a share of the cargo, as its till allows.
            double want = qty / 6.0 + s.target[static_cast<size_t>(i)];
            double units = std::min(qty, want);
            double cost = fils(units * unit);
            if (s.till < cost) {
                units = std::floor(s.till / unit * 100.0) / 100.0;
                cost = fils(units * unit);
            }
            if (units <= 0.005) continue;
            double toll = fils(cost * CARAVAN_TOLL);
            if (!transfer(w, shop_acc(s.id), majlis_acc(), toll,
                          "caravan toll", false))
                continue;
            if (transfer(w, shop_acc(s.id), caravan_acc(c.id),
                         fils(cost - toll), "caravan goods: " + d.name,
                         false)) {
                move_stock(w, i, units, h_caravan(c.id), h_shop(s.id),
                           "caravan goods: " + d.name, false);
                c.sold_value += cost;
            } else {
                transfer(w, majlis_acc(), shop_acc(s.id), toll,
                         "toll refund", false);
            }
        }
    }
}

// The date caravan buys straight from the groves: the valley's great
// surplus never sits in shop shelves, it waits in the Majlis's stores.
// Pays the Majlis, the groves' owner - this is the council's real income.
static void buy_from_groves(World& w, CaravanCo& c, double& budget) {
    const int di = w.ix.dates;
    const ItemDef& d = w.data.items[static_cast<size_t>(di)];
    if (d.export_per_caravan <= 0 || budget <= 0) return;
    for (auto& g : w.groves) {
        if (budget <= 0) break;
        double stock = g.stock[static_cast<size_t>(di)];
        // Keep a local reserve: the valley eats first.
        double reserve = 90.0 * w.scale * 30.0;
        double avail = stock - reserve;
        if (avail <= 0) continue;
        double units = std::min(avail, d.export_per_caravan * w.scale / 3.0);
        double unit = fils(std::max(d.base_price * d.price_floor,
                                    d.base_price * 0.85));  // wholesale
        double cost = fils(units * unit);
        if (cost > budget) {
            units = std::floor(budget / unit * 100.0) / 100.0;
            cost = fils(units * unit);
        }
        if (units <= 0.005) continue;
        double toll = fils(cost * CARAVAN_TOLL);
        if (!transfer(w, caravan_acc(c.id), majlis_acc(), toll,
                      "caravan toll", false))
            continue;
        if (transfer(w, caravan_acc(c.id), bayt_acc(),
                     fils((cost - toll) * 0.5), "dates for the north",
                     false)) {
            transfer(w, caravan_acc(c.id), majlis_acc(),
                     fils((cost - toll) * 0.5), "grove share of the majlis",
                     false);
            move_stock(w, di, units, h_grove(g.id), h_caravan(c.id),
                       "dates for the north", false);
            c.cargo[static_cast<size_t>(di)] -= units;  // straight to the hold
            c.bought[static_cast<size_t>(di)] += units;
            c.bought_value += cost;
            budget -= cost;
        }
    }
}

// Buy the local surplus for the road home.
static void buy_surplus(World& w, CaravanCo& c) {
    // The company spends roughly what it earned (less its margin), so trade
    // does not drain the valley's dirhams away over the years.
    double budget = c.purse * 0.9;  // earnings buy the load home
    buy_from_groves(w, c, budget);
    for (auto& s : w.shops) {
        for (int i = 0; i < w.n_items; ++i) {
            if (budget <= 0) break;
            const ItemDef& d = w.data.items[static_cast<size_t>(i)];
            if (d.export_per_caravan <= 0 || !s.sells(i)) continue;
            double surplus =
                s.units(i) - s.target[static_cast<size_t>(i)] * 0.7;
            if (surplus <= 0) continue;
            double units =
                std::min(surplus, d.export_per_caravan * w.scale / 3.0);
            double unit = fils(s.ask(i) * 0.9);  // export wholesale
            double cost = fils(units * unit);
            if (cost > budget) {
                units = std::floor(budget / unit * 100.0) / 100.0;
                cost = fils(units * unit);
            }
            if (units <= 0.005) continue;
            double toll = fils(cost * CARAVAN_TOLL);
            if (!transfer(w, caravan_acc(c.id), majlis_acc(), toll,
                          "caravan toll", false))
                continue;
            if (transfer(w, caravan_acc(c.id), shop_acc(s.id),
                         fils(cost - toll), "export: " + d.name, false)) {
                move_stock(w, i, units, h_shop(s.id), h_caravan(c.id),
                           "export: " + d.name, false);
                // Slide the purchase into the home-bound hold (same owner).
                c.cargo[static_cast<size_t>(i)] -= units;
                c.bought[static_cast<size_t>(i)] += units;
                c.bought_value += cost;
                budget -= cost;
            } else {
                transfer(w, majlis_acc(), caravan_acc(c.id), toll,
                         "toll refund", false);
            }
        }
    }
}

// Hire the local guards, drivers and guides for the trip.
static void hire_locals(World& w, CaravanCo& c) {
    double pot = 120.0 * w.scale;
    for (auto& p : w.people) {
        if (pot <= 1) break;
        if (!p.alive) continue;
        if (p.role != "Faris" && p.role != "CamelDriver" &&
            p.role != "Caravaneer" && p.role != "Dalil")
            continue;
        double pay = std::min(pot, 4.0);
        if (transfer(w, caravan_acc(c.id), person_acc(p.id), pay,
                     "caravan hire", false)) {
            pot -= pay;
            p.honor += 0.05;
        }
    }
}

void caravans_daily(World& w) {
    const int doy = w.tick % 360;
    const bool summer = doy >= 150 && doy < 240;
    for (auto& c : w.caravans) {
        if (c.state == CaravanState::Away) {
            if (w.tick < c.next_arrival) continue;
            // Arrival: small, late or ruined cargo are the desert's luck.
            bool small = chance(0.15);
            bool lost = chance(0.08);
            load_cargo(w, c, small, lost);
            c.state = CaravanState::InValley;
            c.days_in_valley = 0;
            c.trips++;
            w.caravans_arrived++;
            if (lost) {
                c.failed++;
                w.caravans_failed++;
                w.events.add("Day " + std::to_string(w.tick) +
                             ": the caravan from " + c.origin +
                             " arrives ruined - the sands took the cargo");
            } else {
                w.events.add("Day " + std::to_string(w.tick) +
                             ": a caravan from " + c.origin +
                             (small ? " arrives, a small one" : " arrives"));
            }
            hire_locals(w, c);
        } else {
            // Trading in the valley for two days.
            sell_cargo(w, c);
            buy_surplus(w, c);
            c.days_in_valley++;
            if (c.days_in_valley >= 2) {
                // Departure: what was bought leaves the valley's books, and
                // so does whatever of the cargo went unsold - the company
                // carries it back over the sands.
                for (int i = 0; i < w.n_items; ++i) {
                    double qty = c.bought[static_cast<size_t>(i)];
                    if (qty > 0) {
                        // move to cargo then destroy as export
                        c.cargo[static_cast<size_t>(i)] +=
                            c.bought[static_cast<size_t>(i)];
                        c.bought[static_cast<size_t>(i)] = 0;
                        stock_destroy(w, h_caravan(c.id), i, qty, "export");
                    }
                    double left = c.cargo[static_cast<size_t>(i)];
                    if (left > 0)
                        stock_destroy(w, h_caravan(c.id), i, left,
                                      "export: unsold cargo home");
                }
                c.state = CaravanState::Away;
                int gap = randi(10, 20) + (summer ? randi(6, 14) : 0);
                if (chance(0.15)) gap += randi(5, 10);  // the late ones
                c.next_arrival = w.tick + gap;
            }
        }
    }
}

}  // namespace tribes
