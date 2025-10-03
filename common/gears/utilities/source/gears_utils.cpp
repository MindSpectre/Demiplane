#include "gears_utils.hpp"



#include <netinet/in.h>

namespace demiplane::gears {

    std::uint64_t ntoh64(const std::uint64_t net) {
        if constexpr (std::endian::native == std::endian::big) {
            // ReSharper disable once CppDFAUnreachableCode
            return net;
        } else {
            return (static_cast<uint64_t>(ntohl(net >> 32)) | (static_cast<uint64_t>(ntohl(net & 0xFFFFFFFF)) << 32));
        }
    }

    std::uint64_t hton64(std::uint64_t host) {
        return ntoh64(host);  // Same operation
    }
}  // namespace demiplane::gears
