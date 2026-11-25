#pragma once
// Stock accounting (section 4b). Every physical thing is a counted stock,
// owned by exactly one holder at any time. Stocks change in exactly two
// ways:
//   move_stock()    - the ONLY way to change an owner (like transfer() for
//                     money). Rejects negative or unavailable quantities.
//   stock_create()  - a NAMED process adds units (harvest, milking, a
//                     crafted output, a birth, an import, a well recharge).
//   stock_destroy() - a NAMED process removes units (consumption, spoilage,
//                     a recipe input, slaughter, an export, a death).
// Each item keeps a ledger of these flows, and the day-end check verifies
//   initial + imported + produced - exported - consumed - spoiled - used
//   == sum of every holding
// for every item, and aborts with the item and the numbers on any drift.

#include <string>
#include <vector>
#include <map>

namespace tribes {

struct World;

enum class HolderKind : int {
    Person = 0, Shop, Caravan, Majlis, Bayt, Well, Grove, Field, Ground
};

struct Holder {
    HolderKind kind;
    int id;  // person id, shop index, caravan company index, well/grove/field
};

Holder h_person(int pid);
Holder h_shop(int shop);
Holder h_caravan(int co);
Holder h_majlis();
Holder h_bayt();
Holder h_well(int i);
Holder h_grove(int i);
Holder h_field(int i);
Holder h_ground();

std::string holder_name(const World& w, Holder h);

// Move `qty` of `item` between holders. Returns false and moves nothing
// when the sender does not hold enough. `log_history` controls whether the
// move is written to a person's own history (routine food shopping is not
// worth a diary line; a mahr, a gift or an inheritance is).
bool move_stock(World& w, int item, double qty, Holder from, Holder to,
                const std::string& reason, bool log_history = true);

// Named creation / destruction processes.
bool stock_create(World& w, Holder h, int item, double qty,
                  const std::string& process);
bool stock_destroy(World& w, Holder h, int item, double qty,
                   const std::string& process);

// How much of `item` does this holder own?
double stock_of(const World& w, Holder h, int item);

// Per-item flow ledger for the reconciliation check and the reports.
struct StockBook {
    double initial = 0, imported = 0, produced = 0, exported = 0;
    double consumed = 0, spoiled = 0, used = 0;
    // Which named processes created / destroyed units (audit trail; the
    // tests use it to prove e.g. that herds grow only through breeding).
    std::map<std::string, double> created_by;
    std::map<std::string, double> destroyed_by;
    // Today's flows (reset each tick; feed the `chain` command's
    // throughput numbers and the economy report).
    double produced_today = 0, consumed_today = 0, imported_today = 0;
    double exported_today = 0;
    double expected() const {
        return initial + imported + produced - exported - consumed - spoiled -
               used;
    }
};

// Sum of one item over every holder in the world.
double total_holdings(const World& w, int item);

// The day-end stock reconciliation (4b). Aborts with details on failure.
void assert_stock_books(const World& w);

}  // namespace tribes
