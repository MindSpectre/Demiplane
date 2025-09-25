#pragma once
#include <memory>
#include <memory_resource>

namespace demiplane::db {
    class CompiledQuery {
    public:
        CompiledQuery(std::pmr::string sql,
                      std::shared_ptr<void> backend_packet,
                      std::shared_ptr<std::pmr::monotonic_buffer_resource> arena)
            : sql_{std::move(sql)},
              backend_packet_{std::move(backend_packet)},
              arena_{std::move(arena)} {
        }
        [[nodiscard]] std::string_view sql() const & noexcept {
            return sql_;
        }
        [[nodiscard]] std::shared_ptr<void> backend_packet() & noexcept {
            return backend_packet_;
        }
        [[nodiscard]] std::shared_ptr<std::pmr::monotonic_buffer_resource> arena() & noexcept {
            return arena_;
        }
    private:
        //TODO: add constexpr support
        std::pmr::string sql_;
        std::shared_ptr<void> backend_packet_;
        std::shared_ptr<std::pmr::monotonic_buffer_resource> arena_;  // lifetime keeper
    };


}  // namespace demiplane::db
