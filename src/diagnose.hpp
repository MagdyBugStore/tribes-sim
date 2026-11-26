// diagnose.hpp: the realism audit report (issue #2). Headless, read-only.
#pragma once
#include <string>
#include "world.hpp"
namespace tribes {
// Runs `days` steps on w and returns a plain-text report.
std::string diagnose_run(World& w, int days);
}
