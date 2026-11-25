// demographics.cpp: the human fabric - aging and life stages, weddings with
// the mahr, pregnancy and births, deaths and the fara'id inheritance,
// coming of age and retirement. All of it through transfer() and
// move_stock(), so the books always balance.
#include <cmath>
#include <cstdio>

#include "world.hpp"

namespace tribes {

static const std::vector<std::string> MALE_N = {
    "Muhammad", "Ali", "Khalid", "Zayd", "Bilal", "Umar", "Hamza", "Talha",
    "Zubayr", "Sa'd", "Amr", "Hafs", "Jafar", "Suhaib", "Nu'man", "Usama",
    "Qays", "Hassan", "Husayn", "Abbas", "Salman", "Ammar", "Miqdad",
    "Abdullah", "Mus'ab", "Iyas", "Rafi'", "Durayd", "Waraqa", "Mukhallad"};
static const std::vector<std::string> FEMALE_N = {
    "Fatima", "Aisha", "Sumayya", "Hind", "Layla", "Salma", "Zaynab",
    "Ruqayya", "Khadija", "Maymuna", "Rabi'a", "Baraka", "Halima",
    "Safiyya", "Hafsa", "Sawda", "Asma", "Nusayba", "Ghazala", "Rayhana",
    "Lubna", "Sakina", "Durra", "Ward", "Narjis", "Mariam", "Dunya",
    "Amat al-Aziz", "Juwayriyya", "Umm Salama"};

// A wedding: the mahr is the bride's own right, paid through the gateway,
// and she joins her husband's tribe. Cross-tribe matches can seal an
// alliance.
static void marry(World& w, Person& groom, Person& bride) {
    groom.spouse = bride.id;
    bride.spouse = groom.id;
    bride.tribe = groom.tribe;
    bride.widowed_since = -1;
    groom.widowed_since = -1;
    double mahr = fils(std::min(groom.pouch * 0.25, randd(40, 160)));
    if (mahr > 0)
        transfer(w, person_acc(groom.id), person_acc(bride.id), mahr, "mahr",
                 true);
    groom.happiness = clampd(groom.happiness + 12, 0, 100);
    bride.happiness = clampd(bride.happiness + 12, 0, 100);
    groom.honor += 1;
    bride.honor += 1;
    w.weddings_total++;
    w.events.add("Day " + std::to_string(w.tick) + ": " +
                 w.full_name(groom) + " marries " + w.full_name(bride) +
                 " (mahr " + std::to_string((long long)mahr) + ")");
}

// The estate divided by the fara'id: the spouse's share, then the children
// (a son two shares, a daughter one), then the parents, else the Bayt
// al-Mal. Every fils moves through transfer(); the pouch ends at zero.
static bool spouse_alive(World& w, const Person& p);

static void inherit(World& w, Person& dead) {
    // The wadiah returns to the estate first.
    if (dead.wadiah > 0.005) {
        if (transfer(w, bayt_acc(), person_acc(dead.id), dead.wadiah,
                     "wadiah returned to the estate", false)) {
            w.bayt.deposits = fils(w.bayt.deposits - dead.wadiah);
            dead.wadiah = 0;
        }
    }
    double estate = dead.pouch;
    if (estate > 0.005) {
        std::vector<std::pair<int, double>> shares;  // person id, share
        Person* spouse = dead.spouse >= 0 ? w.find_person(dead.spouse) : nullptr;
        bool has_children = false;
        for (int c : dead.children)
            if (w.people[static_cast<size_t>(c)].alive) has_children = true;
        double spouse_share = 0;
        if (spouse && spouse->alive) {
            spouse_share = has_children
                               ? (dead.gender == Gender::Male ? 0.125 : 0.25)
                               : (dead.gender == Gender::Male ? 0.25 : 0.5);
            shares.push_back({spouse->id, spouse_share});
        }
        if (has_children) {
            double sons = 0, daughters = 0;
            for (int c : dead.children) {
                const Person& ch = w.people[static_cast<size_t>(c)];
                if (!ch.alive) continue;
                if (ch.gender == Gender::Male) sons += 1;
                else daughters += 1;
            }
            double rest = 1.0 - spouse_share;
            double units = sons * 2 + daughters;
            for (int c : dead.children) {
                const Person& ch = w.people[static_cast<size_t>(c)];
                if (!ch.alive) continue;
                double u = ch.gender == Gender::Male ? 2.0 : 1.0;
                shares.push_back({c, rest * u / units});
            }
        } else {
            double rest = 1.0 - spouse_share;
            std::vector<int> parents;
            if (dead.father >= 0 && w.people[static_cast<size_t>(dead.father)].alive)
                parents.push_back(dead.father);
            if (dead.mother >= 0 && w.people[static_cast<size_t>(dead.mother)].alive)
                parents.push_back(dead.mother);
            if (!parents.empty()) {
                double each = rest / parents.size();
                for (int par : parents) shares.push_back({par, each});
            }
        }
        for (auto& sh : shares) {
            double amt = fils(estate * sh.second);
            transfer(w, person_acc(dead.id), person_acc(sh.first), amt,
                     "inheritance (fara'id)", true);
        }
        // Whatever is left (rounding, or no kin at all) goes to the Bayt.
        transfer(w, person_acc(dead.id), bayt_acc(), dead.pouch,
                 "estate remainder", true);
        dead.pouch = 0;
    } else {
        dead.pouch = 0;
    }
    // The household stock passes to the spouse, else the eldest child,
    // else the Bayt's sadaqah stores.
    int heir = (spouse_alive(w, dead)) ? dead.spouse : -1;
    if (heir < 0)
        for (int c : dead.children)
            if (w.people[static_cast<size_t>(c)].alive) {
                heir = c;
                break;
            }
    for (int i = 0; i < w.n_items; ++i) {
        double qty = dead.stock[static_cast<size_t>(i)];
        if (qty <= 0) continue;
        if (heir >= 0)
            move_stock(w, i, qty, h_person(dead.id), h_person(heir),
                       "inheritance", true);
        else
            move_stock(w, i, qty, h_person(dead.id), h_bayt(),
                       "estate to the Bayt al-Mal", true);
    }
    // Livestock herds pass to the heir too.
    for (auto& h : w.herds)
        if (h.owner == dead.id && heir >= 0) h.owner = heir;
    // A shopkeeper's shop passes to the heir's keeping.
    for (auto& s : w.shops)
        if (s.keeper == dead.id) s.keeper = heir;
    // Debts die with the debtor: the Bayt writes off the qard.
    dead.qard = 0;
}

static bool spouse_alive(World& w, const Person& p) {
    return p.spouse >= 0 && w.people[static_cast<size_t>(p.spouse)].alive;
}

static void die(World& w, Person& p, const char* cause) {
    p.alive = false;
    p.death_day = w.tick;
    p.death_cause = cause;
    w.deaths_total++;
    w.deaths_today++;
    if (p.spouse >= 0) {
        Person& sp = w.people[static_cast<size_t>(p.spouse)];
        if (sp.alive) {
            sp.widowed_since = w.tick;
            sp.happiness = clampd(sp.happiness - 25, 0, 100);
        }
        p.spouse = -1;
    }
    for (int c : p.children) {
        Person& ch = w.people[static_cast<size_t>(c)];
        ch.happiness = clampd(ch.happiness - 12, 0, 100);
        if (ch.age() < 16) {
            bool parent_alive = false;
            if (ch.father >= 0 && w.people[static_cast<size_t>(ch.father)].alive)
                parent_alive = true;
            if (ch.mother >= 0 && w.people[static_cast<size_t>(ch.mother)].alive)
                parent_alive = true;
            if (!parent_alive) ch.orphan = true;
        }
    }
    inherit(w, p);
    w.events.add("Day " + std::to_string(w.tick) + ": " + w.full_name(p) +
                 " dies of " + cause + " at " + std::to_string(p.age()));
}

// A birth: named, of the father's tribe, pouch empty, counted. The mother
// then nurses (she cannot conceive again until the baby is weaned), and
// childbirth itself carried a real risk to her life in this world.
// NOTE: takes the mother by id and re-fetches her after the push_back,
// because growing the people vector can reallocate it.
static void give_birth(World& w, int mother_id) {
    Person baby;
    baby.id = static_cast<int>(w.people.size());
    baby.gender = chance(0.5) ? Gender::Male : Gender::Female;
    baby.name = baby.gender == Gender::Male ? pick(MALE_N) : pick(FEMALE_N);
    baby.age_days = 0;
    int father = w.people[static_cast<size_t>(mother_id)].spouse;
    baby.tribe = w.people[static_cast<size_t>(mother_id)].tribe;
    baby.role = "Child";
    baby.mother = mother_id;
    baby.father = father;
    baby.stock.assign(static_cast<size_t>(w.n_items), 0.0);
    baby.last_bought.assign(static_cast<size_t>(w.n_items), -10000);
    baby.hunger = baby.thirst = baby.energy = 90;
    baby.honor = 5;
    w.people.push_back(baby);
    Person& mother = w.people[static_cast<size_t>(mother_id)];
    mother.children.push_back(baby.id);
    if (father >= 0)
        w.people[static_cast<size_t>(father)].children.push_back(baby.id);
    mother.pregnant_since = -1;
    mother.nursing_until = w.tick + NURSING_DAYS;
    mother.happiness = clampd(mother.happiness + 15, 0, 100);
    w.births_total++;
    w.births_today++;
    mother.remember("Day " + std::to_string(w.tick) + ": gave birth to " +
                    baby.name);
    // Maternal mortality (MATERNAL_MORTALITY, see person.hpp): the child
    // lives, the mother does not.
    if (chance(MATERNAL_MORTALITY)) die(w, mother, "childbirth");
}

// Coming of age: a youth takes a trade. The choice follows the valley's
// needs: the trades whose goods are dearest (against their base price) are
// the ones the young hear calling.
void World::assign_trade(Person& p, bool from_labor) {
    struct Cand {
        std::string role;
        double score;
    };
    std::vector<Cand> cands;
    int n_workers = 0;
    for (auto& q : people)
        if (q.alive && q.is_worker()) ++n_workers;
    for (const auto& r : data.roles) {
        if (r.frac <= 0) continue;
        // A laborer who apprentices leaves the labor pool: the residual
        // role itself is not a calling he can be called to.
        if (from_labor && r.name == "Ajer") continue;
        if (r.male_only && p.gender != Gender::Male) continue;
        if (r.female_only && p.gender != Gender::Female) continue;
        // Salaried roles fill to their fraction and no more.
        int count = 0;
        for (auto& q : people)
            if (q.alive && q.role == r.name) ++count;
        double vacancy = r.frac * std::max(1, n_workers) - count;
        double score = 0.2 + std::max(0.0, vacancy) * 0.2;
        // Producer roles bid higher when their good is scarce and dear.
        auto recipes = data.recipes_of(r.name);
        for (const RecipeDef* rec : recipes)
            for (const auto& out : rec->out_name) {
                int item = data.item_index(out);
                if (item < 0) continue;
                double best = 0;
                const ItemDef& d = data.items[static_cast<size_t>(item)];
                for (auto& s : shops)
                    if (s.sells(item))
                        best = std::max(best, s.ask(item) / d.base_price);
                score += best;
            }
        cands.push_back({r.name, score * randd(0.7, 1.3)});
    }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.score > b.score; });
    if (cands.empty()) {
        p.role = "Ajer";
        return;
    }
    p.role = cands[randi(0, static_cast<int>(std::min<size_t>(2, cands.size() - 1)))]
                 .role;
    p.skill = 0.7;
    // A new Tajir may take over a keeperless shop.
    if (p.role == "Tajir")
        for (auto& s : shops)
            if (s.keeper < 0 ||
                !people[static_cast<size_t>(s.keeper)].alive) {
                s.keeper = p.id;
                p.employer_shop = s.id;
                break;
            }
    if (p.role == "Herder" || p.role == "CamelHerder") {
        for (int item : (p.role == "Herder"
                             ? std::vector<int>{ix.goat, ix.sheep}
                             : std::vector<int>{ix.camel})) {
            Herd h;
            h.owner = p.id;
            h.item = item;
            herds.push_back(h);
        }
    }
}

