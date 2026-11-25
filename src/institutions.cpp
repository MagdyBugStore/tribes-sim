// institutions.cpp: the Bayt al-Mal (zakat, wadiah, qard hassan, sadaqah),
// the Majlis (payroll, wells, feasts) and the yearly zakat. No riba
// anywhere: loans are interest-free, deposits are safekeeping only.
#include <cmath>

#include "world.hpp"

namespace tribes {

static constexpr double ZAKAT_RATE = 0.025;
static constexpr double ZAKAT_NISAB = 200.0;
static constexpr double QARD_AMOUNT = 60.0;
static constexpr int QARD_FORGIVE_DAYS = 360;
static constexpr double SADAQAH_STIPEND = 5.0;
static constexpr double ORPHAN_STIPEND = 3.0;

void bayt_daily(World& w) {
    // Day cache for the sheikh's-tent almsgiving: -2 means "not looked up
    // yet", -1 means "this tribe has no one with a full purse".
    std::vector<int> sheikh_of(w.tribe_names.size(), -2);

    // The khazin and the Bayt's own payroll first.
    for (auto& p : w.people) {
        if (!p.alive || !p.is_worker()) continue;
        const RoleDef* r = w.data.role(p.role);
        if (r && r->payer == 'B' && w.bayt.own_funds() >= r->wage)
            transfer(w, bayt_acc(), person_acc(p.id), r->wage,
                     "bayt al-mal wage", false);
    }
    // Wadiah withdrawals: the deposit is a trust, returned on demand.
    for (auto& p : w.people) {
        if (!p.alive || p.wadiah <= 0 || p.pouch >= 25.0) continue;
        double amt = fils(std::min(p.wadiah, 80.0));
        if (amt > 0.01 &&
            transfer(w, bayt_acc(), person_acc(p.id), amt,
                     "wadiah withdrawal", false)) {
            p.wadiah = fils(p.wadiah - amt);
            w.bayt.deposits = fils(w.bayt.deposits - amt);
        }
    }
    // Sadaqah: a food stipend for the destitute, and a little more for the
    // orphans the Bayt looks after. Paid from the Bayt's own funds, never
    // from the wadiah deposits (they are a trust).
    for (auto& p : w.people) {
        if (!p.alive) continue;
        if (p.pouch < 8.0 && w.bayt.own_funds() > 200) {
            double stipend = SADAQAH_STIPEND;
            if (p.orphan && p.age() < 16) stipend += ORPHAN_STIPEND;
            if (p.age() >= RETIREMENT_AGE) stipend += 1.0;
            if (transfer(w, bayt_acc(), person_acc(p.id), stipend, "sadaqah",
                         false))
                w.bayt.sadaqah_out += stipend;
        }
        // The sheikh's tent: when a man of the tribe has nothing and the
        // Bayt is empty, the wealthy of his own tribe feed him. This is
        // the desert's oldest welfare, and it buys honor.
        if (p.pouch < 3.0 && w.bayt.own_funds() <= 200) {
            // The richest man of the tribe, found once and then only
            // re-found when his purse runs below the threshold.
            int& best = sheikh_of[p.tribe];
            while (true) {
                if (best == -2) {
                    best = -1;
                    for (auto& q : w.people)
                        if (q.alive && q.tribe == p.tribe && q.id != p.id &&
                            q.pouch > 60 &&
                            (best < 0 ||
                             q.pouch >
                                 w.people[static_cast<size_t>(best)].pouch))
                            best = q.id;
                }
                if (best < 0) break;
                Person& g = w.people[static_cast<size_t>(best)];
                if (!g.alive || g.pouch <= 60 || g.id == p.id) {
                    best = -2;  // find the next sheikh
                    continue;
                }
                if (transfer(w, person_acc(best), person_acc(p.id), 4.0,
                             "the sheikh's tent", false))
                    g.honor += 0.05;
                break;
            }
        }
        // Qard hassan: an interest-free loan when hunger presses and the
        // family cannot help. Repaid on a schedule when the purse recovers.
        if (p.age() >= COMING_OF_AGE && p.pouch < 3.0 && p.qard <= 0 &&
            w.bayt.own_funds() > 800) {
            if (transfer(w, bayt_acc(), person_acc(p.id), QARD_AMOUNT,
                         "qard hassan", true)) {
                p.qard = QARD_AMOUNT;
                p.qard_day = w.tick;
            }
        }
        // Repayment: a small daily share when there is a surplus.
        if (p.qard > 0 && p.pouch > 80) {
            double pay = std::min(p.qard, 2.0);
            if (transfer(w, person_acc(p.id), bayt_acc(), pay,
                         "qard repayment", false))
                p.qard = fils(p.qard - pay);
        }
        // Zakat forgiveness: a poor debtor's old loan is wiped by the zakat.
        if (p.qard > 0 && w.tick - p.qard_day > QARD_FORGIVE_DAYS &&
            p.wealth() < ZAKAT_NISAB * 0.5) {
            p.qard = 0;  // forgiven: a liability erased, no money moves
            p.remember("Day " + std::to_string(w.tick) +
                       ": the zakat forgave the rest of my loan");
        }
    }
}

void majlis_daily(World& w) {
    // The public payroll at dawn. When the treasury runs short the wages
    // shrink rather than vanish, and the shortfall is recorded.
    double shortfall = 0;
    for (auto& p : w.people) {
        if (!p.alive || !p.is_worker()) continue;
        const RoleDef* r = w.data.role(p.role);
        if (!r || r->payer != 'M') continue;
        double wage = r->wage;
        if (w.majlis.balance >= wage) {
            transfer(w, majlis_acc(), person_acc(p.id), wage,
                     "majlis wage", false);
        } else {
            double part = std::max(0.0, w.majlis.balance);
            if (part > 0.01)
                transfer(w, majlis_acc(), person_acc(p.id), part,
                         "majlis wage (short)", false);
            shortfall += wage - part;
        }
    }
    if (shortfall > 50)
        w.events.add("Day " + std::to_string(w.tick) +
                     ": the Majlis could not pay its people in full");

    // The council does not hoard: a surplus goes back into the valley.
    // First the wells, then the feasts.
    // Dig only when the valley actually needs water: existing wells below
    // 130% of the valley's daily draw.
    double well_yield = 0, water_need = 0;
    for (auto& wl : w.wells) well_yield += wl.yield_day;
    for (auto& p : w.people)
        if (p.alive) water_need += p.age() < 16 ? 2.0 : 3.5;
    const bool need_well = well_yield < water_need * 1.3;
    if (need_well && w.majlis.balance > 4000 &&
        w.majlis.well_progress < w.majlis.well_cost) {
        double spend = std::min(150.0, w.majlis.balance - 3500);
        // The well diggers do the work and take the wage.
        std::vector<int> diggers;
        for (auto& p : w.people)
            if (p.alive && p.role == "WellDigger") diggers.push_back(p.id);
        if (!diggers.empty()) {
            double each = spend / diggers.size();
            double paid = 0;
            for (int d : diggers)
                if (transfer(w, majlis_acc(), person_acc(d), each,
                             "well digging", false))
                    paid += each;
            w.majlis.well_progress += paid;
            if (w.majlis.well_progress >= w.majlis.well_cost) {
                w.majlis.well_progress = 0;
                // A new well: more yield for the whole valley.
                Well wl;
                wl.id = static_cast<int>(w.wells.size());
                wl.tribe = randi(0, static_cast<int>(w.tribe_names.size()) - 1);
                wl.yield_day =
                    w.data.items[static_cast<size_t>(w.ix.well)].aux;
                wl.stock.assign(static_cast<size_t>(w.n_items), 0.0);
                w.wells.push_back(wl);
                w.ground_stock[static_cast<size_t>(w.ix.well)] += 1;
                w.books[static_cast<size_t>(w.ix.well)].initial += 1;
                w.majlis.wells_dug++;
                w.events.add("Day " + std::to_string(w.tick) +
                             ": a new well is dug, praise God");
            }
        }
    }
    // The two Eids: the Majlis feasts the valley.
    const int doy = w.tick % 360;
    if ((doy == 120 || doy == 300) && w.majlis.balance > 3000) {
        double gift = 2.0;
        double cost = gift * w.people.size();
        if (w.majlis.balance > cost + 2500) {
            for (auto& p : w.people) {
                if (!p.alive) continue;
                transfer(w, majlis_acc(), person_acc(p.id), gift, "eid gift",
                         false);
                p.happiness = clampd(p.happiness + 10, 0, 100);
            }
            w.events.add("Day " + std::to_string(w.tick) +
                         ": the Majlis gives the Eid feast");
        }
    }
}

void zakat_annual(World& w) {
    // The yearly zakat: 2.5% on wealth above the nisab, to the Bayt al-Mal,
    // and from there back to the poor.
    double collected = 0;
    for (auto& p : w.people) {
        if (!p.alive) continue;
        double wealth = p.wealth();
        if (wealth < ZAKAT_NISAB) continue;
        double z = fils(wealth * ZAKAT_RATE);
        // Take from the pouch first, then redeem wadiah for the rest.
        double from_pouch = std::min(z, p.pouch);
        if (from_pouch > 0 &&
            transfer(w, person_acc(p.id), bayt_acc(), from_pouch, "zakat",
                     false))
            collected += from_pouch;
    }
    w.bayt.zakat_in += collected;
    w.events.add("Day " + std::to_string(w.tick) + ": the zakat is taken (" +
                 std::to_string(static_cast<long long>(collected)) +
                 " dirhams)");
}

}  // namespace tribes
