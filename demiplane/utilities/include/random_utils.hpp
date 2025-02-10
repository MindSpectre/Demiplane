#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <random>


namespace common::utilities::math::random {
    [[nodiscard]] inline uint32_t generate_random_number(const uint32_t min, const uint32_t max) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution distrib(min, max);
        return distrib(gen);
    }

    [[nodiscard]] inline uint32_t generate_random_uint() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(0, std::numeric_limits<uint32_t>::max());
        return distrib(gen);
    }

    template <typename Returned, typename T, std::size_t N>
    [[nodiscard]] std::vector<Returned> generate_based_on_subset(
        const std::array<T, N>& arr, std::function<Returned(T)> transformation) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(0, 1);
        std::vector<Returned> subset{};
        for (const auto& elem : arr) {
            if (distrib(gen)) {
                subset.emplace_back(transformation(elem));
            }
        }
        return subset;
    }
    template <typename T, std::size_t N>
    [[nodiscard]] std::vector<T> generate_subset(const std::array<T, N>& arr) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(0, 1);
        std::vector<T> subset{};
        for (const auto& elem : arr) {
            if (distrib(gen)) {
                subset.emplace_back(elem);
            }
        }
        return subset;
    }
    template <typename T, std::size_t N>
    [[nodiscard]] T pick(const std::array<T, N>& arr) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(0, arr.size() - 1);
        return arr[distrib(gen)];
    }
    template <typename T>
    [[nodiscard]] std::optional<T> pick(std::span<const T> arr) {
        if (arr.empty()) {
            return std::nullopt; // Handle empty array safely
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> distrib(0, arr.size() - 1);
        return arr[distrib(gen)];
    }

    template <typename Iterator>
    [[nodiscard]] std::optional<typename std::iterator_traits<Iterator>::value_type> pick(
        Iterator begin, Iterator end) {
        if (begin == end) {
            return std::nullopt; // Handle empty range safely
        }

        std::random_device rd;
        std::mt19937 gen(rd()); // Indirect initialization of the random generator
        std::uniform_int_distribution<std::size_t> distrib(0, std::distance(begin, end) - 1);

        std::advance(begin, distrib(gen));
        return *begin;
    }

    inline std::chrono::year_month_day date_generator(
        const std::chrono::year_month_day start_point, const std::chrono::year_month_day end_point) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution year_distribution(start_point.year(), end_point.year());
        std::uniform_int_distribution month_distribution(start_point.month(), end_point.month());
        std::uniform_int_distribution day_distribution(start_point.day(), end_point.day());
        auto dategen_impl = [&] {
            return std::chrono::year_month_day{std::chrono::year{year_distribution(gen)},
                std::chrono::month{month_distribution(gen)}, std::chrono::day{day_distribution(gen)}};
        };
        auto basic_date = [&] {
            return std::chrono::year_month_day{std::chrono::year{1983}, std::chrono::month{7}, std::chrono::day{25}};
        };
        for (uint32_t i = 1; i < 200; ++i) {
            if (auto date = dategen_impl(); date.ok() && date < end_point && date > start_point) {
                return date;
            }
        }
        return basic_date();
    }


    inline std::chrono::year_month_day date_generator_from_x_to_now(const std::chrono::year_month_day start_point) {
        const auto now = std::chrono::system_clock::now();
        auto time      = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&local_tm, &time); // Windows-specific
#else
        localtime_r(&time, &local_tm); // POSIX-specific
#endif

        const int32_t year   = local_tm.tm_year + 1900; // tm_year is years since 1900
        const uint32_t month = local_tm.tm_mon + 1; // tm_mon is 0-based
        const uint32_t day   = local_tm.tm_mday;
        // Set the end date of the found record
        return date_generator(start_point, std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day});
    }
    inline std::chrono::year_month_day date_generator_to_x(const std::chrono::year_month_day end_point) {
        return date_generator(
            std::chrono::year_month_day{std::chrono::year{1925}, std::chrono::month{7}, std::chrono::day{25}},
            end_point);
    }
    inline std::chrono::year_month_day date_generator_last_century() {
        return date_generator_from_x_to_now(
            std::chrono::year_month_day{std::chrono::year{1925}, std::chrono::month{7}, std::chrono::day{25}});
    }

    inline std::pair<std::chrono::year_month_day, std::chrono::year_month_day> time_period_generator(
        const std::chrono::year_month_day from, const std::chrono::year_month_day to) {
        auto start_period_date = date_generator(from, to);
        auto end_period_date   = date_generator(start_period_date, to);
        return std::make_pair(std::move(start_period_date), std::move(end_period_date));
    }


    inline std::pair<std::chrono::year_month_day, std::chrono::year_month_day> time_period_generator() {
        auto start_period_date = date_generator_last_century();
        auto end_period_date   = date_generator_from_x_to_now(start_period_date);
        return std::make_pair(std::move(start_period_date), std::move(end_period_date));
    }


    inline std::pair<std::chrono::year_month_day, std::chrono::year_month_day> time_period_generator_from(
        const std::chrono::year_month_day from) {
        auto start_period_date = date_generator_from_x_to_now(from);
        auto end_period_date   = date_generator_from_x_to_now(start_period_date);
        return std::make_pair(std::move(start_period_date), std::move(end_period_date));
    }


    inline std::pair<std::chrono::year_month_day, std::chrono::year_month_day> time_period_generator_to(
        const std::chrono::year_month_day to) {
        auto start_period_date = date_generator_to_x(to);
        auto end_period_date   = date_generator(start_period_date, to);
        return std::make_pair(std::move(start_period_date), std::move(end_period_date));
    }


    inline std::string generate_random_uuid_v4() {
        std::random_device rd;
        std::mt19937_64 gen(rd()); // 64-bit Mersenne Twister RNG
        std::uniform_int_distribution<uint64_t> distrib(0, std::numeric_limits<uint64_t>::max());

        uint64_t part1 = distrib(gen);
        uint64_t part2 = distrib(gen);

        // Set UUID v4 format (variant 1 and version 4)
        part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL; // Version 4
        part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL; // Variant 1

        // Convert to string
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << ((part1 >> 32) & 0xFFFFFFFF) << "-" << std::setw(4)
           << ((part1 >> 16) & 0xFFFF) << "-" << std::setw(4) << (part1 & 0xFFFF) << "-" << std::setw(4)
           << ((part2 >> 48) & 0xFFFF) << "-" << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

        return ss.str();
    }
} // namespace common::utilities::math::random
