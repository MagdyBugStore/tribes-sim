#pragma once
// Events: one bounded world log (weddings, births, deaths, caravans, eids,
// wells) so long runs keep flat memory, plus helpers to log and read back.

#include <deque>
#include <string>

namespace tribes {

constexpr size_t WORLD_LOG_MAX = 400;

struct EventLog {
    std::deque<std::string> entries;
    size_t total = 0;       // entries ever added (a stable sequence number for readers)
    void add(const std::string& s) {
        entries.push_back(s);
        ++total;
        while (entries.size() > WORLD_LOG_MAX) entries.pop_front();
    }
};

}  // namespace tribes