void demographics_daily(World& w) {
    // Aging, sickness and death.
    for (auto& p : w.people) {
        if (!p.alive) continue;
        p.age_days++;
        // Infant and child mortality, before any midwife or clinic: in the
        // pre-industrial record more than 200 of every 1000 newborns died
        // within their first year, and about half of all children before
        // fifteen (see README, demographic sources). Hunger waves take
        // still more on top of this baseline.
        {
            int ca = p.age();
            double crisk = ca < 1    ? 0.00055   // ~18% in the first year
                           : ca < 5  ? 0.00016   // ~6%/year, ages 1-4
                           : ca < 15 ? 0.000045  // ~1.6%/year, ages 5-14
                                     : 0.0;
            if (crisk > 0 && chance(crisk)) {
                die(w, p, "childhood illness");
                continue;
            }
        }
        if (p.sick) {
            p.sick_days++;
            p.health = clampd(p.health - 0.6, 0, 100);
            if (p.sick_days > 10 && chance(0.03)) {
                die(w, p, "illness");
                continue;
            }
        }
        int a = p.age();
        if (a >= 60) {
            double risk = 0.0002 * std::pow(1.09, a - 60);
            if (chance(risk)) {
                die(w, p, "old age");
                continue;
            }
        }
        if (!p.sick && chance(0.001)) {
            p.sick = true;
            p.sick_days = 0;
        }
        // Sixty: a salaried man steps down from the payroll. A craftsman
        // does not stop one morning - his hands simply do less each year.
        if (a == RETIREMENT_AGE && !p.retired && p.age_days % 365 == 0) {
            p.retired = true;
            const RoleDef* rr = w.data.role(p.role);
            if (rr && rr->payer != 'N') p.role = "Elder";
        }
        // Coming of age at sixteen.
        if (a == COMING_OF_AGE && p.role == "Tilmidh" &&
            p.age_days % 365 == 0)
            w.assign_trade(p);
    }
    // Elder care: the old are carried by their children, or by their
    // neighbors - or they face the years alone and fade the faster for it.
    // Care costs the caregiver time and money, and buys honor.
    for (auto& p : w.people) {
        if (!p.alive || p.age() < RETIREMENT_AGE) continue;
        bool supported = false;
        bool has_children = false;
        for (int c : p.children) {
            Person& ch = w.people[static_cast<size_t>(c)];
            if (!ch.alive || ch.age() < COMING_OF_AGE) continue;
            has_children = true;
            // Time with the old father costs the son a piece of his day.
            ch.energy = clampd(ch.energy - 8, 0, 100);
            if (p.pouch < 12 && ch.pouch > 10) {
                double gift = std::min(5.0, fils(ch.pouch - 8));
                if (transfer(w, person_acc(ch.id), person_acc(p.id), gift,
                             "caring for a parent", false)) {
                    ch.honor += 0.05;
                    supported = true;
                    if (w.tick % 30 == 0)
                        ch.remember("Day " + std::to_string(w.tick) +
                                    ": cares for his old parent");
                }
            }
            double pfood = p.stock[static_cast<size_t>(w.ix.bread)] +
                           p.stock[static_cast<size_t>(w.ix.dates)];
            if (pfood < 2.0) {
                for (int item : {w.ix.bread, w.ix.dates}) {
                    if (ch.stock[static_cast<size_t>(item)] > 4.0 &&
                        move_stock(w, item, 2.0, h_person(ch.id),
                                   h_person(p.id), "caring for a parent",
                                   false)) {
                        ch.honor += 0.02;
                        supported = true;
                        break;
                    }
                }
            }
            break;  // the children take turns; one sits with him a day
        }
        if (!has_children) {
            // The childless elder: a neighbor's hand, or loneliness.
            if (p.pouch < 6.0 && chance(0.5)) {
                int n_p = static_cast<int>(w.people.size());
                int start = randi(0, n_p - 1);
                for (int k = 0; k < n_p; ++k) {
                    Person& n = w.people[static_cast<size_t>(
                        (start + k) % n_p)];
                    if (!n.alive || n.id == p.id || n.tribe != p.tribe ||
                        n.age() < 18 || n.age() >= RETIREMENT_AGE ||
                        n.pouch < 20)
                        continue;
                    if (transfer(w, person_acc(n.id), person_acc(p.id), 3.0,
                                 "a neighbor's hand", false)) {
                        n.honor += 0.03;
                        supported = true;
                        if (w.tick % 30 == 0)
                            n.remember("Day " + std::to_string(w.tick) +
                                       ": looks in on an old neighbor");
                    }
                    break;
                }
            }
        }
        if (!supported) {
            // Alone with his fate: the years press harder.
            p.health = clampd(p.health - 0.8, 0, 100);
            p.happiness = clampd(p.happiness - 0.6, 0, 100);
            if (!p.sick && chance(0.002)) {
                p.sick = true;
                p.sick_days = 0;
            }
        }
    }
    // Pregnancies progress; babies are born. Collect first: giving birth
    // grows the people vector, so no reference may be held across it.
    std::vector<int> due;
    for (const auto& p : w.people) {
        if (!p.alive || p.pregnant_since == -1) continue;
        if (w.tick - p.pregnant_since >= PREGNANCY_DAYS) due.push_back(p.id);
    }
    for (int id : due) {
        Person& m = w.people[static_cast<size_t>(id)];
        if (m.alive && m.pregnant_since != -1) give_birth(w, id);
    }
    // A dead craftsman's trade does not die with him: laborers apprentice
    // into the roles the valley lacks (the scarcity price calls them).
    if (w.tick % 7 == 3) {
        for (auto& p : w.people) {
            if (!p.alive || p.role != "Ajer" || p.age() > 50) continue;
            if (!chance(0.30)) continue;
            p.employer_shop = -1;
            w.assign_trade(p, true);
        }
    }
    // Courtship and weddings, once a week.
    if (w.tick % 7 == 0) {
        std::vector<int> singles;
        for (auto& p : w.people) {
            if (!p.alive || p.spouse != -1) continue;
            if (p.gender == Gender::Male && p.age() >= 18 && p.age() <= 60 &&
                (p.widowed_since < 0 || w.tick - p.widowed_since > 30))
                singles.push_back(p.id);
        }
        std::shuffle(singles.begin(), singles.end(), RNG);
        for (int m : singles) {
            if (!chance(0.10)) continue;
            Person& g = w.people[static_cast<size_t>(m)];
            if (g.spouse != -1) continue;
            // Find a bride: single or a widow past her waiting days.
            int best = -1;
            for (int tries = 0; tries < 25; ++tries) {
                int id = randi(0, static_cast<int>(w.people.size()) - 1);
                Person& b = w.people[static_cast<size_t>(id)];
                if (!b.alive || b.gender != Gender::Female || b.spouse != -1)
                    continue;
                if (b.age() < 16 || b.age() > 45) continue;
                if (b.widowed_since >= 0 && w.tick - b.widowed_since < 130)
                    continue;
                int gap = g.age() - b.age();
                if (gap < 0 || gap > 20) continue;
                best = id;
                break;
            }
            if (best >= 0) {
                Person& b = w.people[static_cast<size_t>(best)];
                int gt = g.tribe, bt = b.tribe;
                marry(w, g, b);
                if (gt != bt && !w.allied(gt, bt) && chance(0.05)) {
                    w.set_allied(gt, bt);
                    w.events.add("Day " + std::to_string(w.tick) + ": " +
                                 w.tribe_names[static_cast<size_t>(gt)] +
                                 " and " +
                                 w.tribe_names[static_cast<size_t>(bt)] +
                                 " are allied by marriage");
                }
            }
        }
        // New conceptions among the married. The historical rhythm: a wife
        // bears a child, nurses him (~16 months, no conception), then
        // conceives again - roughly one child every 2-3 years through her
        // fertile years, 5-8 over a lifetime (see README, demographic
        // sources). -1 alone means "not expecting".
        for (auto& p : w.people) {
            if (!p.alive || p.gender != Gender::Female || p.spouse < 0 ||
                p.pregnant_since != -1)
                continue;
            if (p.age() < 16 || p.age() > 45) continue;
            if (p.children.size() >= 10) continue;
            if (w.tick < p.nursing_until) continue;
            double rate = p.age() < 30 ? 0.09 : p.age() < 40 ? 0.05 : 0.02;
            if (chance(rate)) p.pregnant_since = w.tick;
        }
    }
    // Hospitality (dyafa): a tent opens to guests, honor follows.
    if (chance(0.3)) {
        int id = randi(0, static_cast<int>(w.people.size()) - 1);
        Person& host = w.people[static_cast<size_t>(id)];
        if (host.alive && host.pouch > 30) {
            Shop* s = nullptr;
            for (auto& x : w.shops)
                if (x.kind == ShopKind::FoodStall && x.tribe == host.tribe &&
                    x.units(w.ix.dates) > 1)
                    s = &x;
            if (s) {
                extern double sell_to_person_pub(World&, Shop&, int, double,
                                                 Person&, double);
                if (sell_to_person_pub(w, *s, w.ix.dates, 2.0, host,
                                       40.0) > 0) {  // household deyn limit
                    host.honor += 0.6;
                    host.happiness = clampd(host.happiness + 3, 0, 100);
                }
            }
        }
    }
}

}  // namespace tribes
