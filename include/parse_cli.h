#ifndef PARSE_CLI_H
#define PARSE_CLI_H

#include "types.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace SHAYVERI::ParseCLI {

inline std::string trim(std::string value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char c) { return std::isspace(c); }).base();
    if (first >= last) return "";
    return std::string(first, last);
}

inline bool boolean(const std::string &value, bool &out) {
    std::string normalized = value;
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    if (normalized == "true" || normalized == "1"
        || normalized == "yes" || normalized == "on") {
        out = true;
        return true;
    }
    if (normalized == "false" || normalized == "0"
        || normalized == "no" || normalized == "off") {
        out = false;
        return true;
    }
    return false;
}

inline bool integer(const std::string &value, int &out) {
    const std::string normalized = trim(value);
    if (normalized.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(normalized.c_str(), &end, 10);
    if (end == normalized.c_str() || *end != '\0' || errno == ERANGE
        || parsed < std::numeric_limits<int>::min()
        || parsed > std::numeric_limits<int>::max())
        return false;
    out = static_cast<int>(parsed);
    return true;
}

inline bool unsigned_integer(const std::string &value, U64 &out) {
    const std::string normalized = trim(value);
    if (normalized.empty() || normalized.front() == '-') return false;
    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed =
        std::strtoull(normalized.c_str(), &end, 10);
    if (end == normalized.c_str() || *end != '\0' || errno == ERANGE)
        return false;
    out = static_cast<U64>(parsed);
    return true;
}

inline bool real(const std::string &value, double &out) {
    const std::string normalized = trim(value);
    if (normalized.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(normalized.c_str(), &end);
    if (end == normalized.c_str() || *end != '\0' || errno == ERANGE
        || !std::isfinite(parsed))
        return false;
    out = parsed;
    return true;
}

} // namespace SHAYVERI::ParseCLI

#endif // PARSE_CLI_H
