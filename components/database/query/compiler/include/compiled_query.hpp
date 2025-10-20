#pragma once
#include <memory>
#include <memory_resource>

#include <supported_providers.hpp>

namespace demiplane::db {
    class CompiledQuery {
    public:
        constexpr CompiledQuery(std::pmr::string sql,
                                std::shared_ptr<void> backend_packet,
                                const SupportedProviders provider,
                                std::shared_ptr<std::pmr::monotonic_buffer_resource> arena)
            : sql_{std::move(sql)},
              backend_packet_{std::move(backend_packet)},
              provider_(provider),
              arena_{std::move(arena)} {
        }
        [[nodiscard]] constexpr std::string_view sql() const& noexcept {
            return sql_;
        }
        [[nodiscard]] constexpr std::shared_ptr<void> backend_packet() const& noexcept {
            return backend_packet_;
        }

        /// @warning UB if wrong type
        template <typename T>
        [[nodiscard]] constexpr std::shared_ptr<T> backend_packet_as() const& noexcept {
            return std::static_pointer_cast<T>(backend_packet_);
        }

        [[nodiscard]] constexpr SupportedProviders provider() const {
            return provider_;
        }

        [[nodiscard]] constexpr std::shared_ptr<std::pmr::monotonic_buffer_resource> arena() const& noexcept {
            return arena_;
        }

    private:
        // TODO: add constexpr support
        std::pmr::string sql_;
        std::shared_ptr<void> backend_packet_;
        SupportedProviders provider_;
        std::shared_ptr<std::pmr::monotonic_buffer_resource> arena_;  // lifetime keeper
    };


}  // namespace demiplane::db
