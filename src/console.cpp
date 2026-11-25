// console.cpp: the interactive console (live mode) and the simulation
// thread. Commands: person, family, status, history, world, top, economy,
// tribes, market, trade, chain, pause, resume, speed, quit.
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "world.hpp"

namespace tribes {

static const char* HELP =
    "Commands:\n"
    "  person <id>       who is this person (gender, age, role, spouse, "
    "family)\n"
    "  family <id>       the household tree: spouse, parents, siblings, "
    "children\n"
    "  status <id>       current state (pouch, wadiah, qard, needs, honor, "
    "goods)\n"
    "  history <id> [n]  last n events for this person (default 10)\n"
    "  world             live valley summary (dirham supply asserted "
    "constant)\n"
    "  top [n]           richest people (default 10)\n"
    "  economy           the stock table: item, total, per owner, price, "
    "trend\n"
    "  tribes            the tribes: people, wealth, honor, alliances\n"
    "  market            stock, price and price change per good\n"
    "  trade             caravans (past and next), trade balance, top goods\n"
    "  chain <good>      the production chain of a good, stage by stage\n"
    "  pause / resume    pause or resume the simulation\n"
    "  speed <sec>       set tick interval in seconds\n"
    "  quit              stop and print the end report";

void run_console(World& w, int max_days) {
    std::thread sim([&]() {
        while (!w.stop.load()) {
            if (!w.paused.load()) {
                {
                    std::lock_guard<std::mutex> g(w.lock);
                    if (max_days > 0 && w.tick >= max_days) {
                        w.stop.store(true);
                        break;
                    }
                    w.step();
                    std::cout << w.daily_summary() << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int>(w.tick_interval * 1000)));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    std::cout << HELP << std::endl;
    std::string line;
    while (!w.stop.load()) {
        std::cout << "\n> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) continue;
        std::lock_guard<std::mutex> g(w.lock);
        try {
            if (cmd == "person") {
                int id;
                in >> id;
                std::cout << w.describe_person(id) << "\n";
            } else if (cmd == "family") {
                int id;
                in >> id;
                std::cout << w.describe_family(id) << "\n";
            } else if (cmd == "status") {
                int id;
                in >> id;
                std::cout << w.describe_status(id) << "\n";
            } else if (cmd == "history") {
                int id, n = 10;
                in >> id;
                if (in >> n) {}
                std::cout << w.describe_history(id, n) << "\n";
            } else if (cmd == "world") {
                std::cout << w.daily_summary() << "\n";
            } else if (cmd == "top") {
                int n = 10;
                if (in >> n) {}
                std::cout << w.describe_top(n) << "\n";
            } else if (cmd == "economy") {
                std::cout << w.describe_economy() << "\n";
            } else if (cmd == "tribes") {
                std::cout << w.describe_tribes() << "\n";
            } else if (cmd == "market") {
                std::cout << w.describe_market() << "\n";
            } else if (cmd == "trade") {
                std::cout << w.describe_trade() << "\n";
            } else if (cmd == "chain") {
                std::string good;
                in >> good;
                std::cout << w.describe_chain(good) << "\n";
            } else if (cmd == "pause") {
                w.paused.store(true);
                std::cout << "Paused.\n";
            } else if (cmd == "resume") {
                w.paused.store(false);
                std::cout << "Resumed.\n";
            } else if (cmd == "speed") {
                double s;
                if (in >> s) {
                    w.tick_interval = std::max(0.01, s);
                    std::cout << "Tick interval: " << w.tick_interval << "s\n";
                }
            } else if (cmd == "quit") {
                break;
            } else if (cmd == "help") {
                std::cout << HELP << "\n";
            } else {
                std::cout << "Unknown command. Type 'help'.\n";
            }
        } catch (...) {
            std::cout << "Bad argument. Type 'help'.\n";
        }
    }
    w.stop.store(true);
    sim.join();
}

}  // namespace tribes
