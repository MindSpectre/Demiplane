#include "generators/number_generator.hpp"
#include "generators/time_generator.hpp"

std::chrono::milliseconds
demiplane::math::random::RandomTimeGenerator::generate_milliseconds(const uint32_t target_ms,
                                                                    const int8_t deviation) const {
    if (deviation < 0 || deviation > 100) {
        throw std::invalid_argument("Both target_ms and deviation must be non-negative, deviation <= 100");
    }

    const auto lower_bound = static_cast<uint32_t>(target_ms * (100.0 - deviation) / 100.0);
    const auto upper_bound = static_cast<uint32_t>(target_ms * (100.0 + deviation) / 100.0);

    std::uniform_int_distribution dist(lower_bound, upper_bound);
    return std::chrono::milliseconds{dist(generator_)};
}
std::chrono::year_month_day
demiplane::math::random::RandomTimeGenerator::generate_date(const std::chrono::year_month_day& from,
                                                            const std::chrono::year_month_day& to) const {
    const auto start_days = std::chrono::sys_days{from};
    const auto end_days   = std::chrono::sys_days{to};

    std::uniform_int_distribution<int64_t> dist(0, (end_days - start_days).count());
    return std::chrono::year_month_day{start_days + std::chrono::days{dist(generator_)}};
}
std::chrono::year_month_day
demiplane::math::random::RandomTimeGenerator::generate_date_from_to_now(const std::chrono::year_month_day& from) const {
    const auto now_time = std::chrono::system_clock::now();
    const auto now_day  = std::chrono::floor<std::chrono::days>(now_time);
    return generate_date(from, std::chrono::year_month_day{now_day});
}
std::chrono::year_month_day
demiplane::math::random::RandomTimeGenerator::generate_date_to(const std::chrono::year_month_day& to) const {
    return generate_date(start_of_last_century_, to);
}
std::chrono::year_month_day demiplane::math::random::RandomTimeGenerator::generate_date_last_century() const {
    return generate_date_from_to_now(start_of_last_century_);
}
std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
demiplane::math::random::RandomTimeGenerator::generate_time_period(const std::chrono::year_month_day& from,
                                                                   const std::chrono::year_month_day& to) const {
    auto start = generate_date(from, to);
    auto end   = generate_date(start, to);
    return {start, end};
}
std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
demiplane::math::random::RandomTimeGenerator::generate_time_period() const {
    auto start = generate_date_last_century();
    auto end   = generate_date_from_to_now(start);
    return {start, end};
}
std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
demiplane::math::random::RandomTimeGenerator::generate_time_period_from(const std::chrono::year_month_day& from) const {
    auto start = generate_date_from_to_now(from);
    auto end   = generate_date_from_to_now(start);
    return {start, end};
}
std::pair<std::chrono::year_month_day, std::chrono::year_month_day>
demiplane::math::random::RandomTimeGenerator::generate_time_period_to(const std::chrono::year_month_day& to) const {
    auto start = generate_date_to(to);
    auto end   = generate_date(start, to);
    return {start, end};
}
uint32_t demiplane::math::random::NumberGenerator::generate_random_uint32(const uint32_t min,
                                                                          const uint32_t max) const {
    std::uniform_int_distribution distrib(min, max);
    return distrib(generator_);
}
