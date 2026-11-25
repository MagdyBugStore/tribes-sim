// schedules_api.cpp: see schedules_api.hpp.
#include "schedules_api.hpp"

#include <chrono>
#include <cmath>
#include <cstring>

#include "activity.hpp"

namespace tribes {

namespace {
// 64 URL-safe-in-JSON characters: a slot index or activity index is one char.
const char* ALPHA = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string json_schedules(World& w, const ValleyMap& m, int day, long long* micros) {
    const auto t0 = std::chrono::steady_clock::now();
    std::string o;
    o.reserve(1 << 20);
    {
        std::lock_guard<std::mutex> g(w.lock);
        // Static tables first: activities (by first use order is unstable, so
        // list the whole table through the roles' schedules instead).
        std::vector<const char*> act_names, act_en, act_ar;
        std::vector<int> act_zone;
        auto act_index = [&](const Activity& a) -> int {
            for (size_t i = 0; i < act_names.size(); ++i)
                if (std::strcmp(act_names[i], a.name) == 0) return static_cast<int>(i);
            act_names.push_back(a.name);
            act_en.push_back(a.en);
            act_ar.push_back(a.ar);
            act_zone.push_back(static_cast<int>(a.zone));
            return static_cast<int>(act_names.size()) - 1;
        };
        std::vector<std::string> role_names;
        auto role_index = [&](const std::string& r) -> int {
            for (size_t i = 0; i < role_names.size(); ++i)
                if (role_names[i] == r) return static_cast<int>(i);
            role_names.push_back(r);
            return static_cast<int>(role_names.size()) - 1;
        };
        for (const auto& r : w.data.roles) role_index(r.name);

        std::string people;
        people.reserve(1 << 20);
        int count = 0;
        char b[64];
        for (const auto& p : w.people) {
            if (!p.alive) continue;
            const auto s = schedule_for(w, p, day);
            // distinct stops
            std::vector<Pt> stops;
            std::string seq, acts;
            for (int h = 0; h < 24; ++h) {
                const Activity& a = s[static_cast<size_t>(h)];
                Pt q = position_of(w, m, p, a.zone);
                q.x = std::round(q.x);
                q.y = std::round(q.y);
                size_t k = 0;
                while (k < stops.size() && !(stops[k].x == q.x && stops[k].y == q.y)) ++k;
                if (k == stops.size()) stops.push_back(q);
                if (k > 63) k = 63;   // cannot happen: at most 24 slots
                seq += ALPHA[k];
                acts += ALPHA[act_index(a) % 64];
            }
            if (count++) people += ",";
            std::snprintf(b, sizeof b, "[%d,%d,%d,%d,%d,[", p.id, p.tribe, p.gender == Gender::Male ? 0 : 1,
                          p.age(), role_index(p.role));
            people += b;
            for (size_t k = 0; k < stops.size(); ++k) {
                std::snprintf(b, sizeof b, "%s%d,%d", k ? "," : "", static_cast<int>(stops[k].x), static_cast<int>(stops[k].y));
                people += b;
            }
            people += "],\"" + seq + "\",\"" + acts + "\"]";
        }
        std::snprintf(b, sizeof b, "{\"day\":%d,\"hours\":24,\"acts\":[", day);
        o += b;
        for (size_t i = 0; i < act_names.size(); ++i) {
            if (i) o += ",";
            o += "[\"" + std::string(act_names[i]) + "\",\"" + act_en[i] + "\",\"" + act_ar[i] + "\",\"" +
                 zone_name(static_cast<Zone>(act_zone[i])) + "\"]";
        }
        o += "],\"roles\":[";
        for (size_t i = 0; i < role_names.size(); ++i) {
            const RoleDef* rd = w.data.role(role_names[i]);
            if (i) o += ",";
            o += "[\"" + role_names[i] + "\",\"" + (rd ? rd->sector : std::string("home")) + "\"]";
        }
        o += "],\"people\":[" + people + "]}";
    }
    if (micros)
        *micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
    return o;
}

}  // namespace tribes
