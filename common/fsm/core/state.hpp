#pragma once
#include <cstdint>
#include <functional>

#include <gears_class_traits.hpp>
#include <generators/number_generator.hpp>

namespace demiplane::fsm {
    class State : gears::NonCopyable {
        public:
        // Constructor
        explicit State(const uint32_t id)
            : id_(id) {
        }
        State() {
            const math::random::NumberGenerator num_gen;
            id_ = num_gen.generate_random_uint32(0, std::numeric_limits<uint32_t>::max());
        }
        // destructor for proper inheritance
        ~State() = default;

        // Get state ID
        [[nodiscard]] uint32_t get_id() const {
            return id_;
        }
        std::function<void()> on_enter;
        std::function<void()> on_exit;


        protected:
        std::string name_;
        uint32_t id_;
    };
}  // namespace demiplane::fsm
