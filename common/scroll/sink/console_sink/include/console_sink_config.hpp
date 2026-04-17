#pragma once

#include <iostream>

#include <config_interface.hpp>
#include <json/json.hpp>
#include <prefix_filter.hpp>
#include <sink_interface.hpp>

namespace demiplane::scroll {

    class ConsoleSinkConfig final : public serialization::ConfigInterface<ConsoleSinkConfig, Json::Value> {
    public:
        // Full constructor (escape hatch)
        constexpr ConsoleSinkConfig(const LogLevel threshold,
                                    const bool enable_colors,
                                    const bool flush_each_entry,
                                    std::ostream* const output,
                                    PrefixFilter prefix_filter = {}) noexcept
            : threshold_{threshold},
              enable_colors_{enable_colors},
              flush_each_entry_{flush_each_entry},
              output_{output},
              prefix_filter_{std::move(prefix_filter)} {
        }

        constexpr void validate() const override {
            // always valid
        }

        [[nodiscard]] constexpr LogLevel threshold() const noexcept {
            return threshold_;
        }
        [[nodiscard]] constexpr bool enable_colors() const noexcept {
            return enable_colors_;
        }
        [[nodiscard]] constexpr bool flush_each_entry() const noexcept {
            return flush_each_entry_;
        }
        [[nodiscard]] constexpr std::ostream* output() const noexcept {
            return output_;
        }
        [[nodiscard]] constexpr const PrefixFilter& prefix_filter() const noexcept {
            return prefix_filter_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ConsoleSinkConfig::threshold_, "threshold">{},
                serialization::Field<&ConsoleSinkConfig::enable_colors_, "enable_colors">{},
                serialization::Field<&ConsoleSinkConfig::flush_each_entry_, "flush_each_entry">{},
                serialization::Field<&ConsoleSinkConfig::output_, "output", serialization::FieldPolicy::Excluded>{},
                serialization::
                    Field<&ConsoleSinkConfig::prefix_filter_, "prefix_filter", serialization::FieldPolicy::Excluded>{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr ConsoleSinkConfig() = default;

        LogLevel threshold_    = LogLevel::Debug;
        bool enable_colors_    = true;
        bool flush_each_entry_ = false;
        std::ostream* output_  = &std::cout;
        PrefixFilter prefix_filter_{};
    };

    class ConsoleSinkConfig::Builder {
    public:
        Builder() = default;
        explicit Builder(const ConsoleSinkConfig& existing)
            : config_{existing} {
        }
        explicit Builder(ConsoleSinkConfig&& existing)
            : config_{std::move(existing)} {
        }

        template <typename Self>
        constexpr auto&& threshold(this Self&& self, const LogLevel value) noexcept {
            self.config_.threshold_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& enable_colors(this Self&& self, const bool value) noexcept {
            self.config_.enable_colors_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flush_each_entry(this Self&& self, const bool value) noexcept {
            self.config_.flush_each_entry_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& output(this Self&& self, std::ostream* value) noexcept {
            self.config_.output_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& prefix_filter(this Self&& self, PrefixFilter value) noexcept {
            self.config_.prefix_filter_ = std::move(value);
            return std::forward<Self>(self);
        }

        [[nodiscard]] ConsoleSinkConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ConsoleSinkConfig;
        friend class ConfigInterface;
        ConsoleSinkConfig config_;
    };

}  // namespace demiplane::scroll
