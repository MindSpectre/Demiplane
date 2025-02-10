#pragma once

#include <string>


namespace common::utilities::security {
    [[nodiscard]] inline bool constant_time_compare(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) {
            return false;
        }
        bool result = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            result &= a[i] == b[i];
        }
        return result;
    }
} // namespace common::utilities::security
