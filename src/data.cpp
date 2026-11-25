#include "data.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace tribes {

// Split one CSV line. No quoting: the data files use ';' inside text fields
// and never a bare comma.
static std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    for (auto& s : out) {
        size_t a = s.find_first_not_of(" \t\r");
        size_t b = s.find_last_not_of(" \t\r");
        s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
    }
    return out;
}

static double num(const std::string& s, const std::string& what, int line) {
    if (s.empty()) return 0.0;
    try {
        return std::stod(s);
    } catch (...) {
        throw std::runtime_error("bad number '" + s + "' for " + what +
                                 " at line " + std::to_string(line));
    }
}

static std::vector<std::vector<std::string>> read_csv(const std::string& path,
                                                      size_t min_cols) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<std::vector<std::string>> rows;
    std::string line;
    bool header_skipped = false;
    int ln = 0;
    while (std::getline(f, line)) {
        ++ln;
        if (line.empty() || line[0] == '#') continue;
        if (!header_skipped) {  // the column-header row
            header_skipped = true;
            continue;
        }
        auto cols = split_csv(line);
        if (cols.size() < min_cols)
            throw std::runtime_error(path + " line " + std::to_string(ln) +
                                     ": too few columns");
        rows.push_back(cols);
    }
    return rows;
}

Data load_data(const std::string& dir) {
    Data d;

    // --- resources.csv -------------------------------------------------------
    for (auto& c : read_csv(dir + "/resources.csv", 20)) {
        ItemDef it;
        int i = 0;
        it.name = c[i++];
        it.unit = c[i++];
        it.kind = c[i++];
        it.local = c[i++] == "1";
        it.shelf_life = static_cast<int>(num(c[i++], "shelf", 0));
        it.base_price = num(c[i++], "price", 0);
        it.price_floor = num(c[i++], "floor", 0);
        it.price_ceil = num(c[i++], "ceil", 0);
        it.start_total = num(c[i++], "start", 0);
        it.dist_households = num(c[i++], "dh", 0);
        it.dist_shops = num(c[i++], "ds", 0);
        it.dist_institutions = num(c[i++], "di", 0);
        it.dist_ground = num(c[i++], "dg", 0);
        it.eat_adult_day = num(c[i++], "adult", 0);
        it.eat_child_day = num(c[i++], "child", 0);
        it.use_household_month = num(c[i++], "hh", 0);
        it.satisfy = num(c[i++], "satisfy", 0);
        it.need = c[i++];
        it.import_per_caravan = num(c[i++], "import", 0);
        it.export_per_caravan = num(c[i++], "export", 0);
        it.aux = num(c[i++], "aux", 0);
        it.reason = c[i++];
        d.items.push_back(it);
    }

    // --- recipes.csv -----------------------------------------------------------
    // stage,process,role,in1,qty1,...,in4,qty4,out1,qty1,out2,qty2,capacity
    for (auto& c : read_csv(dir + "/recipes.csv", 16)) {
        RecipeDef r;
        int i = 0;
        r.stage = c[i++];
        r.process = c[i++];
        r.role = c[i++];
        for (int k = 0; k < 4; ++k) {
            std::string nm = c[i++];
            double q = num(c[i++], "inqty", 0);
            if (!nm.empty() && q > 0) {
                r.in_name.push_back(nm);
                r.in_qty.push_back(q);
            }
        }
        for (int k = 0; k < 2; ++k) {
            std::string nm = c[i++];
            double q = num(c[i++], "outqty", 0);
            if (!nm.empty() && q > 0) {
                r.out_name.push_back(nm);
                r.out_qty.push_back(q);
            }
        }
        r.capacity = num(c[i++], "capacity", 0);
        d.recipes.push_back(r);
    }

    // --- roles.csv -----------------------------------------------------------------
    // name,sector,payer,wage,male_only,female_only,frac
    for (auto& c : read_csv(dir + "/roles.csv", 7)) {
        RoleDef r;
        int i = 0;
        r.name = c[i++];
        r.sector = c[i++];
        std::string p = c[i++];
        r.payer = p.empty() ? 'N' : p[0];
        r.wage = num(c[i++], "wage", 0);
        r.male_only = c[i++] == "1";
        r.female_only = c[i++] == "1";
        r.frac = num(c[i++], "frac", 0);
        d.roles.push_back(r);
    }

    return d;
}

int Data::item_index(const std::string& name) const {
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].name == name) return static_cast<int>(i);
    return -1;
}

const ItemDef* Data::item(const std::string& name) const {
    int i = item_index(name);
    return i < 0 ? nullptr : &items[static_cast<size_t>(i)];
}

const RoleDef* Data::role(const std::string& name) const {
    for (auto& r : roles)
        if (r.name == name) return &r;
    return nullptr;
}

std::vector<const RecipeDef*> Data::recipes_of(const std::string& role) const {
    std::vector<const RecipeDef*> out;
    for (auto& r : recipes)
        if (r.role == role) out.push_back(&r);
    return out;
}

}  // namespace tribes
