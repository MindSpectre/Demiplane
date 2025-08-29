#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace demiplane::chrono {
    /**
     * @brief Stopwatch class used for measuring time intervals.
     * @tparam T The time unit used for measuring intervals (milliseconds by default).
     */
    template <typename T = std::chrono::milliseconds>
    class PrintingStopwatch {
        public:
        /**
         * @brief Checks if time is counted from the previous flag.
         *
         * @return true if countdown is from the previous flag.
         */
        [[nodiscard]] bool is_count_from_prev() const {
            return countdown_from_prev_;
        }

        /**
         * @brief Sets countdown from the previous flag.
         *
         * @param state Boolean value to set countdown from the previous flag.
         */
        void set_countdown_from_prev(const bool state) {
            countdown_from_prev_ = state;
        }

        /**
         * @brief Checks if time is counted from the start time.
         *
         * @return true if countdown is from the start time.
         */
        [[nodiscard]] bool is_count_from_start() const {
            return countdown_from_start_;
        }

        /**
         * @brief Sets countdown from the start time.
         *
         * @param state Boolean value to set countdown from start.
         */
        void set_countdown_from_start(const bool state) {
            countdown_from_start_ = state;
        }

        /**
         * @brief Constructor that initializes the stopwatch and reserves memory for flags.
         *
         * @param name Name of measure
         * @param flags_cnt_reserve The initial reserved size of the flags vector.
         */
        // Constructor
        explicit PrintingStopwatch(std::string name = "", const std::size_t flags_cnt_reserve = 30)
            : running_name_(std::move(name)) {
            flags_.reserve(flags_cnt_reserve);
        }
        static constexpr const char* name() {
            return "Stopwatch";
        }

        /**
         * @brief Destructor that automatically prints the times.
         */
        // Destructor automatically flushes the times
        ~PrintingStopwatch() noexcept {
            print();
        }

        /**
         * @brief Starts or resets the stopwatch.
         */
        // Start or reset the stopwatch
        void start(std::string name) noexcept {
            running_name_ = std::move(name);
            start_time_   = now();
            flags_.clear();
        }
        void start() noexcept {
            start_time_ = now();
            flags_.clear();
        }
        /**
         * @brief Stops the stopwatch and prints the times.
         */
        // Finish and print the times
        void finish() noexcept {
            flag("Finish");
            print();
        }

        /**
         * @brief Prints all flags and time differences.
         */
        // Flush the times (printing all flags and time differences)
        void print() noexcept {
            if (flags_.empty()) {
                return;
            }

            std::ostringstream stream;
            stream << "\nRunning: " << running_name_ << '\n' << "Stopwatch times (in " << time_unit_name() << "):\n";
            auto previous = start_time_;
            for (size_t i = 0; i < flags_.size(); ++i) {
                if (flags_[i].name_.empty()) {
                    stream << "Flag " << i + 1;
                } else {
                    stream << flags_[i].name_;
                }
                if (countdown_from_prev_) {
                    stream << "  |  " << std::chrono::duration_cast<T>(flags_[i].point_ - previous).count()
                           << time_unit_name_short();
                }
                if (countdown_from_start_) {
                    stream << "  |  " << std::chrono::duration_cast<T>(flags_[i].point_ - start_time_).count()
                           << time_unit_name_short();
                }
                stream << "\n";
                previous = flags_[i].point_;
            }
            stream << std::endl;
            std::cout << stream.str();
            flags_.clear();
        }

        /**
         * @brief Resets the stopwatch, clears flags, and sets a new start time.
         */
        void reset() noexcept {
            start();
        }

        /**
         * @brief Adds a new flag using the postfix increment operator.
         *
         * @return Reference to the Stopwatch object.
         */
        PrintingStopwatch& operator++(int) {
            flag("");
            return *this;
        }

        /**
         * @brief Adds a new flag using the prefix increment operator.
         *
         * @return Reference to the Stopwatch object.
         */
        PrintingStopwatch& operator++() {
            flag("");
            return *this;
        }

        /**
         * @brief Removes the last flag using the postfix decrement operator.
         *
         * @return Reference to the Stopwatch object.
         */
        PrintingStopwatch& operator--(int) {
            if (!flags_.empty()) {
                flags_.pop_back();
            }
            return *this;
        }

        /**
         * @brief Removes the last flag using the prefix decrement operator.
         *
         * @return Reference to the Stopwatch object.
         */
        PrintingStopwatch& operator--() {
            if (!flags_.empty()) {
                flags_.pop_back();
            }
            return *this;
        }

        /**
         * @brief Adds a new flag with the specified name.
         *
         * @tparam string_mv The type of the name parameter (must be convertible to std::string).
         * @param name The name of the flag.
         */
        template <typename string_mv>
        std::enable_if_t<std::is_same_v<std::remove_cvref_t<string_mv>, std::string> ||
                             std::is_convertible_v<string_mv, std::string>,
                         void>
        flag(string_mv&& name) {
            flags_.emplace_back(std::forward<string_mv>(name), now());
        }

        // Overload -- operator to remove the last flag

        private:
        bool countdown_from_prev_  = true;
        bool countdown_from_start_ = true;
        std::string running_name_;

        struct Flag {
            std::string name_;
            std::chrono::time_point<std::chrono::high_resolution_clock> point_;

            Flag(std::string&& name, const std::chrono::time_point<std::chrono::high_resolution_clock> point)
                : name_(std::move(name)),
                  point_(point) {
            }

            Flag(const std::string& name, const std::chrono::time_point<std::chrono::high_resolution_clock> point)
                : name_(name),
                  point_(point) {
            }
        };

        /**
         * @brief Gets the current time.
         *
         * @return The current time point.
         */
        static constexpr auto now() noexcept {
            return std::chrono::high_resolution_clock::now();
        }

        /**
         * @brief Gets the name of the time unit as a string.
         *
         * @return The name of the time unit (e.g., milliseconds).
         */
        [[nodiscard]] static constexpr const char* time_unit_name() {
            if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
                return "milliseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
                return "microseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
                return "nanoseconds";
            } else if constexpr (std::is_same_v<T, std::chrono::seconds>) {
                return "seconds";
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for time");
            }
            return {};
        }

        /**
         * @brief Gets the short name of the time unit.
         *
         * @return The short name of the time unit (e.g., mls for milliseconds).
         */
        [[nodiscard]] static constexpr const char* time_unit_name_short() {
            if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
                return "mls";
            } else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
                return "mcs";
            } else if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
                return "ns";
            } else if constexpr (std::is_same_v<T, std::chrono::seconds>) {
                return "s";
            } else {
                static_assert(sizeof(T) == 0, "Unsupported field type for time");
            }
            return {};
        }

        std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
        std::vector<Flag> flags_;
    };
}  // namespace demiplane::chrono
