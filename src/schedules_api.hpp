#pragma once
// /api/schedules: the viewer's view of one day. For every living person it
// carries the 24 hourly slots (activity) and where each slot happens (map
// position), packed compactly. Built once per day and cached by the server.

#include <string>

#include "map.hpp"
#include "world.hpp"

namespace tribes {

// Holds World::lock while it derives the schedules (read-only).
std::string json_schedules(World& w, const ValleyMap& m, int day, long long* micros = nullptr);

}  // namespace tribes
