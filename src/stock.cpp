#include "stock.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "world.hpp"

namespace tribes {

Holder h_person(int pid) { return {HolderKind::Person, pid}; }
Holder h_shop(int shop) { return {HolderKind::Shop, shop}; }
Holder h_caravan(int co) { return {HolderKind::Caravan, co}; }
Holder h_majlis() { return {HolderKind::Majlis, 0}; }
Holder h_bayt() { return {HolderKind::Bayt, 0}; }
Holder h_well(int i) { return {HolderKind::Well, i}; }
Holder h_grove(int i) { return {HolderKind::Grove, i}; }
Holder h_field(int i) { return {HolderKind::Field, i}; }
Holder h_ground() { return {HolderKind::Ground, 0}; }

// Resolve a holder to the actual vector that stores its stock.
static std::vector<double>* bag_of(World& w, Holder h) {
    switch (h.kind) {
        case HolderKind::Person:
            return &w.people[static_cast<size_t>(h.id)].stock;
        case HolderKind::Shop:
            return &w.shops[static_cast<size_t>(h.id)].stock;
        case HolderKind::Caravan: {
            // A caravan in the valley sells from cargo; what it has bought
            // for the road home sits in the bought hold. Both are its stock.
            return &w.caravans[static_cast<size_t>(h.id)].cargo;
        }
        case HolderKind::Majlis: return &w.majlis.stock;
        case HolderKind::Bayt: return &w.bayt.stock;
        case HolderKind::Well: return &w.wells[static_cast<size_t>(h.id)].stock;
        case HolderKind::Grove:
            return &w.groves[static_cast<size_t>(h.id)].stock;
        case HolderKind::Field:
            return &w.fields[static_cast<size_t>(h.id)].stock;
        case HolderKind::Ground: return &w.ground_stock;
    }
    return nullptr;
}

static const std::vector<double>* bag_of(const World& w, Holder h) {
    return bag_of(const_cast<World&>(w), h);
}

std::string holder_name(const World& w, Holder h) {
    char buf[64];
    switch (h.kind) {
        case HolderKind::Person:
            return "person " + std::to_string(h.id);
        case HolderKind::Shop:
            snprintf(buf, sizeof buf, "shop %d", h.id);
            return buf;
        case HolderKind::Caravan:
            return "caravan " + w.caravans[static_cast<size_t>(h.id)].origin;
        case HolderKind::Majlis: return "the Majlis";
        case HolderKind::Bayt: return "the Bayt al-Mal";
        case HolderKind::Well:
            snprintf(buf, sizeof buf, "well %d", h.id);
            return buf;
        case HolderKind::Grove:
            snprintf(buf, sizeof buf, "grove %d", h.id);
            return buf;
        case HolderKind::Field:
            snprintf(buf, sizeof buf, "field %d", h.id);
            return buf;
        case HolderKind::Ground: return "the ground";
    }
    return "?";
}

double stock_of(const World& w, Holder h, int item) {
    const auto* bag = bag_of(w, h);
    return (*bag)[static_cast<size_t>(item)];
}

static void note_person(World& w, Holder h, int item, double qty,
                        const std::string& reason, bool gained) {
    if (h.kind != HolderKind::Person) return;
    Person& p = w.people[static_cast<size_t>(h.id)];
    if (!p.alive) return;
    char buf[160];
    snprintf(buf, sizeof buf, "Day %d: %s %.1f %s (%s)", w.tick,
             gained ? "received" : "gave", qty,
             w.data.items[static_cast<size_t>(item)].name.c_str(),
             reason.c_str());
    p.remember(buf);
}

bool move_stock(World& w, int item, double qty, Holder from, Holder to,
                const std::string& reason, bool log_history) {
    if (qty <= 0.0) return false;  // negative and zero moves are rejected
    auto* fb = bag_of(w, from);
    auto* tb = bag_of(w, to);
    double& f = (*fb)[static_cast<size_t>(item)];
    if (f < qty) {
        // Allow a rounding hair on an exact drain, never more.
        if (qty - f < 1e-6) {
            qty = f;
            if (qty <= 0.0) return false;
        } else {
            return false;  // the sender does not hold it: refused
        }
    }
    f -= qty;
    (*tb)[static_cast<size_t>(item)] += qty;
    if (log_history) {
        note_person(w, from, item, qty, reason, false);
        note_person(w, to, item, qty, reason, true);
    }
    return true;
}

bool stock_create(World& w, Holder h, int item, double qty,
                  const std::string& process) {
    if (qty <= 0.0) return false;
    auto* b = bag_of(w, h);
    (*b)[static_cast<size_t>(item)] += qty;
    StockBook& book = w.books[static_cast<size_t>(item)];
    if (process == "import") {
        book.imported += qty;
        book.imported_today += qty;
    } else {
        book.produced += qty;
        book.produced_today += qty;
    }
    book.created_by[process] += qty;
    w.stage_value[process + "|" +
                  w.data.items[static_cast<size_t>(item)].name] += qty;
    return true;
}

bool stock_destroy(World& w, Holder h, int item, double qty,
                   const std::string& process) {
    if (qty <= 0.0) return false;
    auto* b = bag_of(w, h);
    double& v = (*b)[static_cast<size_t>(item)];
    if (v < qty) {
        if (qty - v < 1e-6) {
            qty = v;
            if (qty <= 0.0) return false;
        } else {
            return false;  // cannot destroy what is not held
        }
    }
    v -= qty;
    StockBook& book = w.books[static_cast<size_t>(item)];
    book.destroyed_by[process] += qty;
    if (process == "export") {
        book.exported += qty;
        book.exported_today += qty;
    } else if (process == "spoilage") {
        book.spoiled += qty;
    } else if (process == "consumption") {
        book.consumed += qty;
        book.consumed_today += qty;
    } else {  // recipe inputs, seed, slaughter: "used"
        book.used += qty;
    }
    return true;
}

double total_holdings(const World& w, int item) {
    double sum = 0;
    const size_t i = static_cast<size_t>(item);
    for (const auto& p : w.people) sum += p.stock[i];
    for (const auto& s : w.shops) sum += s.stock[i];
    for (const auto& c : w.caravans) sum += c.cargo[i] + c.bought[i];
    sum += w.majlis.stock[i] + w.bayt.stock[i] + w.ground_stock[i];
    for (const auto& wl : w.wells) sum += wl.stock[i];
    for (const auto& g : w.groves) sum += g.stock[i];
    for (const auto& f : w.fields) sum += f.stock[i];
    return sum;
}

void assert_stock_books(const World& w) {
    for (size_t i = 0; i < w.books.size(); ++i) {
        double holdings = total_holdings(w, static_cast<int>(i));
        double expected = w.books[i].expected();
        double tol = std::max(0.01, std::fabs(expected) * 1e-6);
        if (std::fabs(holdings - expected) > tol) {
            std::cerr << "\nSTOCK CHECK FAILED day " << w.tick << " item "
                      << w.data.items[i].name << ": holdings "
                      << std::to_string(holdings) << " vs expected "
                      << std::to_string(expected) << " (initial "
                      << w.books[i].initial << " + imported "
                      << w.books[i].imported << " + produced "
                      << w.books[i].produced << " - exported "
                      << w.books[i].exported << " - consumed "
                      << w.books[i].consumed << " - spoiled "
                      << w.books[i].spoiled << " - used " << w.books[i].used
                      << ")\n";
            std::abort();
        }
    }
}

}  // namespace tribes
