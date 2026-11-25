#pragma once
// People: one struct per soul in the valley, with needs, the family fabric,
// work, a pouch of dirhams and a household stock of physical goods.
// Bounded personal history so memory stays flat over long runs.

#include <deque>
#include <string>
#include <vector>

namespace tribes {

constexpr int COMING_OF_AGE = 16;   // takes a trade
constexpr int RETIREMENT_AGE = 60;  // rests, lives on family and sadaqah
constexpr int PREGNANCY_DAYS = 270;
// After a birth a mother nurses and does not conceive: lactational
// amenorrhea, up to about two years postpartum where nursing is frequent
// (see README, demographic sources). ~13 months here; with the pregnancy
// and the wait to the next conception this gives a child every ~2-2.5
// years, the fast end of the historical record (Hutterite spacing).
constexpr int NURSING_DAYS = 400;
// A mother dying in childbirth: 500-1000 per 100,000 births in the
// 19th-century record (Our World in Data, maternal mortality) -> ~0.7%.
constexpr double MATERNAL_MORTALITY = 0.007;
constexpr size_t HISTORY_MAX = 60;  // bounded per-person memory

enum class Gender { Male, Female };
enum class LifeStage { Child, Youth, Adult, Elder };

struct Person {
    int id = -1;
    std::string name;
    Gender gender = Gender::Male;
    int age_days = 0;
    int tribe = 0;
    bool alive = true;
    int death_day = -1;
    std::string death_cause;

    // Family fabric (person ids; -1 = none).
    int father = -1, mother = -1;
    int spouse = -1;
    std::vector<int> children;
    int widowed_since = -1;
    bool orphan = false;

    // Work.
    std::string role = "Child";
    int employer_shop = -1;   // Ajers/Tajirs attached to a shop
    bool retired = false;
    double skill = 1.0;       // 0.5 .. 1.5, grows with the years

    // Needs, 0..100. Water matters most in the desert.
    double hunger = 80, thirst = 80, energy = 80;
    double health = 90, happiness = 70;
    bool sick = false;
    int sick_days = 0;

    // Honor (sharaf): hospitality, generosity, poetry, honest trade, help.
    double honor = 10.0;

    // Money. The pouch is the person's own account (in the conservation
    // sum). The wadiah is a safekeeping claim at the Bayt al-Mal; the
    // dirhams themselves sit in the Bayt's account.
    double pouch = 0.0;
    double wadiah = 0.0;
    double qard = 0.0;        // interest-free loan owed to the Bayt al-Mal
    int qard_day = -1;

    // The household's physical stock (indexed by item). Food, firewood,
    // durables, livestock: everything the family owns is counted here.
    std::vector<double> stock;
    // The day the household last bought each durable item.
    std::vector<int> last_bought;

    // Pregnancy. -1 = not expecting; a negative value means the pregnancy
    // began before the founding (so day 0 - (-k) days along).
    int pregnant_since = -1;
    // Cannot conceive again until this day: she is nursing the baby.
    int nursing_until = -1;

    std::deque<std::string> history;

    int age() const { return age_days / 365; }
    double wealth() const { return pouch + wadiah; }
    LifeStage life_stage() const {
        const int a = age();
        if (a < 7) return LifeStage::Child;
        if (a < COMING_OF_AGE) return LifeStage::Youth;
        if (a < RETIREMENT_AGE) return LifeStage::Adult;
        return LifeStage::Elder;
    }
    const char* stage_name() const {
        switch (life_stage()) {
            case LifeStage::Child: return "child";
            case LifeStage::Youth: return "youth";
            case LifeStage::Adult: return "adult";
            case LifeStage::Elder: return "elder";
        }
        return "adult";
    }
    const char* gender_label() const {
        return gender == Gender::Male ? "male" : "female";
    }
    bool is_worker() const {
        // The old keep working their trade as their strength fades; only
        // those who stepped down from a salary are fully retired.
        return alive && age() >= COMING_OF_AGE && role != "Tilmidh" &&
               role != "Child" && role != "Elder";
    }
    void remember(const std::string& s) {
        history.push_back(s);
        while (history.size() > HISTORY_MAX) history.pop_front();
    }
};

}  // namespace tribes
