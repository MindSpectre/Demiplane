#pragma once
#include <random>
#include <chrono>
#include <stdexcept>
namespace demiplane::utilities::chrono {
    class RandomTimeGenerator {
    public:
        RandomTimeGenerator() =default;

        /// @brief Generate a random time in range [ms * (100 - pc) / 100, ms * (100 + pc) / 100].
        /// @param point The base time in milliseconds.
        /// @param deviation The percentage variation allowed. Defualt value is 15%
        /// @return A random time in milliseconds.
        /// @throws std::invalid_argument if pc < 0 or ms < 0.
        static std::chrono::milliseconds generate(const int32_t point, const int32_t deviation = 15) {
            std::mt19937 gen(std::random_device{}());  // Mersenne Twister random number generator
            if (point < 0 || deviation < 0 || deviation > 100) {
                throw std::invalid_argument("Both ms and pc must be non-negative.");
            }

            // Calculate bounds
            const uint32_t lower_bound = point * (100.0 - deviation) / 100.0;
            const uint32_t upper_bound = point * (100.0 + deviation) / 100.0;
            if (lower_bound > upper_bound) {
                throw std::invalid_argument("Lower bound must be less than upper bound.");
            }
            // Generate random value in range
            std::uniform_int_distribution dist(lower_bound, upper_bound);
            return std::chrono::milliseconds(dist(gen));
        }

    };
}