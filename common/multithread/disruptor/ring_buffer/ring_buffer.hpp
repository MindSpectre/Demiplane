#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace demiplane::multithread {

    /**
     * @brief Runtime-sized ring buffer with power-of-2 sizing for O(1) indexing.
     *
     * The single heap allocation happens once at construction, so the runtime sizing
     * costs nothing on the hot path. Power-of-2 sizing lets us index with a bitwise
     * AND instead of a modulo:
     * ```
     * index = sequence & (buffer_size - 1);   // single AND, 1 cycle
     * ```
     *
     * Power-of-2 (and non-zero) sizing is a precondition, mirroring the compile-time
     * `static_assert` in StaticRingBuffer. It is checked with `assert` so the
     * allocating constructor stays `noexcept` (a wrong size is a programming error,
     * not a runtime condition).
     *
     * Thread safety: the buffer is dumb storage. Ordering/coordination is provided by
     * the Sequence cursors and the MultiProducerSequencer claim/publish protocol.
     *
     * @tparam T Element type stored in the ring buffer
     */
    template <typename T>
    class RingBuffer {
    public:
        /// @brief Allocate `size` (power-of-2) value-initialized elements, once.
        constexpr explicit RingBuffer(const std::size_t size) noexcept
            : buffer_size_{size},
              index_mask_{size - 1} {
            assert(size != 0 && std::has_single_bit(size) && "RingBuffer size must be a non-zero power of 2");
            buffer_.assign(size, T{});
        }

        /// @brief Access element at sequence position (wraps via power-of-2 mask).
        [[nodiscard]] T& get(const std::int64_t sequence) noexcept {
            return buffer_[static_cast<std::size_t>(sequence) & index_mask_];
        }
        [[nodiscard]] const T& get(const std::int64_t sequence) const noexcept {
            return buffer_[static_cast<std::size_t>(sequence) & index_mask_];
        }

        [[nodiscard]] T& operator[](const std::int64_t sequence) noexcept {
            return get(sequence);
        }
        [[nodiscard]] const T& operator[](const std::int64_t sequence) const noexcept {
            return get(sequence);
        }

        /// @brief Maximum number of elements.
        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return buffer_size_;
        }

        /// @brief Direct storage access (bypasses sequence indexing — advanced use).
        [[nodiscard]] std::vector<T>& storage() noexcept {
            return buffer_;
        }
        [[nodiscard]] const std::vector<T>& storage() const noexcept {
            return buffer_;
        }

    private:
        std::size_t buffer_size_;
        std::size_t index_mask_;  // buffer_size_ - 1; valid because size is a power of 2
        std::vector<T> buffer_;
    };

}  // namespace demiplane::multithread
