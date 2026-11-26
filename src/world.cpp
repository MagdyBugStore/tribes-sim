#include "world.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>

namespace tribes {

// --- static names -------------------------------------------------------------

static const std::vector<std::string> MALE_NAMES = {
    "Muhammad", "Ali", "Khalid", "Zayd", "Bilal", "Umar", "Hamza",
    "Talha", "Zubayr", "Sa'd", "Amr", "Hafs", "Jafar", "Suhaib",
    "Nu'man", "Waraqa", "Usama", "Mukhallad", "Qays", "Durayd",
    "Hassan", "Husayn", "Abbas", "Mu'awiya", "Salman", "Ammar",
    "Miqdad", "Abdullah", "Ubayda", "Mus'ab", "Iyas", "Rafi'"};
static const std::vector<std::string> FEMALE_NAMES = {
    "Fatima", "Aisha", "Sumayya", "Hind", "Layla", "Salma", "Zaynab",
    "Ruqayya", "Khadija", "Maymuna", "Rabi'a", "Baraka", "Halima",
    "Safiyya", "Hafsa", "Umm Kulthum", "Juwayriyya", "Sawda", "Asma",
    "Nusayba", "Umm Salama", "Ghazala", "Rayhana", "Lubna", "Sakina",
    "Durra", "Amat al-Aziz", "Ward", "Narjis", "Mariam", "Dunya"};

static const std::vector<std::string> ALL_TRIBES = {
    "Banu Taym", "Banu Asad", "Banu Kalb", "Banu Amir",
    "Banu Sulaym", "Banu Ghatafan"};
static const std::vector<std::string> ALL_NISBA_M = {
    "al-Taymi", "al-Asadi", "al-Kalbi", "al-Amiri", "al-Sulami",
    "al-Ghatafani"};
static const std::vector<std::string> ALL_NISBA_F = {
    "al-Taymiyya", "al-Asadiyya", "al-Kalbiyya", "al-Amiriyya",
    "al-Sulamiyya", "al-Ghatafaniyya"};

// --- construction ------------------------------------------------------------

World::World(int num_people, uint64_t seed_, double tick_interval_s,
             const Data& d)
    : initial_people(num_people),
      seed(seed_),
      tick_interval(tick_interval_s),
      data(d) {
    RNG.seed(seed);
    scale = num_people / 1000.0;
    n_items = static_cast<int>(data.items.size());
    reason_purchase.resize(static_cast<size_t>(n_items));
    reason_sale.resize(static_cast<size_t>(n_items));
    for (int i = 0; i < n_items; ++i) {
        reason_purchase[static_cast<size_t>(i)] =
            "purchase: " + data.items[static_cast<size_t>(i)].name;
        reason_sale[static_cast<size_t>(i)] =
            "sale: " + data.items[static_cast<size_t>(i)].name;
    }
    resolve_indices();

    const int n_tribes = static_cast<int>(
        clampd(std::round(num_people / 170.0), 2, 6));
    for (int i = 0; i < n_tribes; ++i) {
        tribe_names.push_back(ALL_TRIBES[static_cast<size_t>(i)]);
        nisba_m.push_back(ALL_NISBA_M[static_cast<size_t>(i)]);
        nisba_f.push_back(ALL_NISBA_F[static_cast<size_t>(i)]);
    }
    // A couple of standing alliances between neighbors.
    if (n_tribes >= 2) set_allied(0, 1);
    if (n_tribes >= 4) set_allied(2, 3);

    found_land();
    found_people(num_people);
    found_families();
    found_economy();
    distribute_starting_stock();

    books.assign(static_cast<size_t>(n_items), StockBook{});
    for (int i = 0; i < n_items; ++i) books[static_cast<size_t>(i)].initial = total_holdings(*this, i);
    sales_units.assign(static_cast<size_t>(n_items), 0.0);
    sales_value.assign(static_cast<size_t>(n_items), 0.0);
    initial_supply = total_money(*this);
}

void World::resolve_indices() {
    auto get = [&](const char* n) {
        int i = data.item_index(n);
        if (i < 0) {
            std::cerr << "resources.csv is missing item: " << n << "\n";
            std::abort();
        }
        return i;
    };
    ix.water = get("water");   ix.bread = get("bread");
    ix.wheat = get("wheat");   ix.flour = get("flour");
    ix.bran = get("bran");     ix.dates = get("dates");
    ix.milk = get("milk");     ix.laban = get("laban");
    ix.cheese = get("cheese"); ix.samn = get("samn"); ix.meat = get("meat");
    ix.salt = get("salt");     ix.hide = get("hide");
    ix.leather = get("leather"); ix.sandals = get("sandals");
    ix.waterskin = get("waterskin"); ix.wool = get("wool");
    ix.yarn = get("yarn");     ix.rug = get("rug");
    ix.tent = get("tent");     ix.iron = get("iron");
    ix.tools = get("tools");   ix.blade = get("blade");
    ix.pottery = get("pottery"); ix.cloth = get("cloth");
    ix.silk = get("silk");     ix.incense = get("incense");
    ix.spice = get("spice");   ix.sugar = get("sugar");
    ix.coffee = get("coffee"); ix.oil = get("oil");
    ix.firewood = get("firewood");
    ix.camel = get("camel");   ix.horse = get("horse");
    ix.cattle = get("cattle"); ix.goat = get("goat");
    ix.sheep = get("sheep");   ix.donkey = get("donkey");
    ix.chicken = get("chicken");
    ix.well = get("well");     ix.palm = get("palm");
    ix.field = get("field");   ix.saltpan = get("saltpan");
}

int World::tribe_size(int t) const {
    int n = 0;
    for (const auto& p : people)
        if (p.alive && p.tribe == t) ++n;
    return n;
}

std::string World::full_name(const Person& p) const {
    const auto& nisbas = (p.gender == Gender::Male) ? nisba_m : nisba_f;
    return p.name + " " + nisbas[static_cast<size_t>(p.tribe)];
}

Person* World::find_person(int pid) {
    if (pid < 0 || pid >= static_cast<int>(people.size())) return nullptr;
    return &people[static_cast<size_t>(pid)];
}

// --- founding: the land ---------------------------------------------------------

void World::found_land() {
    const int n_tribes = static_cast<int>(tribe_names.size());
    ground_stock.assign(static_cast<size_t>(n_items), 0.0);

    const ItemDef& wdef = data.items[static_cast<size_t>(ix.well)];
    int n_wells = std::max(2, static_cast<int>(std::round(wdef.start_total * scale)));
    for (int i = 0; i < n_wells; ++i) {
        Well wl;
        wl.id = i;
        wl.tribe = i % n_tribes;
        wl.yield_day = wdef.aux;
        wl.stock.assign(static_cast<size_t>(n_items), 0.0);
        wells.push_back(wl);
    }
    ground_stock[static_cast<size_t>(ix.well)] = n_wells;
    majlis.wells_dug = n_wells;

    const ItemDef& pdef = data.items[static_cast<size_t>(ix.palm)];
    double palms = pdef.start_total * scale;
    for (int t = 0; t < n_tribes; ++t) {
        Grove g;
        g.id = t;
        g.tribe = t;
        g.palms = palms / n_tribes;
        g.kg_per_palm_year = pdef.aux;
        g.stock.assign(static_cast<size_t>(n_items), 0.0);
        groves.push_back(g);
    }
    ground_stock[static_cast<size_t>(ix.palm)] = palms;
    ground_stock[static_cast<size_t>(ix.saltpan)] =
        data.items[static_cast<size_t>(ix.saltpan)].start_total * scale;
}

// --- founding: the people ------------------------------------------------------

void World::found_people(int num_people) {
    const int n_tribes = static_cast<int>(tribe_names.size());
    // Random tribe sizes: a weighted draw per tribe, so the settlements are
    // organically unequal (some large, some small).
    std::vector<double> w(static_cast<size_t>(n_tribes));
    double wsum = 0;
    for (auto& x : w) {
        x = randd(0.3, 1.0);
        wsum += x;
    }
    std::vector<double> cum(static_cast<size_t>(n_tribes));
    double acc = 0;
    for (int i = 0; i < n_tribes; ++i) {
        acc += w[static_cast<size_t>(i)] / wsum;
        cum[static_cast<size_t>(i)] = acc;
    }
    auto draw_tribe = [&]() {
        double r = randd(0, 1);
        for (int i = 0; i < n_tribes; ++i)
            if (r <= cum[static_cast<size_t>(i)]) return i;
        return n_tribes - 1;
    };

    people.reserve(static_cast<size_t>(num_people * 2));
    for (int i = 0; i < num_people; ++i) {
        Person p;
        p.id = i;
        p.gender = chance(0.5) ? Gender::Male : Gender::Female;
        p.name = p.gender == Gender::Male ? pick(MALE_NAMES) : pick(FEMALE_NAMES);
        p.tribe = draw_tribe();
        // A realistic age pyramid: children, youths, adults, elders.
        double r = randd(0, 1);
        int years;
        if (r < 0.24) years = randi(0, 12);
        else if (r < 0.32) years = randi(13, 17);
        else if (r < 0.90) years = randi(18, 55);
        else years = randi(56, 74);
        p.age_days = years * 365 + randi(0, 364);
        p.stock.assign(static_cast<size_t>(n_items), 0.0);
        p.last_bought.assign(static_cast<size_t>(n_items), -10000);
        p.skill = randd(0.7, 1.2);
        p.honor = randd(5, 15);
        if (p.age() < 7) p.role = "Child";
        else if (p.age() < COMING_OF_AGE) p.role = "Tilmidh";
        else if (p.age() >= RETIREMENT_AGE) { p.role = "Elder"; p.retired = true; }
        people.push_back(p);
    }
}

void World::found_families() {
    // Marry the founding generation: about two men in three take a wife,
    // mostly from their own tribe, with a sensible age gap. The wife joins
    // her husband's tribe, as the custom is.
    std::vector<int> men, women;
    for (auto& p : people) {
        if (p.age() < 18 || p.age() > 60) continue;
        if (p.gender == Gender::Male) men.push_back(p.id);
        else women.push_back(p.id);
    }
    std::shuffle(men.begin(), men.end(), RNG);
    std::vector<char> taken(people.size(), 0);
    auto free_woman = [&](int tribe, int lo, int hi) -> int {
        for (int tries = 0; tries < 40; ++tries) {
            int id = pick(women);
            const Person& q = people[static_cast<size_t>(id)];
            if (taken[static_cast<size_t>(id)] || q.spouse != -1) continue;
            if (q.age() < lo || q.age() > hi) continue;
            if (chance(0.85) && q.tribe != tribe) continue;
            return id;
        }
        return -1;
    };
    for (int m : men) {
        Person& g = people[static_cast<size_t>(m)];
        if (!chance(0.72)) continue;
        int b = free_woman(g.tribe, 16, std::max(16, g.age() - 2));
        if (b < 0) continue;
        Person& bride = people[static_cast<size_t>(b)];
        bride.tribe = g.tribe;  // she joins his tribe
        g.spouse = b;
        bride.spouse = m;
        taken[static_cast<size_t>(b)] = 1;
        // Founding couples expecting: in a high-fertility society a large
        // share of wives is pregnant at any moment (a pregnancy every ~2.5
        // years means ~30% of fertile wives are expecting at any time).
        if (bride.age() >= 17 && bride.age() <= 42 && chance(0.12))
            bride.pregnant_since = -randi(0, PREGNANCY_DAYS - 1);
    }
    // A few founding widows and widowers.
    for (auto& p : people) {
        if (p.age() >= 25 && p.spouse == -1 && chance(0.06))
            p.widowed_since = -randi(30, 2000);
    }
    // Every child gets parents from a married household of their tribe whose
    // mother is old enough; the overflow is left with unknown parents.
    std::vector<int> couples;  // father ids
    for (auto& p : people)
        if (p.gender == Gender::Male && p.spouse != -1) couples.push_back(p.id);
    std::shuffle(couples.begin(), couples.end(), RNG);
    for (auto& ch : people) {
        if (ch.age() >= 16) continue;
        bool placed = false;
        for (int tries = 0; tries < 30 && !placed; ++tries) {
            int f = pick(couples);
            Person& dad = people[static_cast<size_t>(f)];
            Person& mom = people[static_cast<size_t>(dad.spouse)];
            if (mom.age() - ch.age() < 18 || mom.age() - ch.age() > 46) continue;
            if (dad.tribe != ch.tribe) continue;
            if (dad.children.size() >= 7) continue;
            ch.father = f;
            ch.mother = dad.spouse;
            dad.children.push_back(ch.id);
            mom.children.push_back(ch.id);
            placed = true;
        }
        if (!placed) ch.orphan = true;  // the Bayt al-Mal looks after them
    }
    // Founding mothers of babies are nursing them, and so not yet
    // available for the next conception.
    for (auto& p : people) {
        if (p.gender != Gender::Female || p.spouse < 0) continue;
        int youngest = -1;
        for (int c : p.children) {
            int cd = people[static_cast<size_t>(c)].age_days;
            if (youngest < 0 || cd < youngest) youngest = cd;
        }
        if (youngest >= 0 && youngest < NURSING_DAYS)
            p.nursing_until = NURSING_DAYS - youngest;
    }
}

// --- founding: the economy -------------------------------------------------------

void World::found_economy() {
    const int n_tribes = static_cast<int>(tribe_names.size());
    const int ni = n_items;

    // Roles for the working adults, by the founding fractions of roles.csv.
    std::vector<int> workers;
    for (auto& p : people)
        if (p.age() >= COMING_OF_AGE && p.age() < RETIREMENT_AGE)
            workers.push_back(p.id);
    std::shuffle(workers.begin(), workers.end(), RNG);
    size_t cursor = 0;
    auto n_workers = workers.size();
    for (const auto& r : data.roles) {
        if (r.name == "Ajer" || r.name == "Tilmidh" || r.name == "Child" ||
            r.name == "GrandSheikh" ||  // chosen from the Sheikhs below
            r.name == "Elder")
            continue;
        int want = static_cast<int>(std::round(r.frac * n_workers));
        if (want < 1 && r.frac > 0) want = 1;  // a real trade never starts empty
        int placed = 0;
        // Scan the unassigned tail for matching workers; swap each one placed
        // into the assigned prefix. workers[0..cursor) are always assigned.
        size_t i = cursor;
        while (i < n_workers && placed < want) {
            Person& p = people[workers[i]];
            if ((r.male_only && p.gender != Gender::Male) ||
                (r.female_only && p.gender != Gender::Female)) {
                ++i;
                continue;
            }
            p.role = r.name;
            std::swap(workers[i], workers[cursor]);
            ++cursor;
            ++placed;
            if (i < cursor) i = cursor;
        }
    }
    for (; cursor < n_workers; ++cursor) people[workers[cursor]].role = "Ajer";

    // Weavers are mostly women in this valley: swap where we can.
    // (A light touch: skip if the swap would break anything.)
    // The tribes each get a sheikh: the most honored elderly man.
    for (int t = 0; t < n_tribes; ++t) {
        int best = -1;
        for (auto& p : people)
            if (p.tribe == t && p.gender == Gender::Male && p.age() >= 40 &&
                p.role == "Sheikh")
                if (best < 0 || p.honor > people[static_cast<size_t>(best)].honor)
                    best = p.id;
        (void)best;
    }
    // The Grand Sheikh: the oldest Sheikh.
    int gs = -1;
    for (auto& p : people)
        if (p.role == "Sheikh" && (gs < 0 || p.age() > people[static_cast<size_t>(gs)].age()))
            gs = p.id;
    if (gs >= 0) people[static_cast<size_t>(gs)].role = "GrandSheikh";

    // Markets: one suq per tribe settlement, four shops each.
    for (int t = 0; t < n_tribes; ++t) {
        for (ShopKind kind : {ShopKind::FoodStall, ShopKind::WaterStall,
                              ShopKind::CraftShop, ShopKind::CaravanSouk}) {
            Shop s;
            s.id = static_cast<int>(shops.size());
            s.kind = kind;
            s.tribe = t;
            s.stock.assign(static_cast<size_t>(ni), 0.0);
            s.price.assign(static_cast<size_t>(ni), 0.0);
            s.target.assign(static_cast<size_t>(ni), 0.0);
            s.trades.assign(static_cast<size_t>(ni), 0);
            s.price_hist.resize(static_cast<size_t>(ni));
            shops.push_back(s);
        }
    }
    auto trades = [&](ShopKind k, int item) {
        const bool F = k == ShopKind::FoodStall, W = k == ShopKind::WaterStall,
                   C = k == ShopKind::CraftShop, V = k == ShopKind::CaravanSouk;
        if (item == ix.water) return W;
        if (item == ix.bread || item == ix.dates || item == ix.milk ||
            item == ix.laban || item == ix.cheese || item == ix.samn || item == ix.meat ||
            item == ix.salt || item == ix.spice || item == ix.sugar ||
            item == ix.coffee)
            return F;
        if (item == ix.wheat || item == ix.flour || item == ix.bran)
            return F || V;
        if (item == ix.hide || item == ix.leather || item == ix.sandals ||
            item == ix.waterskin || item == ix.wool || item == ix.yarn ||
            item == ix.rug || item == ix.tent || item == ix.tools ||
            item == ix.blade || item == ix.firewood)
            return C;
        if (item == ix.iron || item == ix.pottery || item == ix.cloth ||
            item == ix.silk || item == ix.incense || item == ix.oil)
            return V;
        return false;
    };
    for (auto& s : shops)
        for (int i = 0; i < ni; ++i) {
            if (!trades(s.kind, i)) continue;
            s.trades[static_cast<size_t>(i)] = 1;
            const ItemDef& d = data.items[static_cast<size_t>(i)];
            s.price[static_cast<size_t>(i)] =
                fils(d.base_price * randd(0.95, 1.08));
        }

    // Shop keepers: a Tajir per shop; the remaining Tajirs are merchants
    // (they store and resell through the shops of their suq).
    for (auto& s : shops) {
        for (auto& p : people) {
            if (p.role == "Tajir" && p.employer_shop < 0) {
                p.employer_shop = s.id;
                s.keeper = p.id;
                break;
            }
        }
    }
    // Ajers help in the shops: two slots per shop.
    {
        std::vector<int> ajers;
        for (auto& p : people)
            if (p.role == "Ajer") ajers.push_back(p.id);
        std::shuffle(ajers.begin(), ajers.end(), RNG);
        size_t ai = 0;
        const int slots = 1 + static_cast<int>(people.size()) / 150;
        for (auto& s : shops)
            for (int slot = 0; slot < slots && ai < ajers.size(); ++slot, ++ai)
                people[ajers[ai]].employer_shop = s.id;
    }

    // Fields for the Fallah households.
    const ItemDef& fdef = data.items[static_cast<size_t>(ix.field)];
    double ha_total = fdef.start_total * scale;
    std::vector<int> fallah;
    for (auto& p : people)
        if (p.role == "Fallah") fallah.push_back(p.id);
    for (size_t i = 0; i < fallah.size(); ++i) {
        Field f;
        f.id = static_cast<int>(fields.size());
        f.owner = fallah[i];
        f.ha = ha_total / std::max<size_t>(1, fallah.size());
        f.yield_kg_ha = fdef.aux;
        f.stock.assign(static_cast<size_t>(ni), 0.0);
        fields.push_back(f);
    }
    ground_stock[static_cast<size_t>(ix.field)] = ha_total;

    // Herds: herder families hold the goats and sheep, camel herders the
    // camels. The animals themselves arrive with the stock distribution.
    auto make_herds = [&](const char* role, int item) {
        std::vector<int> owners;
        for (auto& p : people)
            if (p.role == role) owners.push_back(p.id);
        for (int o : owners) {
            Herd h;
            h.owner = o;
            h.item = item;
            herds.push_back(h);
        }
    };
    make_herds("Herder", ix.goat);
    make_herds("Herder", ix.sheep);
    make_herds("CamelHerder", ix.camel);

    // Caravan companies and their purses.
    static const char* ORIGINS[] = {"Makkah", "al-Sham", "Yemen"};
    for (int i = 0; i < 3; ++i) {
        CaravanCo c;
        c.id = i;
        c.origin = ORIGINS[i];
        c.purse = fils(30000.0 * scale);
        c.cargo.assign(static_cast<size_t>(ni), 0.0);
        c.bought.assign(static_cast<size_t>(ni), 0.0);
        c.next_arrival = randi(4, 24);
        caravans.push_back(c);
    }

    // Institution balances (money: part of the fixed supply).
    bayt.stock.assign(static_cast<size_t>(ni), 0.0);
    majlis.stock.assign(static_cast<size_t>(ni), 0.0);
    bayt.balance = fils(15000.0 * scale);
    majlis.balance = fils(25000.0 * scale);

    // Pouches: a founding spread that is neither equal nor extreme.
    for (auto& p : people) {
        double base = randd(20, 60);
        const std::string& r = p.role;
        if (r == "GrandSheikh") base = randd(900, 1400);
        else if (r == "Sheikh") base = randd(250, 450);
        else if (r == "Tajir") base = randd(150, 320);
        else if (r == "Overseer" || r == "Qadi") base = randd(140, 240);
        else if (r == "Haddad" || r == "Jazzar" || r == "Miller" ||
                 r == "Caravaneer")
            base = randd(90, 170);
        else if (r == "Herder" || r == "CamelHerder" || r == "Fallah")
            base = randd(40, 90);  // asset-rich, cash-poor
        p.pouch = fils(base * randd(0.8, 1.3));
        if (p.pouch > 220) {  // the well-off keep some in safekeeping
            double w_ = fils(p.pouch * 0.35);
            p.wadiah = w_;
            bayt.deposits += w_;
        }
    }
    // The Bayt holds the deposited dirhams on top of its own funds.
    bayt.balance += bayt.deposits;
    bayt.deposits = fils(bayt.deposits);
    // Shop tills.
    for (auto& s : shops) s.till = fils(300.0 * scale);
}

// Distribute the starting physical stock per resources.csv: households,
// shops, institutions and the ground. Households are split poor/middle/rich
// by pouch, and the rich hold more - but never absurdly more (the startup
// checks verify the 15x rule).
void World::distribute_starting_stock() {
    const int ni = n_items;
    // Adults sorted by pouch define the household tiers.
    std::vector<int> adults;
    for (auto& p : people)
        if (p.age() >= COMING_OF_AGE) adults.push_back(p.id);
    std::sort(adults.begin(), adults.end(), [&](int a, int b) {
        return people[static_cast<size_t>(a)].pouch <
               people[static_cast<size_t>(b)].pouch;
    });
    auto tier_weight = [&](size_t rank) {
        double f = adults.empty() ? 0.5 : double(rank) / adults.size();
        if (f < 0.33) return 0.6;   // poor third
        if (f < 0.8) return 1.0;    // middle
        return 2.4;                 // rich fifth
    };

    for (int i = 0; i < ni; ++i) {
        const ItemDef& d = data.items[static_cast<size_t>(i)];
        if (d.kind == "land") continue;  // land rows already placed
        double total = d.start_total * scale;
        if (total <= 0) continue;

        // Livestock: goats/sheep to herders, camels to camel herders,
        // chickens and donkeys spread over households, the rest to the rich.
        if (d.kind == "livestock") {
            std::vector<int> owners;
            const char* pref = nullptr;
            if (i == ix.goat || i == ix.sheep) pref = "Herder";
            else if (i == ix.camel) pref = "CamelHerder";
            if (pref) {
                for (auto& p : people)
                    if (p.role == pref) owners.push_back(p.id);
            }
            if (owners.empty()) owners = adults;
            double share = total / owners.size();
            for (int o : owners)
                people[static_cast<size_t>(o)].stock[static_cast<size_t>(i)] =
                    share * randd(0.7, 1.3);
            continue;
        }

        double hh = total * d.dist_households;
        double sh = total * d.dist_shops;
        double ins = total * d.dist_institutions;
        double gr = total * d.dist_ground;

        if (hh > 0) {
            double wsum = 0;
            for (size_t r = 0; r < adults.size(); ++r) wsum += tier_weight(r);
            for (size_t r = 0; r < adults.size(); ++r)
                people[static_cast<size_t>(adults[r])]
                    .stock[static_cast<size_t>(i)] +=
                    hh * tier_weight(r) / wsum * randd(0.7, 1.3);
        }
        if (sh > 0) {
            std::vector<int> takers;
            for (auto& s : shops)
                if (s.sells(i)) takers.push_back(s.id);
            if (!takers.empty()) {
                double each = sh / takers.size();
                for (int sid : takers)
                    shops[static_cast<size_t>(sid)]
                        .stock[static_cast<size_t>(i)] += each * randd(0.8, 1.2);
            }
        }
        if (ins > 0) {
            bayt.stock[static_cast<size_t>(i)] += ins * 0.4;
            majlis.stock[static_cast<size_t>(i)] += ins * 0.6;
        }
        if (gr > 0) {
            if (i == ix.water) {
                double each = gr / wells.size();
                for (auto& wl : wells)
                    wl.stock[static_cast<size_t>(i)] += each;
            } else if (i == ix.dates) {
                double each = gr / groves.size();
                for (auto& g : groves)
                    g.stock[static_cast<size_t>(i)] += each;
            } else if (i == ix.wheat && !fields.empty()) {
                // Founding standing crop: some fields are already growing.
                double each = gr / fields.size();
                for (auto& f : fields) {
                    f.stock[static_cast<size_t>(i)] += each;
                    f.growing = true;
                    f.sown_day = -randi(10, 80);
                }
            } else {
                ground_stock[static_cast<size_t>(i)] += gr;
            }
        }
    }

    // Shop targets: a few days of the tribe's demand for daily goods, a
    // month for durables.
    const int n_tribes = static_cast<int>(tribe_names.size());
    std::vector<int> tpop(static_cast<size_t>(n_tribes), 0);
    for (auto& p : people) tpop[static_cast<size_t>(p.tribe)]++;
    for (auto& s : shops) {
        double pop = tpop[static_cast<size_t>(s.tribe)];
        // Direct household demand per day...
        std::vector<double> daily(static_cast<size_t>(ni), 0.0);
        for (int i = 0; i < ni; ++i) {
            const ItemDef& d = data.items[static_cast<size_t>(i)];
            daily[static_cast<size_t>(i)] =
                d.eat_adult_day * 0.62 * pop + d.eat_child_day * 0.28 * pop +
                d.use_household_month * pop / 4.0 / 30.0;
        }
        // ...and the demand a chain pulls through its inputs: the suq must
        // stock wheat, flour and firewood because the suq sells bread.
        // Three passes carry demand down a three-link chain.
        for (int pass = 0; pass < 3; ++pass)
            for (const auto& r : data.recipes) {
                int o = data.item_index(r.out_name.empty() ? "" : r.out_name[0]);
                if (o < 0 || r.out_qty.empty() || r.out_qty[0] <= 0) continue;
                double runs = daily[static_cast<size_t>(o)] / r.out_qty[0];
                for (size_t k = 0; k < r.in_name.size(); ++k) {
                    int ii = data.item_index(r.in_name[k]);
                    if (ii >= 0)
                        daily[static_cast<size_t>(ii)] += runs * r.in_qty[k];
                }
            }
        for (int i = 0; i < ni; ++i) {
            if (!s.sells(i)) continue;
            const ItemDef& d = data.items[static_cast<size_t>(i)];
            s.target[static_cast<size_t>(i)] =
                std::max(2.0, daily[static_cast<size_t>(i)] *
                                  (d.shelf_life > 0 && d.shelf_life < 30
                                       ? 2.5
                                       : 6.0));
        }
    }
}

// --- the tick -------------------------------------------------------------------

void World::step() {
    for (auto& b : books) {
        b.produced_today = b.consumed_today = 0;
        b.imported_today = b.exported_today = 0;
    }
    births_today = deaths_today = hungry_today = thirsty_today = 0;
    shop_pick_cache.clear();

    demographics_daily(*this);   // aging, deaths, births, marriages, trades
    majlis_daily(*this);         // the council pays its people at dawn
    bayt_daily(*this);           // sadaqah before the day's shopping
    herds_daily(*this);          // grazing, breeding, milking, slaughter prep
    economy_daily(*this);        // chains, crafts, shopping, services
    caravans_daily(*this);       // the roads of the desert

    // Shops close the day: prices drift, perishables spoil, keepers draw.
    extern void shop_close_day(World& w, Shop& s);
    for (auto& s : shops) shop_close_day(*this, s);

    if (tick > 0 && tick % 360 == 0) zakat_annual(*this);

    // The hard day-end checks of section 4b.
    assert_money_supply();
    assert_stock_books(*this);

    ++tick;
}

void World::assert_money_supply() const {
    double now = total_money(*this);
    double expected = initial_supply + mint.coined;
    if (std::fabs(now - expected) > std::max(0.01, expected * 1e-9)) {
        std::cerr << "\nMONEY SUPPLY CHECK FAILED day " << tick << ": total "
                  << now << " vs initial " << initial_supply << " + minted "
                  << mint.coined << "\n";
        std::abort();
    }
}

double World::gini() const {
    std::vector<double> v;
    v.reserve(people.size());
    for (const auto& p : people)
        if (p.alive) v.push_back(p.wealth());
    return gini_of(v);
}

uint64_t World::state_hash() const {
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ULL;
    };
    for (const auto& p : people) {
        mix(static_cast<uint64_t>(p.alive));
        mix(static_cast<uint64_t>(std::llround(p.pouch * 100)));
        mix(static_cast<uint64_t>(std::llround(p.hunger * 100)));
        mix(static_cast<uint64_t>(p.age_days));
    }
    for (const auto& s : shops)
        mix(static_cast<uint64_t>(std::llround(s.till * 100)));
    for (size_t i = 0; i < books.size(); ++i)
        mix(static_cast<uint64_t>(
            std::llround(total_holdings(*this, static_cast<int>(i)) * 1000)));
    return h;
}

