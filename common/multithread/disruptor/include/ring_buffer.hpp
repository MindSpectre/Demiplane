#pragma once

#include <array>
#include <bit>
#include <cstddef>

namespace demiplane::multithread::disruptor {

    /**
     * @brief Fixed-size ring buffer with power-of-2 sizing for O(1) indexing.
     *
     * ## Concept: Power-of-2 Sizing for Fast Modulo
     *
     * Traditional ring buffer indexing uses modulo:
     * ```
     * index = sequence % buffer_size;  // SLOW: Division instruction (~10-40 cycles)
     * ```
     *
     * If buffer_size is power of 2 (e.g., 1024 = 2^10), we can use bitwise AND:
     * ```
     * index = sequence & (buffer_size - 1);  // FAST: Single AND instruction (1 cycle)
     * ```
     *
     * Example with buffer_size = 8 (binary: 1000):
     * ```
     * buffer_size - 1 = 7 (binary: 0111)  <- This is our MASK
     *
     * sequence = 0:   0 & 0111 = 0
     * sequence = 1:   1 & 0111 = 1
     * sequence = 7:   0111 & 0111 = 7
     * sequence = 8:   1000 & 0111 = 0  (wraps around!)
     * sequence = 9:   1001 & 0111 = 1
     * sequence = 15:  1111 & 0111 = 7
     * sequence = 16: 10000 & 0111 = 0  (wraps around!)
     * ```
     *
     * The AND operation automatically keeps only the lower N bits,
     * giving us wrap-around behavior for free!
     *
     * ## Memory Layout
     *
     * Elements are stored contiguously in memory for cache efficiency:
     * ```
     * [Entry0][Entry1][Entry2]...[EntryN-1]
     *   ^-- Sequential access = cache prefetcher friendly
     * ```
     *
     * ## Thread Safety
     *
     * RingBuffer itself is NOT thread-safe. Thread safety is provided by:
     * - Sequence cursors (atomic ordering)
     * - MultiProducerSequencer (claim/publish protocol)
     *
     * The RingBuffer is just dumb storage - the smart coordination happens elsewhere.
     *
     * @tparam T Element type stored in the ring buffer
     * @tparam BufferSize Size of buffer (MUST be power of 2)
     */
    template <typename T, std::size_t BufferSize>
    class RingBuffer {
        static_assert(std::has_single_bit(BufferSize), "BufferSize must be a power of 2");
    public:
        /**
         * @brief Index mask for fast modulo operation
         *
         * Example: If BufferSize = 1024 (binary: 10000000000)
         *          Then MASK = 1023      (binary: 01111111111)
         *
         * Any sequence & MASK will give us valid index [0, BufferSize-1]
         */
        static constexpr std::size_t INDEX_MASK = BufferSize - 1;

        /**
         * @brief Default constructor - value-initializes all elements
         */
        RingBuffer() noexcept {
            buffer_.fill(T{});
        }

        /**
         * @brief Access element at sequence position
         * @param sequence Monotonic sequence number (can be > BufferSize)
         * @return Reference to element
         *
         * Example: BufferSize = 8
         * - get(0) -> buffer_[0]
         * - get(7) -> buffer_[7]
         * - get(8) -> buffer_[0]  (wraps)
         * - get(15) -> buffer_[7] (wraps)
         * - get(1000) -> buffer_[1000 & 7] = buffer_[0]
         */
        [[nodiscard]] T& get(const std::int64_t sequence) noexcept {
            return buffer_[static_cast<std::size_t>(sequence) & INDEX_MASK];
        }

        /**
         * @brief Access element at sequence position (const version)
         */
        [[nodiscard]] const T& get(const std::int64_t sequence) const noexcept {
            return buffer_[static_cast<std::size_t>(sequence) & INDEX_MASK];
        }

        /**
         * @brief Operator overload for cleaner syntax
         * @param sequence Sequence number
         * @return Reference to element
         *
         * Usage: ring_buffer[sequence] = value;
         */
        [[nodiscard]] T& operator[](const std::int64_t sequence) noexcept {
            return get(sequence);
        }

        /**
         * @brief Operator overload for cleaner syntax (const version)
         */
        [[nodiscard]] const T& operator[](const std::int64_t sequence) const noexcept {
            return get(sequence);
        }

        /**
         * @brief Get buffer capacity
         * @return Maximum number of elements
         */
        [[nodiscard]] static constexpr std::size_t capacity() noexcept {
            return BufferSize;
        }

        /**
         * @brief Check if a sequence position is valid (for debugging)
         * @param sequence Sequence number
         * @return Always true (all sequences map to valid indices)
         *
         * Note: This always returns true because masking ensures valid index.
         * Included for API completeness and potential future checks.
         */
        [[nodiscard]] static constexpr bool is_valid_sequence([[maybe_unused]] std::int64_t sequence) noexcept {
            return true;  // Masking guarantees valid index
        }

        /**
         * @brief Get underlying storage (for advanced use)
         * @return Reference to internal array
         *
         * WARNING: Direct access bypasses sequence-based indexing.
         * Only use if you know what you're doing!
         */
        [[nodiscard]] std::array<T, BufferSize>& storage() noexcept {
            return buffer_;
        }

        /**
         * @brief Get underlying storage (const version)
         */
        [[nodiscard]] const std::array<T, BufferSize>& storage() const noexcept {
            return buffer_;
        }

    private:
        /**
         * @brief Contiguous array of elements
         *
         * Uses std::array for:
         * - Stack allocation (if RingBuffer is on stack)
         * - Automatic lifetime management
         * - Cache-friendly contiguous memory
         * - No heap allocation overhead
         */
        std::array<T, BufferSize> buffer_;
    };

    // Compile-time verification examples
    static_assert(RingBuffer<int, 1024>::capacity() == 1024, "Capacity must match BufferSize");
    static_assert(RingBuffer<int, 1024>::INDEX_MASK == 1023, "Mask must be BufferSize - 1");

    // This will cause compile error (not power of 2):
    // RingBuffer<int, 1000> bad_buffer;  // Error: 1000 is not power of 2

}  // namespace demiplane::multithread::disruptor
