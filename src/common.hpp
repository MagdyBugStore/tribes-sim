#pragma once
// Common utilities shared across the simulation: randomness, clamping,
// formatting and small helpers. Everything here is stdlib only.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace tribes {

// The single source of randomness in the whole program (seeded once from
// --seed). Declared in main.cpp; every file draws from it so a seed fully
// determines the run.
extern std::mt19937_64 RNG;

inline double randd(double lo, double hi) {
    std::uniform_real_distribution<double> d(lo, hi);
    return d(RNG);
}

inline int randi(int lo, int hi) {  // inclusive
    std::uniform_int_distribution<int> d(lo, hi);
    return d(RNG);
}

inline bool chance(double p) {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(RNG) < p;
}

template <typename T>
inline const T& pick(const std::vector<T>& v) {
    std::uniform_int_distribution<size_t> d(0, v.size() - 1);
    return v[d(RNG)];
}

inline double clampd(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

// Round to fils (1/100 dirham) so money arithmetic stays exact-looking.
inline double fils(double v) { return std::round(v * 100.0) / 100.0; }

// Format a number with thousands separators: 1234567 -> "1,234,567".
std::string commas(long long v);
std::string commas(double v);

// Gini coefficient of a list of non-negative values (0 = perfect equality).
double gini_of(std::vector<double> values);

}  // namespace tribes
