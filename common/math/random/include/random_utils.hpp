#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <sstream>

namespace demiplane::math::random {


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
} // namespace demiplane::utilities::math::random
