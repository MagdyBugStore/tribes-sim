// activity.cpp: see activity.hpp.
#include "activity.hpp"

#include <cstring>
#include <map>

namespace tribes {

namespace {

struct ZoneRow { const char* name; const char* en; const char* ar; };
const ZoneRow ZONE_TABLE[] = {
    {"home", "Home", "البيت"},
    {"well", "Well", "البئر"},
    {"field", "Field", "الحقل"},
    {"grove", "Date grove", "النخيل"},
    {"pasture", "Pasture", "المرعى"},
    {"suq", "Suq", "السوق"},
    {"mosque", "Mosque", "المسجد"},
    {"majlis", "Majlis", "المجلس"},
    {"workshop", "Workshop", "الورشة"},
    {"caravan_road", "Caravan road", "طريق القافلة"},
    {"salt_pan", "Salt pan", "ملاحة الملح"},
    {"school", "School", "الكتّاب"},
    {"clinic", "Clinic", "دار الطبيب"},
    {"gate", "Settlement gate", "مدخل الحي"},
    {"desert", "Open desert", "البادية"},
    {"bayt_al_mal", "Bayt al-Mal", "بيت المال"},
};
static_assert(sizeof(ZONE_TABLE) / sizeof(ZONE_TABLE[0]) == static_cast<size_t>(Zone::Count_),
              "ZONE_TABLE must have one row per Zone");

// Every activity, in one table.
enum Act {
    SLEEP, REST_SICK, REST_HOME, REST_HEAT, PRAY_MOSQUE, PRAY_HOME, PRAY_AWAY, JUMUAH,
    EAT_HOME, EAT_AWAY, EAT_SUQ, FETCH_WATER, VISIT_SUQ, VISIT_NEIGHBORS, SIT_MAJLIS,
    PLAY, STUDY, TEACH, CARE_CHILDREN, HOUSEWORK, COUNCIL, JUDGE, LEAD_PRAYER,
    CALL_PRAYER, HEAL, MIDWIFE, COMPOSE, WATCH_NIGHT, GUARD_GATE, GUARD_CARAVAN,
    KEEP_BOOKS, SUPERVISE_SUQ, PICK_DATES, DIG_WELL, TRADE_SUQ, TRADE_CARAVAN,
    LABOUR, FARM, TEND_WELL, HERD, HERD_CAMELS, MILK, DAIRY, HUNT, FALCON, MILL,
    BAKE, WEAVE, TAN, SMITH, DIG_SALT, GATHER_WOOD, BUTCHER, PREP_CARAVAN,
    CARAVAN_ROAD, BARBER, GUIDE, TELL_STORIES, DECEASED, ACT_COUNT, NONE_ = -1
};

using K = ActKind;
using Z = Zone;
const Activity ACTS[ACT_COUNT] = {
    {"sleep_home", "Sleeping at home", "النوم في البيت", Z::Home, K::Sleep},
    {"rest_sick", "Resting because sick", "يستريح لأنه مريض", Z::Home, K::Rest},
    {"rest_home", "Resting at home", "راحة في البيت", Z::Home, K::Rest},
    {"rest_heat", "Resting from the midday heat", "راحة من حرّ الظهر", Z::Home, K::Rest},
    {"pray_mosque", "Praying at the mosque", "الصلاة في المسجد", Z::Mosque, K::Prayer},
    {"pray_home", "Praying at home", "الصلاة في البيت", Z::Home, K::Prayer},
    {"pray_away", "Praying where he works", "الصلاة في مكان العمل", Z::Home, K::Prayer},
    {"jumuah", "Attending the Jumu'ah prayer", "صلاة الجمعة", Z::Mosque, K::Prayer},
    {"eat_home", "Eating at home", "الأكل في البيت", Z::Home, K::Meal},
    {"eat_away", "Eating a packed meal", "الأكل في مكان العمل", Z::Home, K::Meal},
    {"eat_suq", "Buying food at the suq", "شراء الطعام من السوق", Z::Suq, K::Meal},
    {"fetch_water", "Fetching water at the well", "جلب الماء من البئر", Z::Well, K::Need},
    {"visit_suq", "Strolling through the suq", "التجوّل في السوق", Z::Suq, K::Social},
    {"visit_neighbors", "Visiting the neighbors", "زيارة الجيران", Z::Home, K::Social},
    {"sit_majlis", "Sitting in the majlis", "الجلوس في المجلس", Z::Majlis, K::Social},
    {"play", "Playing", "اللعب", Z::Home, K::Rest},
    {"study", "Studying with the mu'allim", "الدرس عند المعلّم", Z::School, K::Study},
    {"teach", "Teaching at the school", "التدريس في الكتّاب", Z::School, K::Work},
    {"care_children", "Caring for the children", "رعاية الأطفال", Z::Home, K::Care},
    {"housework", "Doing the housework", "أعمال البيت", Z::Home, K::Work},
    {"council", "Sitting in council", "جلسة المجلس", Z::Majlis, K::Work},
    {"judge", "Judging between people", "القضاء بين الناس", Z::Majlis, K::Work},
    {"lead_prayer", "Leading the prayer", "إمامة الصلاة", Z::Mosque, K::Work},
    {"call_prayer", "Calling the adhan", "رفع الأذان", Z::Mosque, K::Work},
    {"heal", "Healing the sick", "علاج المرضى", Z::Clinic, K::Work},
    {"midwife", "Attending mothers and births", "رعاية الأمهات والولادة", Z::Clinic, K::Work},
    {"compose", "Composing poetry", "نظم الشعر", Z::Majlis, K::Work},
    {"watch_night", "Keeping the night watch", "حراسة الليل", Z::Gate, K::Work},
    {"guard_gate", "Guarding the settlement", "حراسة الديار", Z::Gate, K::Work},
    {"guard_caravan", "Guarding the caravan", "حراسة القافلة", Z::CaravanRoad, K::Work},
    {"keep_books", "Keeping the Bayt al-Mal accounts", "حفظ حسابات بيت المال", Z::BaytAlMal, K::Work},
    {"supervise_suq", "Supervising the suq", "مراقبة السوق", Z::Suq, K::Work},
    {"pick_dates", "Picking dates at the grove", "جني التمر في النخيل", Z::Grove, K::Work},
    {"dig_well", "Digging the well", "حفر البئر", Z::Well, K::Work},
    {"trade_suq", "Trading at the suq", "التجارة في السوق", Z::Suq, K::Work},
    {"trade_caravan", "Trading with the caravan merchants", "التجارة مع تجّار القافلة", Z::Suq, K::Work},
    {"labour", "Working as a day laborer", "العمل باليومية", Z::Field, K::Work},
    {"farm", "Sowing and harvesting at the field", "الزراعة والحصاد في الحقل", Z::Field, K::Work},
    {"tend_well", "Tending the well", "رعاية البئر", Z::Well, K::Work},
    {"herd", "Herding goats and sheep at the pasture", "رعي الغنم والماعز في المرعى", Z::Pasture, K::Work},
    {"herd_camels", "Herding camels at the pasture", "رعي الإبل في المرعى", Z::Pasture, K::Work},
    {"milk", "Milking the herd", "حلب القطيع", Z::Pasture, K::Work},
    {"dairy", "Making cheese and laban", "صنع الجبن واللبن", Z::Workshop, K::Work},
    {"hunt", "Hunting in the desert", "الصيد في البادية", Z::Desert, K::Work},
    {"falcon", "Hunting with the falcon", "الصيد بالصقر", Z::Desert, K::Work},
    {"mill", "Grinding grain at the mill", "طحن الحبوب في الطاحونة", Z::Workshop, K::Work},
    {"bake", "Baking bread at the oven", "خبز الخبز في الفرن", Z::Workshop, K::Work},
    {"weave", "Weaving cloth and rugs", "نسج القماش والسجاد", Z::Workshop, K::Work},
    {"tan", "Tanning hides", "دباغة الجلود", Z::Workshop, K::Work},
    {"smith", "Smithing iron", "الحدادة", Z::Workshop, K::Work},
    {"dig_salt", "Digging salt at the salt pan", "استخراج الملح من الملاحة", Z::SaltPan, K::Work},
    {"gather_wood", "Gathering firewood", "جمع الحطب", Z::Desert, K::Work},
    {"butcher", "Butchering at the suq", "الذبح والجزارة في السوق", Z::Suq, K::Work},
    {"prep_caravan", "Preparing the caravan camels", "تجهيز إبل القافلة", Z::Pasture, K::Work},
    {"caravan_road", "Travelling the caravan road", "السير في طريق القافلة", Z::CaravanRoad, K::Work},
    {"barber", "Cutting hair and cupping", "الحلاقة والحجامة", Z::Suq, K::Work},
    {"guide", "Guiding travelers", "دلالة المسافرين", Z::Desert, K::Work},
    {"tell_stories", "Telling stories at the suq", "حكاية القصص في السوق", Z::Suq, K::Work},
    {"deceased", "Deceased", "متوفى", Z::Home, K::Dead},
};

// A work block: which activity, over which hours (mask), and what changes
// on a caravan day. Mask letters: s sleep, R free time at home, W the main
// work, M the second work activity (alt). 24 letters, hour 0 first.
//   STD   sssss R WWWWWWW RRR WW RRR sss
//   FARM  field in the cool morning and evening, rest at noon
//   HERD  pasture most of the day, home at dusk
//   BAKER oven before dawn, then at the oven again after Fajr
//   WATCH night watch, sleeps in the morning
struct WorkBlock {
    const char* role;
    Act work;
    const char* mask;
    Act caravan;   // NONE_ = unchanged on a caravan day
    Act alt;       // for 'M' hours
};
constexpr const char* M_STD   = "sssssRWWWWWWWRRRWWRRRsss";
constexpr const char* M_FARM  = "sssssRWWWWWRRRRRWWRRRsss";
constexpr const char* M_HERD  = "sssssRWWWWWWWWWWWWRRRsss";
constexpr const char* M_BAKER = "ssWWWRWWWWRRRRRRRRRRRsss";
constexpr const char* M_WATCH = "WWWWWRsssssssRRRRRRRRRWW";
constexpr const char* M_EVEN  = "sssssRRRRWWWRRRWWRRRRsss";
constexpr const char* M_DAIRY = "sssssRMMWWWWWRRRMMRRRsss";

const WorkBlock BLOCKS[] = {
    {"GrandSheikh", COUNCIL, M_STD, NONE_, NONE_},
    {"Sheikh", COUNCIL, M_STD, NONE_, NONE_},
    {"Imam", LEAD_PRAYER, M_STD, NONE_, NONE_},
    {"Muezzin", CALL_PRAYER, M_STD, NONE_, NONE_},
    {"Qadi", JUDGE, M_STD, NONE_, NONE_},
    {"Muallim", TEACH, M_STD, NONE_, NONE_},
    {"Hakim", HEAL, M_STD, NONE_, NONE_},
    {"Qabila", MIDWIFE, M_STD, NONE_, NONE_},
    {"Shair", COMPOSE, M_STD, NONE_, NONE_},
    {"Haris", WATCH_NIGHT, M_WATCH, NONE_, NONE_},
    {"Faris", GUARD_GATE, M_STD, GUARD_CARAVAN, NONE_},
    {"Khazin", KEEP_BOOKS, M_STD, NONE_, NONE_},
    {"Overseer", SUPERVISE_SUQ, M_STD, NONE_, NONE_},
    {"Nakhali", PICK_DATES, M_STD, NONE_, NONE_},
    {"WellDigger", DIG_WELL, M_STD, NONE_, NONE_},
    {"Tajir", TRADE_SUQ, M_STD, TRADE_CARAVAN, NONE_},
    {"Ajer", LABOUR, M_STD, NONE_, NONE_},
    {"Fallah", FARM, M_FARM, NONE_, NONE_},
    {"WellKeeper", TEND_WELL, M_STD, NONE_, NONE_},
    {"Herder", HERD, M_HERD, NONE_, NONE_},
    {"CamelHerder", HERD_CAMELS, M_HERD, NONE_, NONE_},
    {"Dairyman", DAIRY, M_DAIRY, NONE_, MILK},
    {"Hunter", HUNT, M_STD, NONE_, NONE_},
    {"Falconer", FALCON, M_STD, NONE_, NONE_},
    {"Miller", MILL, M_STD, NONE_, NONE_},
    {"Baker", BAKE, M_BAKER, NONE_, NONE_},
    {"Weaver", WEAVE, M_STD, NONE_, NONE_},
    {"Tanner", TAN, M_STD, NONE_, NONE_},
    {"Haddad", SMITH, M_STD, NONE_, NONE_},
    {"SaltDigger", DIG_SALT, M_STD, NONE_, NONE_},
    {"Hattab", GATHER_WOOD, M_STD, NONE_, NONE_},
    {"Jazzar", BUTCHER, M_STD, NONE_, NONE_},
    {"Khaddam", HOUSEWORK, M_STD, NONE_, NONE_},
    {"Caravaneer", PREP_CARAVAN, M_STD, CARAVAN_ROAD, NONE_},
    {"CamelDriver", PREP_CARAVAN, M_STD, CARAVAN_ROAD, NONE_},
    {"Hakawati", TELL_STORIES, M_EVEN, NONE_, NONE_},
    {"Muzayyin", BARBER, M_STD, NONE_, NONE_},
    {"Dalil", GUIDE, M_STD, CARAVAN_ROAD, NONE_},
    // Not trades in the work sense; they follow the age/elder paths below,
    // but still carry a block so every roles.csv row is defined.
    {"Tilmidh", STUDY, M_STD, NONE_, NONE_},
    {"Child", PLAY, M_STD, NONE_, NONE_},
    {"Elder", SIT_MAJLIS, M_STD, NONE_, NONE_},
};

constexpr size_t mask_len(const char* s) { return std::char_traits<char>::length(s); }
static_assert(mask_len(M_STD) == 24 && mask_len(M_FARM) == 24 && mask_len(M_HERD) == 24 &&
              mask_len(M_BAKER) == 24 && mask_len(M_WATCH) == 24 && mask_len(M_EVEN) == 24 &&
              mask_len(M_DAIRY) == 24, "every work mask has 24 hourly letters");

const WorkBlock* block_of(const std::string& role) {
    for (const auto& b : BLOCKS)
        if (role == b.role) return &b;
    return nullptr;
}

bool outdoor_heat_zone(Zone z) {
    return z == Zone::Field || z == Zone::Grove || z == Zone::SaltPan || z == Zone::Desert;
}
bool remote_zone(Zone z) {
    return outdoor_heat_zone(z) || z == Zone::Pasture || z == Zone::CaravanRoad;
}
bool heavy_zone(Zone z) {
    return remote_zone(z);
}

constexpr int FAJR = 5, DHUHR = 13, ASR = 15, MAGHRIB = 18, ISHA = 20, LUNCH = 14;
constexpr int PRAYER_AGE = 12;

}  // namespace

const char* zone_name(Zone z) { return ZONE_TABLE[static_cast<int>(z)].name; }
const char* zone_label_en(Zone z) { return ZONE_TABLE[static_cast<int>(z)].en; }
const char* zone_label_ar(Zone z) { return ZONE_TABLE[static_cast<int>(z)].ar; }

uint64_t activity_hash(int person_id, int day, int hour) {
    uint64_t x = (static_cast<uint64_t>(static_cast<uint32_t>(person_id)) << 32) ^
                 (static_cast<uint64_t>(static_cast<uint32_t>(day)) << 5) ^
                 static_cast<uint64_t>(hour);
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

bool has_work_block(const std::string& role) { return block_of(role) != nullptr; }

std::vector<Activity> schedule_for(const World& w, const Person& p, int day) {
    std::vector<Activity> out(HOURS_PER_DAY, ACTS[SLEEP]);
    if (!p.alive) {
        for (auto& a : out) a = ACTS[DECEASED];
        return out;
    }
    const bool now = day == w.tick;
    const bool male = p.gender == Gender::Male;
    const int age = p.age();
    const bool friday = is_friday(day);
    const int doy = ((day % 360) + 360) % 360;
    const bool summer = doy >= 150 && doy < 240;
    auto S = [&](Act a) { return ACTS[a]; };
    auto zoned = [&](Act a, Zone z) { Activity x = ACTS[a]; x.zone = z; return x; };
    auto is_sleep_hour = [&](int h) { return h >= 21 || h <= 4; };

    // ---- children and youth -------------------------------------------------
    if (age < COMING_OF_AGE) {
        const bool little = age < 7;
        for (int h = 0; h < 24; ++h) {
            const bool night = little ? (h >= 19 || h < 6) : (h >= 20 || h < 5);
            Activity a = S(PLAY);
            if (night) a = S(SLEEP);
            // Kuttab/maktab day (S6 Iranica): from early morning, closing at
            // noon for commoners' children, who then help the family trade
            // or household. Friday off (the Jumu'ah day, as before).
            else if (!little && !friday && h >= 6 && h <= 12 && h != 7) a = S(STUDY);
            else if (h == LUNCH || h == 19 || h == 7) a = S(EAT_HOME);
            else if (!little && h >= 15 && h <= 17) a = S(HOUSEWORK);
            if (p.sick && a.kind != ActKind::Sleep) a = S(REST_SICK);
            if (age >= PRAYER_AGE && !night) {
                if (h == FAJR || h == DHUHR || h == ASR || h == MAGHRIB || h == ISHA)
                    a = male && !p.sick ? S(PRAY_MOSQUE) : S(PRAY_HOME);
                if (friday && (h == 12 || h == 13)) a = p.sick ? S(PRAY_HOME) : S(JUMUAH);
            }
            out[static_cast<size_t>(h)] = a;
        }
        if (age >= PRAYER_AGE && !is_sleep_hour(FAJR))
            out[FAJR] = p.sick || !male ? S(PRAY_HOME) : S(PRAY_MOSQUE);
        return out;
    }

    // ---- the day's inputs ---------------------------------------------------------
    bool caravan_day = false;
    if (now)
        for (const auto& c : w.caravans)
            if (c.state == CaravanState::InValley) caravan_day = true;
    const bool elder = p.role == "Elder" || p.retired;
    const WorkBlock* blk = block_of(p.role);
    const char* mask = M_STD;
    Act work = SIT_MAJLIS, alt = NONE_, cwork = NONE_;
    if (!elder) {
        if (blk) { mask = blk->mask; work = blk->work; alt = blk->alt; cwork = blk->caravan; }
        else { work = LABOUR; }   // an unknown role works as a day laborer
    }
    const bool pregnant_late = p.pregnant_since != -1 && w.tick - p.pregnant_since > 180;
    bool young_children = p.nursing_until > day;
    if (!male && !young_children)
        for (int c : p.children)
            if (c >= 0 && c < static_cast<int>(w.people.size())) {
                const Person& k = w.people[static_cast<size_t>(c)];
                if (k.alive && k.age() < 7) { young_children = true; break; }
            }

    // ---- base from the mask ---------------------------------------------------------
    Act main_act = work;
    if (caravan_day && cwork != NONE_) main_act = cwork;
    Zone work_zone = ACTS[main_act].zone;
    for (int h = 0; h < 24; ++h) {
        char m = elder ? ((h >= 8 && h <= 12) || h == 16 || h == 17 ? 'E' : M_STD[h]) : mask[h];
        Activity a;
        switch (m) {
            case 's': a = S(SLEEP); break;
            case 'W': a = S(main_act); break;
            case 'M': a = alt != NONE_ ? S(alt) : S(main_act); break;
            case 'E': a = male ? S(SIT_MAJLIS) : S(VISIT_NEIGHBORS); break;
            default: a = S(REST_HOME); break;
        }
        // Work that is not a W hour keeps its location logic from the mask.
        if (a.kind == ActKind::Work) {
            if (pregnant_late && !male && heavy_zone(a.zone)) a = S(HOUSEWORK);
            else if (summer && outdoor_heat_zone(a.zone) && h >= 11 && h <= 14) a = S(REST_HEAT);
            else if (friday && main_act == TEACH) a = S(REST_HOME);
        }
        out[static_cast<size_t>(h)] = a;
    }

    // ---- free hours: variation, childcare ---------------------------------------------
    for (int h = 0; h < 24; ++h) {
        Activity& a = out[static_cast<size_t>(h)];
        if (a.name != ACTS[REST_HOME].name) continue;
        if (young_children && h >= 6 && h <= 17) { a = S(CARE_CHILDREN); continue; }
        if (((h >= 6 && h <= 10) || (h >= 15 && h <= 19)) && activity_hash(p.id, day, h) % 6 == 0)
            a = male ? S(VISIT_SUQ) : S(VISIT_NEIGHBORS);
    }

    // ---- meals ------------------------------------------------------------------------------
    {
        Activity& a = out[LUNCH];
        const bool away = !elder && remote_zone(work_zone) &&
                          (mask[LUNCH + 1] == 'W' || mask[LUNCH - 1] == 'W');
        if (a.kind != ActKind::Sleep) a = away ? zoned(EAT_AWAY, work_zone) : S(EAT_HOME);
        if (out[19].kind != ActKind::Sleep) out[19] = S(EAT_HOME);
    }

    // ---- prayers -------------------------------------------------------------------------------
    if (age >= PRAYER_AGE) {
        const int hours[] = {FAJR, DHUHR, ASR, MAGHRIB, ISHA};
        for (int h : hours) {
            const char before = mask[h > 0 ? h - 1 : 0], after = mask[h < 23 ? h + 1 : 23];
            Activity a;
            if (p.sick || !male) a = S(PRAY_HOME);
            else if (remote_zone(work_zone) && !elder && (before == 'W' || after == 'W') &&
                     (h == DHUHR || h == ASR))
                a = zoned(PRAY_AWAY, work_zone);
            else a = S(PRAY_MOSQUE);
            out[static_cast<size_t>(h)] = a;
        }
        if (friday)
            for (int h : {12, 13}) out[static_cast<size_t>(h)] = (p.sick) ? S(PRAY_HOME) : S(JUMUAH);
        // Muezzin and Imam are at the mosque for every prayer, and work there.
        if (p.role == "Muezzin" || p.role == "Imam") {
            for (int h : hours)
                out[static_cast<size_t>(h)] = (h == FAJR || h == DHUHR || h == ASR ||
                                               h == MAGHRIB || h == ISHA) && !p.sick
                    ? S(p.role == "Muezzin" ? CALL_PRAYER : LEAD_PRAYER) : out[static_cast<size_t>(h)];
        }
    }

    // ---- sickness: stay home ----------------------------------------------------------------------
    if (p.sick)
        for (auto& a : out)
            if (a.kind != ActKind::Sleep && a.kind != ActKind::Prayer && a.kind != ActKind::Meal)
                a = S(REST_SICK);

    // ---- needs interrupt (only for "now") ----------------------------------------------------------
    if (now && !p.sick) {
        auto free_hour = [&](int h) {
            const Activity& a = out[static_cast<size_t>(h)];
            return a.kind != ActKind::Sleep && a.kind != ActKind::Prayer;
        };
        if (p.thirst < 30) {
            int h = 7 + static_cast<int>(activity_hash(p.id, day, 100) % 11);
            for (int i = 0; i < 11 && !free_hour(h); ++i) h = 7 + (h - 7 + 1) % 11;
            if (free_hour(h)) out[static_cast<size_t>(h)] = S(FETCH_WATER);
        }
        if (p.hunger < 30) {
            for (int h : {12, 11, 10}) {
                if (!free_hour(h)) continue;
                out[static_cast<size_t>(h)] = p.pouch > 5.0 ? S(EAT_SUQ) : S(EAT_HOME);
                break;
            }
        }
    }
    return out;
}

// --- console helpers ------------------------------------------------------------------

static std::string hh(int h) {
    char b[8];
    std::snprintf(b, sizeof b, "%02d:00", h);
    return b;
}

std::string describe_schedule(const World& w, int pid, int day) {
    if (pid < 0 || pid >= static_cast<int>(w.people.size())) return "No such person.";
    const Person& p = w.people[static_cast<size_t>(pid)];
    auto s = schedule_for(w, p, day);
    std::string o = "Day " + std::to_string(day) + (is_friday(day) ? " (Friday)" : "") + ": " +
                    w.full_name(p) + ", " + p.role + ", age " + std::to_string(p.age()) +
                    (p.sick ? ", sick" : "") + "\n";
    for (int h = 0; h < 24; ++h) {
        const Activity& a = s[static_cast<size_t>(h)];
        char line[200];
        std::snprintf(line, sizeof line, "  %s  %-12s %s  /  %s\n", hh(h).c_str(),
                      zone_name(a.zone), a.en, a.ar);
        o += line;
    }
    return o;
}

std::string describe_activity_counts(const World& w, int hour) {
    if (hour < 0 || hour > 23) return "hour must be 0..23";
    std::map<std::string, int> n;
    int total = 0;
    for (const auto& p : w.people) {
        if (!p.alive) continue;
        ++total;
        n[schedule_for(w, p, w.tick)[static_cast<size_t>(hour)].en]++;
    }
    std::vector<std::pair<int, std::string>> v;
    for (auto& kv : n) v.push_back({kv.second, kv.first});
    std::sort(v.rbegin(), v.rend());
    std::string o = "Day " + std::to_string(w.tick) + ", " + hh(hour) + ": " +
                    std::to_string(total) + " living people\n";
    for (auto& e : v) {
        char line[160];
        std::snprintf(line, sizeof line, "  %5d  %s\n", e.first, e.second.c_str());
        o += line;
    }
    return o;
}

std::string role_activity_table(const World& w) {
    std::string o = "| role | 06:00 | 12:00 | 18:00 |\n|---|---|---|---|\n";
    for (const auto& r : w.data.roles) {
        Person p;
        p.id = 0;
        p.alive = true;
        p.role = r.name;
        p.gender = r.female_only ? Gender::Female : Gender::Male;
        p.age_days = (r.name == "Child" ? 5 : r.name == "Tilmidh" ? 10 : r.name == "Elder" ? 65 : 30) * 365;
        auto s = schedule_for(w, p, 1);   // a winter Monday, routine day
        o += "| " + r.name;
        for (int h : {6, 12, 18}) o += std::string(" | ") + s[static_cast<size_t>(h)].en;
        o += " |\n";
    }
    return o;
}

}  // namespace tribes
