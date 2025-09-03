#pragma once
#include <cstdint>
#include <string>

namespace demiplane::http {
    struct rate_limit {
        std::uint32_t max_in_flight = 0;  // 0 → unlimited
        std::uint32_t req_per_sec   = 0;  // averaged, 0 → unlimited
        std::uint32_t burst         = 0;  // extra tokens beyond steady rate
    };

    struct ip_rule {
        std::string cidr;   // "192.168.0.0/24" or "::1/128"
        rate_limit limits;  // 0/0/0 means block? (TBD)
    };
}  // namespace demiplane::http
