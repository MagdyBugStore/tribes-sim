// console.cpp: the interactive console (live mode) and the simulation
// thread. Commands: person, family, status, history, world, top, economy,
// tribes, market, trade, chain, pause, resume, speed, quit.
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "activity.hpp"
#include "http_server.hpp"
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
    "  schedule <id> [day]  one person's day, 24 hourly lines (zone, activity)\n"
    "  activity [hour]   people per activity at an hour (default 12)\n"
    "  pause / resume    pause or resume the simulation\n"
    "  speed <sec>       set tick interval in seconds\n"
    "  quit              stop and print the end report";

void run_console(World& w, int max_days, int serve_port,
                 const std::string& web_root) {
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
                    w.last_step_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                    std::cout << w.daily_summary() << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    static_cast<int>(w.tick_interval * 1000)));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    HttpServer server(w, serve_port, web_root);
    if (serve_port > 0) {
        std::string err;
        if (!server.start(&err)) {
            std::cerr << "cannot serve on 127.0.0.1:" << serve_port << ": "
                      << err << "\n";
            w.stop.store(true);
            sim.join();
            return;
        }
        std::cout << "Serving the live valley on http://127.0.0.1:" << serve_port
                  << "/ (static files from " << web_root << "/)" << std::endl;
    }
    std::cout << HELP << std::endl;
    std::string line;
    while (!w.stop.load()) {
        std::cout << "\n> " << std::flush;
        if (!std::getline(std::cin, line)) {
            // No console (stdin closed): when serving, keep running until
            // the simulation ends; otherwise quit as before.
            if (serve_port > 0)
                while (!w.stop.load())
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
            break;
        }
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
            } else if (cmd == "schedule") {
                int id, day = w.tick;
                in >> id;
                if (in >> day) {}
                std::cout << describe_schedule(w, id, day) << "\n";
            } else if (cmd == "activity") {
                int hour = 12;
                if (in >> hour) {}
                std::cout << describe_activity_counts(w, hour) << "\n";
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
    server.stop();
    sim.join();
}

}  // namespace tribes
