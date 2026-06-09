#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace demiplane::http {

    /// RFC 3986 percent-decoding. With plus_is_space, '+' -> ' '
    /// (application/x-www-form-urlencoded — query strings and form bodies).
    /// PATH SEGMENTS must pass plus_is_space=false: '+' is a literal in paths
    /// (spec §8.6). Returns nullopt on a malformed escape (truncated or
    /// non-hex) so callers can surface a typed error.
    std::optional<std::string> url_decode(std::string_view in, bool plus_is_space = true);

}  // namespace demiplane::http
