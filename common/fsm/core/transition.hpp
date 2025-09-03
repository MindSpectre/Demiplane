#pragma once
#include <cstdint>

namespace demiplane::fsm {
    class Transition {
    public:
        Transition(const uint32_t from, const uint32_t to)
            : from_(from),
              to_(to) {
        }
        [[nodiscard]] uint32_t get_from() const {
            return from_;
        }
        void set_from(const uint32_t from) {
            from_ = from;
        }
        [[nodiscard]] uint32_t get_to() const {
            return to_;
        }
        void set_to(const uint32_t to) {
            to_ = to;
        }

    private:
        uint32_t from_;
        uint32_t to_;
    };
}  // namespace demiplane::fsm
