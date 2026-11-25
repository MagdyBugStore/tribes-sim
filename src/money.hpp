#pragma once
// The money core. Every dirham lives in exactly one account, and every
// payment goes through the single transfer() function. The day-end check
// sums every account and aborts if the total has drifted from the initial
// supply (plus anything the Mint openly coined, off by default).

#include <map>
#include <string>

namespace tribes {

struct World;

enum class AccKind { Person, Bayt, Majlis, Shop, Caravan, Mint };

struct Acc {
    AccKind kind;
    int id;  // person id, shop index, caravan company index; 0 for the rest
};

Acc person_acc(int pid);
Acc shop_acc(int shop);
Acc caravan_acc(int co);
Acc bayt_acc();
Acc majlis_acc();
Acc mint_acc();

std::string acc_name(const World& w, const Acc& a);

// THE one gateway. Returns false (moving nothing) when the payer cannot
// cover the amount. Writes the ledger and the personal history of people
// involved (unless log_history is false: daily bread and water purchases
// do not deserve a diary line). Never creates or destroys money.
bool transfer(World& w, Acc from, Acc to, double amount,
              const std::string& reason, bool log_history = true);

// Sum of every account in the world.
double total_money(const World& w);

struct Ledger {
    std::map<std::string, double> by_reason;
    void record(const std::string& reason, double amount) {
        by_reason[reason] += amount;
    }
};

}  // namespace tribes
