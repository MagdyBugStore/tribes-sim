// herds.cpp: the flocks. Grazing and bran, breeding (a female carries for
// the real gestation period and the herd grows slowly), milking, natural
// deaths. Slaughter itself is a chain stage in economy.cpp (the butcher's
// recipe), so herds shrink ONLY by slaughter, sale or death - never by
// magic.
#include <cmath>

#include "world.hpp"

namespace tribes {

static double litter_of(int item, const World& w) {
    if (item == w.ix.goat || item == w.ix.sheep) return 1.2;
    if (item == w.ix.chicken) return 6.0;
    return 1.0;
}

void herds_daily(World& w) {
    for (auto& h : w.herds) {
        Person& owner = w.people[static_cast<size_t>(h.owner)];
        if (!owner.alive) continue;
        const ItemDef& d = w.data.items[static_cast<size_t>(h.item)];
        double& heads = owner.stock[static_cast<size_t>(h.item)];
        if (heads < 1) continue;

        const double females = heads * 0.6;
        const int gestation = static_cast<int>(d.aux > 0 ? d.aux : 150);

        // Feeding: pasture carries most of it, bran tops it up. A hungry
        // herd sickens; a fed herd thrives.
        double bran_want = heads * 0.15;  // kg per day
        double bran_have = owner.stock[static_cast<size_t>(w.ix.bran)];
        double bran_used = std::min(bran_want, bran_have);
        if (bran_used > 0)
            stock_destroy(w, h_person(h.owner), w.ix.bran, bran_used,
                          "consumption");
        double fed = 0.7 + 0.3 * (bran_want > 0 ? bran_used / bran_want : 0.5);
        // The herd drinks from the nearest well (the desert's right).
        Well* wl = nullptr;
        for (auto& x : w.wells)
            if (x.tribe == owner.tribe) wl = &x;
        if (!wl && !w.wells.empty()) wl = &w.wells.front();
        double drink = heads * (h.item == w.ix.camel ? 9.0 : 4.0);
        if (wl) {
            double got =
                std::min(drink, wl->stock[static_cast<size_t>(w.ix.water)]);
            stock_destroy(w, h_well(wl->id), w.ix.water, got, "consumption");
            fed *= drink > 0 ? (0.6 + 0.4 * got / drink) : 0.6;
        }
        h.health = clampd(h.health + (fed - 0.85) * 0.05, 0.5, 1.2);

        // Natural deaths: old age and hardship, more when the herd starves.
        double mortality = heads * 0.10 / 365.0 * (2.0 - h.health);
        mortality = std::min(mortality, heads);
        if (mortality > 0.001)
            stock_destroy(w, h_person(h.owner), h.item, mortality,
                          "spoilage");  // carrion, a named loss

        // Breeding: a share of the open females conceive; the carrying batch
        // drops after the real gestation. The herd grows slowly.
        if (h.pregnant <= 0 && females >= 1) {
            double cycle = gestation + 90.0;
            double conceive = females * (0.85 / cycle) *
                              (h.item == w.ix.chicken ? 40.0 : 1.0);
            conceive = std::min(conceive * h.health, females * 0.5);
            if (conceive >= 0.01) {
                h.pregnant = static_cast<int>(std::round(conceive));
                h.due_day = w.tick + gestation;
            }
        } else if (h.pregnant > 0 && w.tick >= h.due_day) {
            double born = h.pregnant * litter_of(h.item, w);
            stock_create(w, h_person(h.owner), h.item, born, "birth");
            h.pregnant = 0;
            h.due_day = -1;
        }

        // Milking: lactating females give milk the herder sells.
        if (h.item == w.ix.goat || h.item == w.ix.sheep ||
            h.item == w.ix.camel) {
            double per_head = h.item == w.ix.camel ? 2.0 : 0.4;
            double liters = females * 0.45 * per_head * h.health;
            if (liters > 0.01) {
                stock_create(w, h_person(h.owner), w.ix.milk, liters,
                             "milking");
            }
        }
        // Shearing: twice a year the sheep give their wool.
        if (h.item == w.ix.sheep && (w.tick % 180) == 30) {
            double wool = heads * 0.6;  // kg per head per shearing
            stock_create(w, h_person(h.owner), w.ix.wool, wool, "shearing");
        }
    }

    // Herders sell their milk and wool into the suq, and surplus stock to
    // the butchers is handled by the butcher's own buying.
    // The day's dairymen by tribe, listed once instead of scanning the
    // whole valley for every herder with milk to sell.
    std::map<int, std::vector<int>> dairymen;
    for (auto& q : w.people)
        if (q.alive && q.role == "Dairyman") dairymen[q.tribe].push_back(q.id);
    for (auto& p : w.people) {
        if (!p.alive) continue;
        if (p.role != "Herder" && p.role != "CamelHerder") continue;
        double milk = p.stock[static_cast<size_t>(w.ix.milk)];
        double keep = p.role == "Dairyman" ? milk : 2.0;
        if (milk > keep) {
            // The dairymen buy first for their laban and cheese.
            double sell = milk - keep;
            for (int qid : dairymen[p.tribe]) {
                if (sell <= 0.005) break;
                Person& q = w.people[static_cast<size_t>(qid)];
                if (q.id == p.id) continue;
                double want = 40.0 - q.stock[static_cast<size_t>(w.ix.milk)];
                if (want <= 0) continue;
                double units = std::min(sell, want);
                double price = fils(w.data.items[static_cast<size_t>(w.ix.milk)]
                                        .base_price *
                                    0.9);
                double cost = fils(units * price);
                if (q.pouch < cost) {
                    units = std::floor(q.pouch / price * 100.0) / 100.0;
                    cost = fils(units * price);
                }
                if (units <= 0.005) continue;
                if (transfer(w, person_acc(q.id), person_acc(p.id), cost,
                             "milk for the dairy", false)) {
                    move_stock(w, w.ix.milk, units, h_person(p.id),
                               h_person(q.id), "milk for the dairy", false);
                    sell -= units;
                }
            }
            if (sell > 0.005) {
                extern double sell_to_market_pub(World&, Person&, int, double,
                                                 const char*);
                sell_to_market_pub(w, p, w.ix.milk, sell, "milking");
            }
        }
        double wool = p.stock[static_cast<size_t>(w.ix.wool)];
        if (wool > 1) {
            extern double sell_to_market_pub(World&, Person&, int, double,
                                             const char*);
            sell_to_market_pub(w, p, w.ix.wool, wool, "shearing");
        }
    }
}

}  // namespace tribes
