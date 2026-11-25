#include "common.hpp"

namespace tribes {

std::string commas(long long v) {
    std::string s = std::to_string(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), ",");
    return (v < 0 ? "-" : "") + s;
}

std::string commas(double v) { return commas(static_cast<long long>(std::llround(v))); }

double gini_of(std::vector<double> values) {
    values.erase(std::remove_if(values.begin(), values.end(),
                                [](double v) { return v < 0; }),
                 values.end());
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    double sum = 0.0;
    for (double v : values) sum += v;
    if (sum <= 0.0) return 0.0;
    double acc = 0.0;
    const double n = static_cast<double>(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        acc += (static_cast<double>(i) + 1.0) * values[i];
    return (2.0 * acc) / (n * sum) - (n + 1.0) / n;
}

}  // namespace tribes
