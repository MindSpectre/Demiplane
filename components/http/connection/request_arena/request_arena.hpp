#pragma once

#include <cstddef>
#include <demiplane/gears>
#include <memory>
#include <memory_resource>

namespace demiplane::http {

    /**
     * @brief One heap block per CONNECTION, reused across every keep-alive
     *        request on it (spec §6.1).
     *
     * Allocated once at `size` bytes (ServerConfig::request_arena_size, default
     * 8 KB); reset() rewinds the monotonic resource to the initial block, so
     * the next request reuses the same memory — amortized, not per-request.
     * Requests that exceed the block grow via upstream new_delete blocks.
     *
     * Non-copyable AND non-movable: monotonic_buffer_resource is immovable, so
     * connections compose this by value and are constructed in place.
     */
    class RequestArena : gears::Immutable {
    public:
        explicit RequestArena(const std::size_t size = 8192)
            : initial_block_{std::make_unique<std::byte[]>(size)},
              resource_{initial_block_.get(), size} {
        }

        [[nodiscard]] std::pmr::polymorphic_allocator<> allocator() noexcept {
            return std::pmr::polymorphic_allocator{&resource_};
        }

        void reset() {
            resource_.release();  // rewinds to the initial block
        }

    private:
        std::unique_ptr<std::byte[]> initial_block_;
        std::pmr::monotonic_buffer_resource resource_;
    };

}  // namespace demiplane::http
