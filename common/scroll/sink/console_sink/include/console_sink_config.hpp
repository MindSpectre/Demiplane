#pragma once
#include <gears_class_traits.hpp>
#include <json/value.h>
#include <sink_interface.hpp>
namespace demiplane::scroll {
    /**
     * @brief Configuration for console output
     */
    class ConsoleSinkConfig final : public gears::ConfigInterface<ConsoleSinkConfig, Json::Value> {
    public:
        constexpr ConsoleSinkConfig(const LogLevel threshold,
                                    const bool enable_colors,
                                    const bool flush_each_entry,
                                    std::ostream* const output) noexcept
            : threshold_{threshold},
              enable_colors_{enable_colors},
              flush_each_entry_{flush_each_entry},
              output_{output} {
        }
        constexpr ConsoleSinkConfig() = default;

        constexpr void validate() override {
            // always valid
        }
        [[nodiscard]] Json::Value serialize() const override {
            Json::Value result;
            // TODO: Add serialization
            return result;
        }
        static ConsoleSinkConfig deserialize(const Json::Value& config) {
            gears::unused_value(config);
            // TODO: Add deserialization
            std::unreachable();
        }

        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel threshold) noexcept {
            self.threshold_ = threshold;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& enable_colors(this Self&& self, const bool enable_colors) noexcept {
            self.enable_colors_ = enable_colors;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool flush_each_entry) noexcept {
            self.flush_each_entry_ = flush_each_entry;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& output(this Self&& self, std::ostream* output) noexcept {
            self.output_ = output;
            return std::forward<Self>(self);
        }

        [[nodiscard]] constexpr LogLevel get_threshold() const noexcept {
            return threshold_;
        }
        [[nodiscard]] constexpr bool is_enable_colors() const noexcept {
            return enable_colors_;
        }
        [[nodiscard]] constexpr bool is_flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        [[nodiscard]] constexpr std::ostream* get_output() const {
            return output_;
        }

    private:
        LogLevel threshold_    = LogLevel::Debug;
        bool enable_colors_    = true;
        bool flush_each_entry_ = false;
        std::ostream* output_  = &std::cout;  // Can redirect to &std::cerr
    };
}  // namespace demiplane::scroll
