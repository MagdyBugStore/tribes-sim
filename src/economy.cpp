// economy.cpp: the daily economic life of the valley - land and water,
// the production chains (section 4c), crafts, services, and everyone's
// shopping. Every unit moves by move_stock(), every dirham by transfer().
#include <cmath>
#include <cstdio>

#include "world.hpp"

namespace tribes {

// Wholesale share of the retail price when a shop buys from a producer.
static constexpr double WHOLESALE = 0.72;
static constexpr double KEEPER_WAGE = 10.0;
static constexpr double SHOP_RESERVE = 60.0;
static constexpr double AJER_WAGE = 5.0;
static constexpr double DEYN_LIMIT = 40.0;
// A craftsman's inputs are trade credit: the suq knows his output comes back
// to its own shelves, so it carries him much further than a household.
static constexpr double TRADE_CREDIT = 300.0;
static constexpr double SUQ_TOLL = 0.06;
static constexpr double USHR = 0.05;  // on production sales, to the Majlis

// --- small helpers -------------------------------------------------------------

// The best shop for `item` in the buyer's market, then allies, then anyone.
static Shop* pick_shop(World& w, int item, int buyer_tribe) {
    // Cheapest shop with stock, preferring the buyer's own market, then an
    // ally's, then anyone. Prices only move when the shops close, so the
    // answer is stable within a day: cache it and recheck only the stock.
    const auto key = std::make_pair(buyer_tribe, item);
    auto it = w.shop_pick_cache.find(key);
    if (it != w.shop_pick_cache.end()) {
        Shop& c = w.shops[static_cast<size_t>(it->second)];
        if (c.sells(item) && c.units(item) > 0) return &c;
        w.shop_pick_cache.erase(it);
    }
    Shop* best = nullptr;
    double best_price = 1e18;
    for (int pass = 0; pass < 3 && !best; ++pass) {
        for (auto& s : w.shops) {
            if (!s.sells(item) || s.units(item) <= 0) continue;
            if (pass == 0 && s.tribe != buyer_tribe) continue;
            if (pass == 1 && !w.allied(s.tribe, buyer_tribe)) continue;
            if (s.ask(item) < best_price) {
                best_price = s.ask(item);
                best = &s;
            }
        }
    }
    if (best) w.shop_pick_cache[key] = best->id;
    return best;
}

// A shop sells to a person: toll to the Majlis, price for allies, deyn for
// known customers who are short. Returns units sold.
double sell_to_person_pub(World& w, Shop& s, int item, double want,
                             Person& buyer, double credit_cap = DEYN_LIMIT) {
    const ItemDef& d = w.data.items[static_cast<size_t>(item)];
    double units = std::min(want, s.units(item));
    if (units <= 0) return 0;
    double unit = s.ask(item);
    if (w.allied(buyer.tribe, s.tribe)) unit = fils(unit * 0.95);
    double cost = fils(units * unit);
    bool credit = false;
    if (buyer.pouch < cost) {
        double owed = s.deyn[buyer.id];
        if (buyer.tribe == s.tribe && owed < credit_cap &&
            s.deyn_total < 400.0 * w.scale) {
            credit = true;
        } else {
            units = std::floor(buyer.pouch / unit * 100.0) / 100.0;
            cost = fils(units * unit);
            if (units <= 0) return 0;
        }
    }
    double toll = fils(cost * SUQ_TOLL);
    if (!credit) {
        if (!transfer(w, person_acc(buyer.id), majlis_acc(), toll,
                      "suq toll", false))
            return 0;
        if (!transfer(w, person_acc(buyer.id), shop_acc(s.id),
                      fils(cost - toll), w.reason_purchase[static_cast<size_t>(item)], false)) {
            transfer(w, majlis_acc(), person_acc(buyer.id), toll,
                     "toll refund", false);
            return 0;
        }
    } else {
        s.deyn[buyer.id] = fils(s.deyn[buyer.id] + cost);
        s.deyn_total = fils(s.deyn_total + cost);
        transfer(w, shop_acc(s.id), majlis_acc(), toll, "suq toll (deyn)",
                 false);
    }
    move_stock(w, item, units, h_shop(s.id), h_person(buyer.id),
               w.reason_purchase[static_cast<size_t>(item)], false);
    w.sales_units[static_cast<size_t>(item)] += units;
    w.sales_value[static_cast<size_t>(item)] += cost;
    return units;
}

// A shop buys from a person (producer sale): wholesale, ushr to the Majlis,
// limited by the till. Returns units bought.
static double buy_from_person(World& w, Shop& s, int item, double offer,
                              Person& seller, const char* stage) {
    const ItemDef& d = w.data.items[static_cast<size_t>(item)];
    if (offer <= 0 || !s.sells(item)) return 0;
    // Do not restock far beyond the target: demand-driven production.
    double room = s.target[static_cast<size_t>(item)] * 3.0 - s.units(item);
    if (room <= 0) return 0;
    double whole = fils(std::max(d.base_price * d.price_floor,
                                 s.ask(item) * WHOLESALE));
    double units = std::min(offer, room);
    double cost = fils(units * whole);
    if (s.till < cost) {
        units = std::floor(s.till / whole * 100.0) / 100.0;
        cost = fils(units * whole);
    }
    if (units <= 0.005) return 0;
    double ushr = fils(cost * USHR);
    if (!transfer(w, shop_acc(s.id), bayt_acc(), ushr, "ushr on produce",
                  false))
        return 0;
    if (!transfer(w, shop_acc(s.id), person_acc(seller.id),
                  fils(cost - ushr), w.reason_sale[static_cast<size_t>(item)], false)) {
        transfer(w, bayt_acc(), shop_acc(s.id), ushr, "ushr refund", false);
        return 0;
    }
    move_stock(w, item, units, h_person(seller.id), h_shop(s.id),
               w.reason_sale[static_cast<size_t>(item)], false);
    w.stage_value[std::string(stage) + "|" + d.name] += fils(cost - ushr);
    return units;
}

// Sell `offer` units into the most willing shop of the seller's market.
// (Shared with herds.cpp.)
double sell_to_market_pub(World& w, Person& seller, int item, double offer,
                          const char* stage) {
    double left = offer;
    const int nt = static_cast<int>(w.tribe_names.size());
    for (int pass = 0; pass < 2 && left > 0.005; ++pass)
        for (auto& s : w.shops) {
            if (left <= 0.005) break;
            if (!s.sells(item)) continue;
            if (pass == 0 && s.tribe != seller.tribe) continue;
            left -= buy_from_person(w, s, item, left, seller, stage);
        }
    (void)nt;
    return offer - left;
}

// --- land and water ---------------------------------------------------------------

void wells_and_groves(World& w) {
    const int day_of_year = w.tick % 360;
    for (auto& wl : w.wells) {
        // The aquifer recharges the cistern, up to its capacity.
        double cap = wl.yield_day * 20.0;
        double& st = wl.stock[static_cast<size_t>(w.ix.water)];
        double room = std::max(0.0, cap - st);
        stock_create(w, h_well(wl.id), w.ix.water, std::min(room, wl.yield_day),
                     "well recharge");
    }
    // The date harvest runs from late summer into autumn.
    bool season = day_of_year >= 200 && day_of_year < 290;
    for (auto& g : w.groves) {
        if (season) {
            double daily = g.palms * g.kg_per_palm_year / 90.0;
            stock_create(w, h_grove(g.id), w.ix.dates, daily, "harvest");
        }
        // The Majlis (owner of the groves) sells packed dates to the suq.
        for (auto& s : w.shops) {
            if (s.kind != ShopKind::FoodStall || s.tribe != g.tribe) continue;
            double room =
                s.target[static_cast<size_t>(w.ix.dates)] * 1.5 -
                s.units(w.ix.dates);
            if (room <= 0) continue;
            double units =
                std::min(room, g.stock[static_cast<size_t>(w.ix.dates)]);
            if (units <= 0) continue;
            const ItemDef& d = w.data.items[static_cast<size_t>(w.ix.dates)];
            double whole = fils(std::max(d.base_price * d.price_floor,
                                         s.ask(w.ix.dates) * WHOLESALE));
            double cost = fils(units * whole);
            double pay_now = std::min(cost, s.till);
            double on_credit = fils(cost - pay_now);
            if (pay_now > 0.005) {
                if (transfer(w, shop_acc(s.id), bayt_acc(),
                             fils(pay_now * 0.5), "dates from the grove",
                             false))
                    transfer(w, shop_acc(s.id), majlis_acc(),
                             fils(pay_now * 0.5), "grove share of the majlis",
                             false);
            }
            if (on_credit > 0.005)
                s.deyn_grove = fils(s.deyn_grove + on_credit);
            move_stock(w, w.ix.dates, units, h_grove(g.id), h_shop(s.id),
                       "dates from the grove", false);
        }
    }
}

// The wheat fields: sow in season, harvest after 120 days (section 4c, the
// worked example). Yield depends on water and luck; a bad year is possible.
void fields_daily(World& w) {
    const int doy = w.tick % 360;
    const bool sowing = doy < 60;
    for (auto& f : w.fields) {
        Person& farmer = w.people[static_cast<size_t>(f.owner)];
        if (!farmer.alive) continue;
        if (!f.growing) {
            if (sowing) {
                double seed = 60.0 * f.ha;  // kg of seed per hectare
                if (farmer.stock[static_cast<size_t>(w.ix.wheat)] >= seed) {
                    stock_destroy(w, h_person(f.owner), w.ix.wheat, seed,
                                  "seed sown");
                    f.growing = true;
                    f.sown_day = w.tick;
                }
            }
            continue;
        }
        if (w.tick - f.sown_day < 120) continue;
        // Harvest: standing crop becomes grain in the farmer's store.
        double water_factor = 0.7 + 0.3 * std::min(1.0, randd(0.4, 1.2));
        double luck = chance(0.08) ? randd(0.3, 0.55) : randd(0.85, 1.2);
        double kg = f.ha * f.yield_kg_ha * water_factor * luck;
        // The standing crop becomes grain: reap it (a named use), then the
        // harvest is created in the farmer's store.
        double standing = f.stock[static_cast<size_t>(w.ix.wheat)];
        if (standing > 0)
            stock_destroy(w, h_field(f.id), w.ix.wheat, standing,
                          "standing crop reaped");
        stock_create(w, h_person(f.owner), w.ix.wheat, kg, "harvest");
        f.growing = false;
        f.harvests++;
        // Keep seed and the family's own bread grain, sell the rest.
        double keep = 60.0 * f.ha + 250.0;
        double sell = std::max(
            0.0, farmer.stock[static_cast<size_t>(w.ix.wheat)] - keep);
        // Pest and spoilage loss on the stored share (a named loss).
        stock_destroy(w, h_person(f.owner), w.ix.wheat, sell * 0.03,
                      "spoilage");
        // Millers buy first (they pay from their pouches), then the suq.
        for (auto& p : w.people) {
            if (sell <= 0.005) break;
            if (p.role != "Miller" || !p.alive || p.tribe != farmer.tribe)
                continue;
            double want = 180.0 - p.stock[static_cast<size_t>(w.ix.wheat)];
            if (want <= 0) continue;
            double units = std::min(sell, want);
            double price = fils(w.data.items[static_cast<size_t>(w.ix.wheat)]
                                    .base_price *
                                randd(0.95, 1.1));
            double cost = fils(units * price);
            if (p.pouch < cost) {
                units = std::floor(p.pouch / price * 100.0) / 100.0;
                cost = fils(units * price);
            }
            if (units <= 0.005) continue;
            if (transfer(w, person_acc(p.id), person_acc(f.owner), cost,
                         "wheat for the mill", false)) {
                move_stock(w, w.ix.wheat, units, h_person(f.owner),
                           h_person(p.id), "wheat for the mill", false);
                w.stage_value["harvest|wheat"] += cost;
                sell -= units;
            }
        }
        if (sell > 0.005)
            sell_to_market_pub(w, farmer, w.ix.wheat, sell, "harvest");
    }
}

// --- crafts (the generic recipe runner, section 4c) --------------------------------

// Strength fades with years: full power to the mid-fifties, a long slope
// down to a quarter by seventy-five.
static double work_factor(const Person& p) {
    int a = p.age();
    if (a < 55) return 1.0;
    if (a >= 75) return 0.25;
    return 1.0 - 0.75 * (a - 55) / 20.0;
}

static void run_recipe(World& w, Person& p, const RecipeDef& r) {
    if (r.capacity <= 0) return;
    // How many runs can the inputs allow? Buy missing inputs from the suq.
    double runs = r.capacity * p.skill * work_factor(p) *
                  (0.6 + 0.4 * p.energy / 100.0);
    for (size_t k = 0; k < r.in_name.size(); ++k) {
        int item = w.data.item_index(r.in_name[k]);
        if (item < 0) return;
        double need = r.in_qty[k] * runs;
        double have = p.stock[static_cast<size_t>(item)];
        if (have < need) {
            // Livestock inputs (the butcher's animals) are bought from
            // herders directly; goods come from the shops.
            if (w.is_livestock(item)) {
                for (auto& h : w.people) {
                    if (h.stock[static_cast<size_t>(item)] < 1 ||
                        &h == &p || h.role == "Jazzar")
                        continue;
                    double price = fils(
                        w.data.items[static_cast<size_t>(item)].base_price *
                        randd(0.9, 1.15));
                    if (p.pouch < price) break;
                    if (transfer(w, person_acc(p.id), person_acc(h.id), price,
                                 "livestock for slaughter", false)) {
                        move_stock(w, item, 1, h_person(h.id),
                                   h_person(p.id), "livestock for slaughter",
                                   false);
                        have += 1;
                    }
                    if (have >= need) break;
                }
            } else {
                Shop* s = pick_shop(w, item, p.tribe);
                if (s)
                    have += sell_to_person_pub(w, *s, item, need - have, p,
                                               TRADE_CREDIT);
            }
        }
        runs = std::min(runs, have / r.in_qty[k]);
    }
    // Slaughter capacity is also limited by what the meat market absorbs.
    runs = std::floor(runs * 100.0) / 100.0;
    if (runs <= 0.0) return;
    // Check the main output's market has room; a craft does not run into a
    // glut. Byproducts (the miller's bran) never block the run: they pile up
    // or sell to whoever takes them.
    for (size_t k = 0; k < 1 && k < r.out_name.size(); ++k) {
        int item = w.data.item_index(r.out_name[k]);
        if (item < 0) continue;
        double room = 0;
        for (auto& s : w.shops)
            if (s.sells(item) && s.tribe == p.tribe)
                room += std::max(
                    0.0, s.target[static_cast<size_t>(item)] * 1.5 -
                             s.units(item));
        if (room < r.out_qty[k] * runs)
            runs = std::min(runs, room / r.out_qty[k]);
    }
    if (runs <= 0.001) return;
    // Consume the inputs (named use) and create the outputs (named craft).
    for (size_t k = 0; k < r.in_name.size(); ++k) {
        int item = w.data.item_index(r.in_name[k]);
        if (!stock_destroy(w, h_person(p.id), item, r.in_qty[k] * runs,
                           "input: " + r.stage))
            return;
    }
    for (size_t k = 0; k < r.out_name.size(); ++k) {
        int item = w.data.item_index(r.out_name[k]);
        stock_create(w, h_person(p.id), item, r.out_qty[k] * runs, r.stage);
    }
    // Sell the outputs into the suq.
    for (size_t k = 0; k < r.out_name.size(); ++k) {
        int item = w.data.item_index(r.out_name[k]);
        double qty = p.stock[static_cast<size_t>(item)];
        // The butcher and the miller keep nothing; families' crafters keep
        // a little of the food they make.
        if (item == w.ix.bread) qty = std::max(0.0, qty - 2.0);
        if (qty > 0.005) sell_to_market_pub(w, p, item, qty, r.stage.c_str());
    }
}

// --- the working day ----------------------------------------------------------------

void production(World& w) {
    for (auto& p : w.people) {
        if (!p.is_worker()) continue;
        p.energy = clampd(p.energy - 12, 0, 100);
        // Salaried roles are paid in majlis_daily/bayt_daily; here everyone
        // who works a chain or a craft does it.
        auto recipes = w.data.recipes_of(p.role);
        for (const RecipeDef* r : recipes) run_recipe(w, p, *r);
        // The well keeper draws water from the well and sells it at the suq.
        if (p.role == "WellKeeper") {
            Well* wl = nullptr;
            for (auto& x : w.wells)
                if (x.tribe == p.tribe) wl = &x;
            if (!wl) wl = &w.wells.front();
            double draw = std::min(1200.0 * p.skill * work_factor(p),
                                   wl->stock[static_cast<size_t>(w.ix.water)]);
            if (draw > 0 &&
                move_stock(w, w.ix.water, draw, h_well(wl->id),
                           h_person(p.id), "drawing water", false))
                sell_to_market_pub(w, p, w.ix.water, draw, "drawing");
        }
        // Ajers: day labor in the shop they help (wage from the till).
        if (p.role == "Ajer") {
            if (p.employer_shop >= 0) {
                Shop& s = w.shops[static_cast<size_t>(p.employer_shop)];
                double wage =
                    std::min(AJER_WAGE, std::max(0.0, s.till - SHOP_RESERVE));
                if (wage > 0.1)
                    transfer(w, shop_acc(s.id), person_acc(p.id), wage,
                             "day labor", false);
            } else if (w.majlis.balance > 200) {
                // No shop slot free: the Majlis always has wells to dig,
                // groves to tend, roads to keep. Public works at a leaner
                // wage, paid while the council can afford them.
                transfer(w, majlis_acc(), person_acc(p.id), 3.0,
                         "public works", false);
            }
        }
        // Khaddams serve the sheikhs' households and are fed and paid there.
        if (p.role == "Khaddam") {
            int best = -1;
            for (auto& q : w.people)
                if (q.alive && q.tribe == p.tribe &&
                    (q.role == "Sheikh" || q.role == "GrandSheikh") &&
                    (best < 0 || q.pouch > w.people[static_cast<size_t>(best)].pouch))
                    best = q.id;
            if (best >= 0)
                transfer(w, person_acc(best), person_acc(p.id), 5.0,
                         "household service", false);
        }
        p.skill = std::min(1.5, p.skill + 0.0002);
    }
}

// --- the shopping day ------------------------------------------------------------

// Family support: when a person cannot cover the day's food, the family
// steps in - spouse, adult children, parents. Every fils through transfer().
// The household pot: the family eats before anyone saves. Working members
// share with the dependents of their house - children, the old, the
// students - enough for the day's food. This is how the valley's food
// money reaches the mouths that do not earn.
static void family_pot(World& w, Person& p) {
    if (p.pouch <= 15.0) return;
    // Gather the dependents of this person's house.
    std::vector<int> deps;
    auto add_dep = [&](int id) {
        if (id < 0) return;
        Person& d = w.people[static_cast<size_t>(id)];
        if (!d.alive || d.id == p.id) return;
        if (d.is_worker()) return;  // earns his own
        if (d.pouch < 10.0) deps.push_back(id);
    };
    add_dep(p.spouse);
    for (int ch : p.children) add_dep(ch);
    add_dep(p.father);
    add_dep(p.mother);
    if (deps.empty()) return;
    double give_cap = (p.pouch - 10.0) * 0.5;  // keep half the excess
    double per = std::min(5.0, give_cap / deps.size());
    if (per < 0.5) return;
    for (int id : deps) {
        Person& d = w.people[static_cast<size_t>(id)];
        double gift = std::min(per, 10.0 - d.pouch);
        if (gift > 0.1 &&
            transfer(w, person_acc(p.id), person_acc(id), gift,
                     "household pot", false))
            p.honor += 0.01;
    }
}

static void family_support(World& w, Person& p, double need) {
    if (p.pouch >= need) return;
    auto try_give = [&](int id) {
        if (id < 0 || p.pouch >= need) return;
        Person& g = w.people[static_cast<size_t>(id)];
        if (!g.alive) return;
        double spare = g.pouch - 8.0;   // keep a little for themselves
        if (spare <= 0) return;
        double gift = std::min(spare, need - p.pouch);
        if (transfer(w, person_acc(g.id), person_acc(p.id), gift,
                     "family support", false))
            g.honor += 0.02;  // helping kin is honor
    };
    try_give(p.spouse);
    for (int c : p.children) try_give(c);
    try_give(p.father);
    try_give(p.mother);
}

void consumption(World& w) {
    for (auto& p : w.people) {
        if (!p.alive) continue;
        // Decay: the desert is thirsty land.
        p.hunger = clampd(p.hunger - 34, 0, 100);
        p.thirst = clampd(p.thirst - 44, 0, 100);
        p.energy = clampd(p.energy + 22, 0, 100);  // the night's rest

        const bool child = p.age() < 16;
        const double eat = child ? 0.62 : 1.0;

        // Nursing babies (under two) live on their mother's milk: they do
        // not shop, and they do not starve while she is fed. If she goes
        // hungry the milk dries up, and without her the house does what it
        // can - the oldest famine story there is.
        if (p.age_days < 2 * 365) {
            Person* mom =
                (p.mother >= 0 &&
                 w.people[static_cast<size_t>(p.mother)].alive)
                    ? &w.people[static_cast<size_t>(p.mother)]
                    : nullptr;
            if (mom && mom->hunger > 45 && mom->thirst > 45) {
                p.hunger = clampd(p.hunger + 60, 0, 100);
                p.thirst = clampd(p.thirst + 60, 0, 100);
            } else if (mom) {
                p.health = clampd(p.health - 1.5, 0, 100);
                p.hunger = clampd(p.hunger + 20, 0, 100);
                p.thirst = clampd(p.thirst + 20, 0, 100);
                ++w.hungry_today;
            } else {
                p.health = clampd(p.health - 0.8, 0, 100);
                p.hunger = clampd(p.hunger + 15, 0, 100);
                p.thirst = clampd(p.thirst + 15, 0, 100);
            }
            continue;
        }

        // Settle shop credit when the purse allows: debts before savings.
        for (auto& s : w.shops) {
            auto it = s.deyn.find(p.id);
            if (it != s.deyn.end() && it->second > 0 && p.pouch > 40) {
                double pay = std::min(it->second, p.pouch - 20);
                if (pay > 0 &&
                    transfer(w, person_acc(p.id), shop_acc(s.id), pay,
                             "settling deyn", false)) {
                    it->second -= pay;
                    s.deyn_total = fils(s.deyn_total - pay);
                    if (it->second < 0.005) s.deyn.erase(it);
                }
            }
        }

        family_pot(w, p);
        double daily_cost = 7.0;  // rough plan for the day's food
        family_support(w, p, daily_cost);

        // Water first: it matters most. The house drinks what it stored
        // before it sends to the suq for more.
        if (p.thirst < 75) {
            double want = (90 - p.thirst) / 12.0;  // liters
            double got = std::min(
                want, p.stock[static_cast<size_t>(w.ix.water)]);
            if (got > 0) {
                stock_destroy(w, h_person(p.id), w.ix.water, got,
                              "consumption");
                p.thirst = clampd(p.thirst + got * 12.0, 0, 100);
                want -= got;
            }
            Shop* s = pick_shop(w, w.ix.water, p.tribe);
            got = 0;
            if (want > 0.05 && s)
                got = sell_to_person_pub(w, *s, w.ix.water, want, p);
            if (got > 0) {
                stock_destroy(w, h_person(p.id), w.ix.water, got,
                              "consumption");
                p.thirst = clampd(p.thirst + got * 12.0, 0, 100);
            } else {
                ++w.thirsty_today;
                p.health = clampd(p.health - 2.0, 0, 100);
            }
        }
        // Food: bread, then dates, then whatever the suq has.
        if (p.hunger < 70) {
            static const int order_cap = 5;
            int tried = 0;
            for (int item : {w.ix.bread, w.ix.dates, w.ix.meat, w.ix.cheese}) {
                if (p.hunger >= 78 || tried >= order_cap) break;
                ++tried;
                const ItemDef& d = w.data.items[static_cast<size_t>(item)];
                double want = ((80 - p.hunger) / std::max(1.0, d.satisfy)) *
                              (item == w.ix.bread ? 1.0 : eat);
                // The pantry first: a household eats what it stored before
                // it sends to the suq.
                double from_stock = std::min(
                    want, p.stock[static_cast<size_t>(item)]);
                if (from_stock > 0.005) {
                    stock_destroy(w, h_person(p.id), item, from_stock,
                                  "consumption");
                    p.hunger = clampd(p.hunger + from_stock * d.satisfy,
                                      0, 100);
                    want -= from_stock;
                }
                Shop* s = pick_shop(w, item, p.tribe);
                double got = 0;
                if (want > 0.005 && s)
                    got = sell_to_person_pub(w, *s, item, want, p);
                if (got > 0) {
                    stock_destroy(w, h_person(p.id), item, got,
                                  "consumption");
                    p.hunger = clampd(p.hunger + got * d.satisfy, 0, 100);
                }
            }
            if (p.hunger < 30) {
                ++w.hungry_today;
                p.health = clampd(p.health - 1.2, 0, 100);
                p.happiness = clampd(p.happiness - 2, 0, 100);
            }
        }
        // Milk and laban for the children.
        if (child && p.thirst < 80) {
            Shop* s = pick_shop(w, w.ix.milk, p.tribe);
            if (s) {
                double got = sell_to_person_pub(w, *s, w.ix.milk, 0.4, p);
                if (got > 0) {
                    stock_destroy(w, h_person(p.id), w.ix.milk, got,
                                  "consumption");
                    p.thirst = clampd(p.thirst + got * 10, 0, 100);
                }
            }
        }
        // Firewood for the cooking fire.
        double wood_need = 12.0 / 30.0;
        if (p.stock[static_cast<size_t>(w.ix.firewood)] < wood_need * 3) {
            Shop* s = pick_shop(w, w.ix.firewood, p.tribe);
            if (s) sell_to_person_pub(w, *s, w.ix.firewood, wood_need * 6, p);
        }
        stock_destroy(w, h_person(p.id), w.ix.firewood, wood_need,
                      "consumption");

        // Discretionary spending when the purse allows: coffee, sugar,
        // spice, incense - and the services of the valley.
        if (p.pouch > 40 && p.hunger > 50 && p.thirst > 50) {
            if (chance(p.pouch > 200 ? 0.65 : 0.35)) {
                int item = pick(std::vector<int>{w.ix.coffee, w.ix.sugar,
                                                 w.ix.spice});
                Shop* s = pick_shop(w, item, p.tribe);
                if (s) {
                    double got = sell_to_person_pub(w, *s, item, 0.05, p);
                    if (got > 0) {
                        stock_destroy(w, h_person(p.id), item, got,
                                      "consumption");
                        p.happiness = clampd(p.happiness + 3, 0, 100);
                    }
                }
            }
            // An evening with the hakawati or a visit to the muzayyin.
            if (chance(0.06)) {
                static const char* SVC[] = {"Hakawati", "Muzayyin", "Dalil"};
                for (const char* role : SVC) {
                    int found = -1, count = 0;
                    for (auto& q : w.people)
                        if (q.alive && q.role == role && q.tribe == p.tribe) {
                            if (randi(0, count) == 0) found = q.id;
                            ++count;
                        }
                    if (found >= 0 &&
                        transfer(w, person_acc(p.id), person_acc(found), 3.0,
                                 "service", false)) {
                        p.happiness = clampd(p.happiness + 5, 0, 100);
                        w.people[static_cast<size_t>(found)].honor += 0.05;
                        break;
                    }
                }
            }
            // The sick seek the hakim.
            if (p.sick && p.pouch > 15) {
                for (auto& q : w.people)
                    if (q.alive && q.role == "Hakim" && q.tribe == p.tribe) {
                        if (transfer(w, person_acc(p.id), person_acc(q.id),
                                     5.0, "healing", false)) {
                            p.health = clampd(p.health + 18, 0, 100);
                            p.sick_days = std::max(0, p.sick_days - 2);
                        }
                        break;
                    }
            }
        }
        // Monthly household goods: cloth, pottery, oil, tools, sandals.
        if (p.age() >= COMING_OF_AGE && p.pouch > 60) {
            for (int item : {w.ix.cloth, w.ix.pottery, w.ix.oil, w.ix.tools,
                             w.ix.sandals, w.ix.waterskin, w.ix.rug,
                             w.ix.tent}) {
                const ItemDef& d = w.data.items[static_cast<size_t>(item)];
                if (d.use_household_month <= 0) continue;
                int& last = p.last_bought[static_cast<size_t>(item)];
                if (w.tick - last < 30) continue;
                Shop* s = pick_shop(w, item, p.tribe);
                if (!s) continue;
                double got = sell_to_person_pub(w, *s, item,
                                            d.use_household_month, p);
                if (got > 0) {
                    last = w.tick;
                    p.happiness = clampd(p.happiness + 1.5, 0, 100);
                    // Cloth and tools wear out (a named process: wear).
                    stock_destroy(w, h_person(p.id), item, got, "wear");
                }
            }
            // The rich adorn their tents: silk, incense, a blade, a camel.
            if (p.pouch > 500 && chance(0.05)) {
                int item = pick(std::vector<int>{w.ix.silk, w.ix.incense,
                                                 w.ix.blade});
                Shop* s = pick_shop(w, item, p.tribe);
                if (s) {
                    double got = sell_to_person_pub(w, *s, item, 1, p);
                    if (got > 0) p.happiness = clampd(p.happiness + 6, 0, 100);
                }
            }
            // Surplus goes to the Bayt al-Mal for safekeeping (wadiah),
            // but only once the suq's books on him are clean.
            bool owes = false;
            for (auto& s : w.shops)
                if (s.deyn.count(p.id) && s.deyn[p.id] > 0.01) {
                    owes = true;
                    break;
                }
            if (!owes && p.pouch > 250 && p.wadiah < 150 && chance(0.1)) {
                double dep = fils(std::min(150.0 - p.wadiah,
                                           (p.pouch - 250.0) * 0.2));
                if (dep > 1 &&
                    transfer(w, person_acc(p.id), bayt_acc(), dep,
                             "wadiah deposit", false)) {
                    p.wadiah += dep;
                    w.bayt.deposits += dep;
                }
            }
        }
        // Health and happiness drift with how the day went.
        if (p.hunger > 55 && p.thirst > 55) {
            p.health = clampd(p.health + 2.0, 0, 96);
            p.happiness = clampd(p.happiness + 0.4, 0, 100);
        }
        if (p.health < 35 && !p.sick) {
            p.sick = true;
            p.sick_days = 0;
        }
        if (p.sick && p.health > 55) p.sick = false;
        // Household stocks spoil too.
        for (size_t i = 0; i < w.data.items.size(); ++i) {
            const ItemDef& d = w.data.items[i];
            if (d.shelf_life <= 0 || p.stock[i] <= 0) continue;
            double spoil = p.stock[i] / (d.shelf_life * 12.0);
            stock_destroy(w, h_person(p.id), static_cast<int>(i), spoil,
                          "spoilage");
        }
    }
}

void economy_daily(World& w) {
    wells_and_groves(w);
    fields_daily(w);
    production(w);
    consumption(w);
}

// --- the shops close the day -------------------------------------------------------

void shop_close_day(World& w, Shop& s) {
    for (int i = 0; i < w.n_items; ++i) {
        if (!s.sells(i)) continue;
        const ItemDef& d = w.data.items[static_cast<size_t>(i)];
        double& price = s.price[static_cast<size_t>(i)];
        const double t = std::max(1.0, s.target[static_cast<size_t>(i)]);
        const double ratio = s.units(i) / t;
        // Supply and demand: scarce stock pushes the price up, a glut pulls
        // it down. Imports react more strongly than local staples; the move
        // is gradual and always clamped between floor and ceiling.
        double strength = d.local ? 0.10 : 0.18;
        double drift = 1.0 + strength * (1.0 - ratio);
        drift = clampd(drift, 1.0 - strength, 1.0 + strength);
        // The season shows in the price: water is dear in the summer,
        // dates cheap at the harvest.
        const int doy = w.tick % 360;
        if (i == w.ix.water && doy >= 150 && doy < 240) drift += 0.005;
        if (i == w.ix.dates && doy >= 200 && doy < 290) drift -= 0.005;
        price = fils(clampd(price * drift, d.base_price * d.price_floor,
                            d.base_price * d.price_ceil));
        auto& hist = s.price_hist[static_cast<size_t>(i)];
        hist.push_back(price);
        while (hist.size() > 30) hist.pop_front();
        // Spoilage by shelf life.
        if (d.shelf_life > 0 && s.units(i) > 0) {
            double spoil = s.units(i) / (d.shelf_life * 8.0);
            stock_destroy(w, h_shop(s.id), i, std::min(spoil, s.units(i)),
                          "spoilage");
        }
    }
    // The shop settles what it owes the groves once the reserve is safe.
    if (s.deyn_grove > 0.005 && s.till > SHOP_RESERVE) {
        double pay = std::min(s.deyn_grove, fils(s.till - SHOP_RESERVE));
        if (pay > 0.005 &&
            transfer(w, shop_acc(s.id), bayt_acc(), fils(pay * 0.5),
                     "settling the grove account", false)) {
            transfer(w, shop_acc(s.id), majlis_acc(), fils(pay * 0.5),
                     "settling the grove account", false);
            s.deyn_grove = fils(s.deyn_grove - pay);
        }
    }
    // The keeper draws a wage from the till, keeping a restocking reserve.
    if (s.keeper >= 0) {
        Person& k = w.people[static_cast<size_t>(s.keeper)];
        if (k.alive) {
            double profit = std::max(0.0, s.till - SHOP_RESERVE * 3.0);
            double wage = std::min(KEEPER_WAGE + profit * 0.10,
                                   std::max(0.0, s.till - SHOP_RESERVE));
            if (wage > 0.1)
                transfer(w, shop_acc(s.id), person_acc(k.id), wage,
                         "shopkeeper wage", false);
        }
    }
}

}  // namespace tribes
