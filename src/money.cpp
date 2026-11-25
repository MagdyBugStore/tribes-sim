#include "money.hpp"

#include <cstdio>

#include "common.hpp"
#include "world.hpp"

namespace tribes {

Acc person_acc(int pid) { return {AccKind::Person, pid}; }
Acc shop_acc(int shop) { return {AccKind::Shop, shop}; }
Acc caravan_acc(int co) { return {AccKind::Caravan, co}; }
Acc bayt_acc() { return {AccKind::Bayt, 0}; }
Acc majlis_acc() { return {AccKind::Majlis, 0}; }
Acc mint_acc() { return {AccKind::Mint, 0}; }

// Resolve an account reference to the actual balance.
static double* balance_of(World& w, const Acc& a) {
    switch (a.kind) {
        case AccKind::Person:
            return &w.people[static_cast<size_t>(a.id)].pouch;
        case AccKind::Bayt: return &w.bayt.balance;
        case AccKind::Majlis: return &w.majlis.balance;
        case AccKind::Shop: return &w.shops[static_cast<size_t>(a.id)].till;
        case AccKind::Caravan:
            return &w.caravans[static_cast<size_t>(a.id)].purse;
        case AccKind::Mint: return &w.mint.balance;
    }
    return nullptr;
}

static const double* balance_of(const World& w, const Acc& a) {
    return balance_of(const_cast<World&>(w), a);
}

std::string acc_name(const World& w, const Acc& a) {
    char buf[64];
    switch (a.kind) {
        case AccKind::Person:
            return "person " + std::to_string(a.id);
        case AccKind::Bayt: return "the Bayt al-Mal";
        case AccKind::Majlis: return "the Majlis";
        case AccKind::Shop:
            snprintf(buf, sizeof buf, "shop %d", a.id);
            return buf;
        case AccKind::Caravan:
            return "caravan " + w.caravans[static_cast<size_t>(a.id)].origin;
        case AccKind::Mint: return "the Mint";
    }
    return "?";
}

static void note(World& w, const Acc& a, double amount, const Acc& other,
                 const std::string& reason, bool received) {
    if (a.kind != AccKind::Person) return;
    Person& p = w.people[static_cast<size_t>(a.id)];
    if (!p.alive) return;
    char buf[192];
    snprintf(buf, sizeof buf, "Day %d: %s %.2f %s %s (%s)", w.tick,
             received ? "received" : "paid", amount,
             received ? "from" : "to", acc_name(w, other).c_str(),
             reason.c_str());
    p.remember(buf);
}

bool transfer(World& w, Acc from, Acc to, double amount,
              const std::string& reason, bool log_history) {
    amount = fils(amount);
    if (amount <= 0.0) return false;
    double* fb = balance_of(w, from);
    double* tb = balance_of(w, to);
    if (*fb < amount) {
        // An exact drain must not fail on a rounding hair (the fara'id pays
        // out the precise remainder of an estate).
        if (amount - *fb < 0.005) {
            amount = *fb;
            if (amount <= 0.0) return false;
        } else {
            return false;  // the payer cannot afford it: refused
        }
    }
    *fb -= amount;
    *tb += amount;
    w.money_flows[reason] += amount;
    if (log_history) {
        note(w, from, amount, to, reason, false);
        note(w, to, amount, from, reason, true);
    }
    return true;
}

double total_money(const World& w) {
    double sum = 0;
    for (const auto& p : w.people) sum += p.pouch;
    for (const auto& s : w.shops) sum += s.till;
    for (const auto& c : w.caravans) sum += c.purse;
    sum += w.bayt.balance + w.majlis.balance + w.mint.balance;
    return sum;
}

}  // namespace tribes
