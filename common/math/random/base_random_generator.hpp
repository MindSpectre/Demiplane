#pragma once

#include <random>

namespace demiplane::math::random {
    class BaseRandomGenerator {
    public:
        BaseRandomGenerator()
            : generator_(std::random_device{}()) {
        }
        explicit BaseRandomGenerator(const std::mt19937& r_generator)
            : generator_(r_generator) {
        }
        virtual ~BaseRandomGenerator() = default;

    protected:
        mutable std::mt19937 generator_;
    };
}  // namespace demiplane::math::random
