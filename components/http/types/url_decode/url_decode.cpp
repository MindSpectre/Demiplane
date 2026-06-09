#include "url_decode.hpp"

namespace demiplane::http {

    std::optional<std::string> url_decode(const std::string_view in, const bool plus_is_space) {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i) {
            if (const char c = in[i]; c == '+' && plus_is_space) {
                out.push_back(' ');
            } else if (c == '%') {
                if (i + 2 >= in.size())
                    return std::nullopt;
                auto hex = [](const char x) -> int {
                    if (x >= '0' && x <= '9')
                        return x - '0';
                    if (x >= 'a' && x <= 'f')
                        return 10 + x - 'a';
                    if (x >= 'A' && x <= 'F')
                        return 10 + x - 'A';
                    return -1;
                };
                const int hi = hex(in[i + 1]);
                const int lo = hex(in[i + 2]);
                if (hi < 0 || lo < 0)
                    return std::nullopt;
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

}  // namespace demiplane::http