// --- the startup balance checks (section 4d) ------------------------------------

static void print_check(const std::string& name, bool ok,
                        const std::string& detail) {
    std::cout << "  check: " << name;
    if (name.size() < 46) std::cout << std::string(46 - name.size(), '.');
    std::cout << " " << (ok ? "PASS" : "FAIL") << "  " << detail << "\n";
}

void run_balance_checks(const World& w) {
    std::cout << "Startup balance checks (data/resources.csv):\n";
    const Data& data = w.data;
    // Rough population mix for consumption arithmetic.
    int adults = 0, children = 0, elders = 0, households = 0;
    for (const auto& p : w.people) {
        if (p.age() < 16) ++children;
        else if (p.age() >= RETIREMENT_AGE) ++elders;
        else ++adults;
    }
    households = std::max(1, adults / 2);
    auto daily_need = [&](const ItemDef& d) {
        return d.eat_adult_day * adults + d.eat_child_day * children +
               d.eat_adult_day * 0.9 * elders +
               d.use_household_month * households / 30.0;
    };
    // Days of cover an item's stock gives, following the chain upstream
    // (wheat counts toward flour, flour toward bread).
    std::function<double(int, int)> cover = [&](int item, int depth) {
        double stock = total_holdings(w, item);
        if (depth < 3) {
            for (const auto& r : data.recipes) {
                for (size_t k = 0; k < r.out_name.size(); ++k)
                    if (r.out_name[k] == data.items[static_cast<size_t>(item)].name)
                        for (size_t j = 0; j < r.in_name.size(); ++j) {
                            int in = data.item_index(r.in_name[j]);
                            if (in >= 0)
                                stock += cover(in, depth + 1) *
                                         (r.out_qty[k] / r.in_qty[j]);
                        }
            }
        }
        return stock;
    };

    // Chain inputs (wheat, flour) are eaten only downstream: their daily
    // need is what the recipes pull through them.
    auto chain_need = [&](int idx) {
        std::vector<double> direct(static_cast<size_t>(w.n_items), 0.0);
        for (int i = 0; i < w.n_items; ++i)
            direct[static_cast<size_t>(i)] =
                daily_need(data.items[static_cast<size_t>(i)]);
        // Iterate to a fixed point: each pass derives input demand from the
        // current output demand, so nothing is counted twice.
        std::vector<double> dem = direct;
        for (int pass = 0; pass < 4; ++pass) {
            std::vector<double> inputs(static_cast<size_t>(w.n_items), 0.0);
            for (const auto& r : data.recipes) {
                int o = data.item_index(r.out_name[0]);
                if (o < 0 || r.out_qty[0] <= 0) continue;
                double runs = dem[static_cast<size_t>(o)] / r.out_qty[0];
                for (size_t k = 0; k < r.in_name.size(); ++k) {
                    int ii = data.item_index(r.in_name[k]);
                    if (ii >= 0)
                        inputs[static_cast<size_t>(ii)] += runs * r.in_qty[k];
                }
            }
            for (int i = 0; i < w.n_items; ++i)
                dem[static_cast<size_t>(i)] =
                    direct[static_cast<size_t>(i)] +
                    inputs[static_cast<size_t>(i)];
        }
        return dem[static_cast<size_t>(idx)];
    };
    // 1. Storable food and water: 60 days of cover with no caravan.
    for (const char* n : {"water", "dates", "wheat", "flour", "salt"}) {
        const ItemDef* d = data.item(n);
        double need = std::max(daily_need(*d),
                               chain_need(data.item_index(n)));
        // For flour/bread-style chain items the cover walk counts upstream.
        double days = cover(data.item_index(n), 0) / std::max(1e-9, need);
        char buf[96];
        snprintf(buf, sizeof buf, "(%.0f days of %.1f %s/day)", days, need,
                 d->unit.c_str());
        print_check(std::string("60-day cover: ") + n, days >= 60.0, buf);
    }
    // Bread is baked daily: its cover lives in the flour and wheat stores.
    {
        const ItemDef* d = data.item("bread");
        double need = daily_need(*d);
        double days = cover(w.ix.bread, 0) / std::max(1e-9, need);
        char buf[96];
        snprintf(buf, sizeof buf, "(%.0f days incl. flour and wheat)", days);
        print_check("60-day cover: bread (chain)", days >= 60.0, buf);
    }
    // Perishables: local production covers daily consumption.
    for (const char* n : {"milk", "meat"}) {
        const ItemDef* d = data.item(n);
        double need = daily_need(*d);
        double prod = 0;
        if (std::string(n) == "milk") {
            double heads = total_holdings(w, w.ix.goat) +
                           total_holdings(w, w.ix.sheep) +
                           total_holdings(w, w.ix.camel) * 4;
            prod = heads * 0.6 * 0.45 * 0.4;  // females, lactating, liters
        } else {
            double herd = total_holdings(w, w.ix.goat) +
                          total_holdings(w, w.ix.sheep);
            double births_year =
                herd * 0.6 * (365.0 / (150 + 90)) * 0.85 * 1.2;
            prod = (births_year / 365.0) * 11.5;  // kg meat per head
        }
        char buf[96];
        snprintf(buf, sizeof buf, "(need %.1f, production %.1f %s/day)", need,
                 prod, d->unit.c_str());
        print_check(std::string("production covers: ") + n, prod >= need, buf);
    }
    // 2. Imports: limited stock, and the caravans can carry the yearly need.
    for (size_t i = 0; i < data.items.size(); ++i) {
        const ItemDef& d = data.items[i];
        if (d.local || d.kind != "good") continue;
        double need = daily_need(d);
        if (need <= 0) continue;
        double days = total_holdings(w, static_cast<int>(i)) / need;
        double yearly = need * 360;
        double capacity = d.import_per_caravan * 3.0 * (360.0 / 20.0);
        char buf[128];
        snprintf(buf, sizeof buf, "(%.0f days stocked; caravans carry %.0f of %.0f %s/yr)",
                 days, capacity, yearly, d.unit.c_str());
        print_check(std::string("caravans needed: ") + d.name,
                    days < 180.0 && capacity >= yearly, buf);
    }
    // 3. Wells: yield covers people and herds.
    {
        double human = 3.5 * adults + 2.0 * children + 3.2 * elders;
        double animals = (total_holdings(w, w.ix.goat) +
                          total_holdings(w, w.ix.sheep)) * 4.0 +
                         total_holdings(w, w.ix.camel) * 9.0 +
                         total_holdings(w, w.ix.donkey) * 7.0 +
                         total_holdings(w, w.ix.cattle) * 30.0 +
                         total_holdings(w, w.ix.horse) * 20.0 +
                         human * 0.15;  // household use
        double yield = 0;
        for (const auto& wl : w.wells) yield += wl.yield_day;
        char buf[96];
        snprintf(buf, sizeof buf, "(need %.0f L/day, wells yield %.0f)",
                 human + animals, yield);
        print_check("wells sustain the valley", yield >= human + animals, buf);
    }
    // 4. No household starts at zero of a basic need.
    {
        int zero = 0;
        for (const auto& p : w.people) {
            if (p.age() < COMING_OF_AGE) continue;
            bool ok = p.stock[static_cast<size_t>(w.ix.water)] > 0 &&
                      (p.stock[static_cast<size_t>(w.ix.dates)] > 0 ||
                       p.stock[static_cast<size_t>(w.ix.bread)] > 0 ||
                       p.stock[static_cast<size_t>(w.ix.flour)] > 0 ||
                       p.stock[static_cast<size_t>(w.ix.wheat)] > 0);
            if (!ok) ++zero;
        }
        char buf[64];
        snprintf(buf, sizeof buf, "(%d households at zero)", zero);
        print_check("no household starts at zero", zero == 0, buf);
    }
    // 5. The richest household holds at most 15x the median, per item.
    {
        double worst = 0;
        std::string worst_item;
        for (int i = 0; i < w.n_items; ++i) {
            std::vector<double> held;
            for (const auto& p : w.people)
                if (p.age() >= COMING_OF_AGE)
                    held.push_back(p.stock[static_cast<size_t>(i)]);
            if (held.empty()) continue;
            std::sort(held.begin(), held.end());
            double med = held[held.size() / 2];
            if (med <= 1e-9) continue;
            double mx = held.back();
            if (mx / med > worst) {
                worst = mx / med;
                worst_item = w.data.items[static_cast<size_t>(i)].name;
            }
        }
        char buf[96];
        snprintf(buf, sizeof buf, "(worst: %s at %.1fx median)",
                 worst_item.c_str(), worst);
        print_check("rich at most 15x the median", worst <= 15.0, buf);
    }
}

}  // namespace tribes
