#pragma once
#include <cassert>
#include <memory>
#include <memory_resource>

#include <providers.hpp>

namespace demiplane::db {
    class CompiledDynamicQuery {
    public:
        constexpr CompiledDynamicQuery(std::pmr::string sql,
                                       std::shared_ptr<void> backend_packet,
                                       const Providers provider,
                                       std::shared_ptr<std::pmr::monotonic_buffer_resource> arena)
            : sql_{std::move(sql)},
              backend_packet_{std::move(backend_packet)},
              provider_{provider},
              arena_{std::move(arena)} {
        }

        [[nodiscard]] constexpr std::string_view sql() const& noexcept {
            return sql_;
        }

        [[nodiscard]] constexpr const char* c_sql() const& noexcept {
            return sql_.c_str();
        }

        [[nodiscard]] constexpr std::shared_ptr<void> backend_packet() const& noexcept {
            return backend_packet_;
        }

        /// @pre The caller must ensure T matches the type originally stored in the backend packet.
        /// @warning UB if wrong type
        template <typename T>
        [[nodiscard]] constexpr std::shared_ptr<T> backend_packet_as() const& noexcept {
            return std::static_pointer_cast<T>(backend_packet_);
        }

        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        [[nodiscard]] constexpr std::shared_ptr<std::pmr::monotonic_buffer_resource> arena() const& noexcept {
            return arena_;
        }

    private:
        std::pmr::string sql_;
        std::shared_ptr<void> backend_packet_;
        Providers provider_;
        std::shared_ptr<std::pmr::monotonic_buffer_resource> arena_;  // lifetime keeper
    };
}  // namespace demiplane::db
