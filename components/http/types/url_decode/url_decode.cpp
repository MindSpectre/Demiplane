#include "url_decode.hpp"

namespace demiplane::http {

    namespace {
        constexpr int hex_val(const char x) noexcept {
            if (x >= '0' && x <= '9')
                return x - '0';
            if (x >= 'a' && x <= 'f')
                return 10 + x - 'a';
            if (x >= 'A' && x <= 'F')
                return 10 + x - 'A';
            return -1;
        }

        /// Decodes `in` into `out`, which must hold >= in.size() chars (the
        /// decoded form never grows). Returns the decoded length, or nullopt
        /// on a truncated/non-hex escape.
        std::optional<std::size_t>
        decode_into(const std::string_view in, const bool plus_is_space, char* out) noexcept {
            std::size_t n = 0;
            for (std::size_t i = 0; i < in.size(); ++i) {
                if (const char c = in[i]; c == '+' && plus_is_space) {
                    out[n++] = ' ';
                } else if (c == '%') {
                    if (i + 2 >= in.size())
                        return std::nullopt;
                    const int hi = hex_val(in[i + 1]);
                    const int lo = hex_val(in[i + 2]);
                    if (hi < 0 || lo < 0)
                        return std::nullopt;
                    out[n++]  = static_cast<char>(hi << 4 | lo);
                    i        += 2;
                } else {
                    out[n++] = c;
                }
            }
            return n;
        }
    }  // namespace

    std::optional<std::string> url_decode(const std::string_view in, const bool plus_is_space) {
        std::string out;
        out.resize(in.size());
        const auto n = decode_into(in, plus_is_space, out.data());
        if (!n)
            return std::nullopt;
        out.resize(*n);
        return out;
    }

    std::optional<std::string_view>
    url_decode_arena(const std::string_view in, const bool plus_is_space, std::pmr::polymorphic_allocator<> alloc) {
        const bool needs_rewrite =
            in.find('%') != std::string_view::npos || (plus_is_space && in.find('+') != std::string_view::npos);
        if (!needs_rewrite)
            return in;  // zero-copy
        const auto buf = static_cast<char*>(alloc.allocate_bytes(in.size(), 1));
        const auto n   = decode_into(in, plus_is_space, buf);
        if (!n)
            return std::nullopt;  // arena bytes wasted; malformed escapes are rare on validated segments
        return std::string_view{buf, *n};
    }

}  // namespace demiplane::http
