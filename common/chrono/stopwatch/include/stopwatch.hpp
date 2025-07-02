#pragma once

#include <chrono>
#include <utility>
#include <vector>

namespace demiplane::chrono {

    template <typename duration = std::chrono::milliseconds, typename clock = std::chrono::high_resolution_clock>
    class Stopwatch {
    public:
        using time_point = typename clock::time_point;

        explicit Stopwatch(std::size_t reserve_flags = 20) {
            flags_.reserve(reserve_flags);
        }

        void start() noexcept {
            flags_.clear();
            add_flag();
        }

        std::vector<time_point> stop() noexcept {
            flags_.emplace_back(clock::now());
            std::vector<time_point> flags = std::move(flags_);
            flags_.clear();
            return flags;
        }

        void add_flag() {
            auto now_time = clock::now();
            flags_.emplace_back(now_time);
        }

        time_point operator[](const std::size_t i) const {
            return flags_[i];
        }

        std::pair<duration, duration> delta_t(const std::size_t i) const {
            if (i >= flags_.size() || i == 0) {
                return {duration::zero(), duration::zero()};
            }
            const duration since_start = std::chrono::duration_cast<duration>(flags_[i] - flags_[0]);
            duration since_prev;
            if (i >= 1) {
                since_prev = std::chrono::duration_cast<duration>(flags_[i] - flags_[i-1]);
            }

            return {since_prev, since_start};
        }

        const std::vector<time_point>& get_flags() const {
            return flags_;
        }

        duration average_delta() const {
            duration total = duration::zero();
            for (std::size_t i = 1; i < flags_.size(); ++i) {
                auto d = flags_[i] - flags_[i-1];
                total += std::chrono::duration_cast<duration>(d);
            }
            return total / (flags_.size()-1);
        }
        
        // Function to measure execution time of a lambda function
        template <typename Func>
        duration measure(Func&& func) {
            auto start_time = clock::now();
            std::forward<Func>(func)(); // Execute the lambda function
            auto end_time = clock::now();
            return std::chrono::duration_cast<duration>(end_time - start_time);
        }
        
    private:
        std::vector<time_point> flags_;
    };

} // namespace demiplane::chrono