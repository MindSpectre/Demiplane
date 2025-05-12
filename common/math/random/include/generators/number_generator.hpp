#pragma once
#include "../base_random_generator.hpp"
namespace demiplane::math::random {
    class NumberGenerator final : BaseRandomGenerator {
    public:
        NumberGenerator() = default;
        explicit NumberGenerator(const std::mt19937& r_generator) : BaseRandomGenerator(r_generator) {}
        [[nodiscard]] uint32_t generate_random_uint32(uint32_t min, uint32_t max) const;

        template <typename T>
        T generate_random_t() const
            requires std::is_integral_v<T>
        {
            std::uniform_int_distribution<T> distrib(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
            return distrib(generator_);
        }
    };

} // namespace demiplane::math::random
