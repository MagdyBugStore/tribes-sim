#pragma once
// Data: every tunable number of the simulation lives in plain-text CSV files
// under data/ and is loaded at startup (editable without recompiling):
//   data/resources.csv - the reference table of physical resources (sec 4d)
//   data/recipes.csv   - the production chains: stages, inputs, outputs (4c)
//   data/roles.csv     - the trades of the valley: wages and founding mix
// Nothing here is hard-coded in the engine.

#include <string>
#include <vector>

namespace tribes {

// One row of data/resources.csv.
struct ItemDef {
    std::string name;
    std::string unit;      // kg, liter, piece, head, meter, load...
    std::string kind;      // good | livestock | land
    bool local = false;    // can the valley produce it at all?
    int shelf_life = 0;    // days; 0 = keeps
    double base_price = 0, price_floor = 0, price_ceil = 0;  // multipliers
    double start_total = 0;              // for 1000 people; scaled by --people
    double dist_households = 0;          // shares of the starting total...
    double dist_shops = 0;
    double dist_institutions = 0;
    double dist_ground = 0;              // wells, groves, fields, salt pans
    double eat_adult_day = 0;            // consumption per adult per day
    double eat_child_day = 0;            // per child per day
    double use_household_month = 0;      // per household per month (durables)
    double satisfy = 0;                  // need restored per unit (food/drink)
    std::string need;                    // "hunger" | "thirst" | ""
    double import_per_caravan = 0;       // cargo per arriving caravan
    double export_per_caravan = 0;       // surplus bought per caravan
    double aux = 0;                      // land rows: well yield, palm kg/yr,
                                         // field yield kg/ha, or 0
    std::string reason;                  // the one-line justification (4d)
};

// One row of data/recipes.csv: one named stage of a production chain.
// process: craft | extract | gather | hunt | slaughter
// (milk, draw and farm are run by dedicated code, but still logged the same)
struct RecipeDef {
    std::string stage;        // milling, baking, tanning...
    std::string process;
    std::string role;         // which trade runs it
    std::vector<std::string> in_name;
    std::vector<double> in_qty;
    std::vector<std::string> out_name;
    std::vector<double> out_qty;
    double capacity = 0;      // max runs per worker per day
};

// One row of data/roles.csv.
struct RoleDef {
    std::string name;
    std::string sector;
    char payer = 'N';         // 'M' Majlis salary, 'B' Bayt salary, 'N' none
    double wage = 0;          // daily, when salaried
    bool male_only = false;
    bool female_only = false;
    double frac = 0;          // share of the working population at founding
};

struct Data {
    std::vector<ItemDef> items;
    std::vector<RecipeDef> recipes;
    std::vector<RoleDef> roles;

    int item_index(const std::string& name) const;
    const ItemDef* item(const std::string& name) const;
    const RoleDef* role(const std::string& name) const;
    // All recipes a role runs.
    std::vector<const RecipeDef*> recipes_of(const std::string& role) const;
};

// Load all three CSVs from dir (e.g. "data"). Aborts with a message on a
// malformed file: better to fail loudly at startup than simulate garbage.
Data load_data(const std::string& dir);

}  // namespace tribes
