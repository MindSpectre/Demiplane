#pragma once

namespace demiplane::fsm {
    class Transition {
    public:
        Transition(const uint32_t from, const uint32_t to) : from_(from), to_(to) {}
    private:
        uint32_t from_;
        uint32_t to_;
    };
} // namespace demiplane::fsm
