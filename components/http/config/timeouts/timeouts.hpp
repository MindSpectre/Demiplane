#pragma once

#include <chrono>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <config_interface.hpp>
#include <json/json.hpp>

namespace demiplane::http {

    /**
     * @brief Per-phase HTTP timeout set (spec §10.1), JSON-loadable.
     *
     * Field names carry the unit (header_ms/body_ms/idle_ms): JSON values are
     * plain integers interpreted as milliseconds. attach_default_listeners
     * maps these onto Http11Config's per-phase timeouts.
     *
     * idle_ms is accepted and mapped but RESERVED in v1: the h1 driver reuses
     * the header timeout for keep-alive idle (see http11_config.hpp).
     */
    class Timeouts final : public serialization::ConfigInterface<Timeouts, Json::Value> {
    public:
        constexpr Timeouts(const std::chrono::milliseconds header,
                           const std::chrono::milliseconds body,
                           const std::chrono::milliseconds idle) noexcept
            : header_{header},
              body_{body},
              idle_{idle} {
        }

        constexpr void validate() const override {
            if (header_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.header_ms must be positive");
            }
            if (body_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.body_ms must be positive");
            }
            if (idle_ <= std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("timeouts.idle_ms must be positive");
            }
        }

        [[nodiscard]] constexpr std::chrono::milliseconds header() const noexcept {
            return header_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds body() const noexcept {
            return body_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds idle() const noexcept {
            return idle_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&Timeouts::header_, "header_ms">{},
                serialization::Field<&Timeouts::body_, "body_ms">{},
                serialization::Field<&Timeouts::idle_, "idle_ms">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr Timeouts() = default;

        std::chrono::milliseconds header_ = std::chrono::seconds{10};
        std::chrono::milliseconds body_   = std::chrono::seconds{30};
        std::chrono::milliseconds idle_   = std::chrono::seconds{60};
    };

    class Timeouts::Builder {
    public:
        Builder() = default;

        template <typename Self>
        constexpr auto&& header(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.header_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& body(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.body_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.idle_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] Timeouts finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class Timeouts;
        friend class ConfigInterface;
        Timeouts config_;
    };

}  // namespace demiplane::http
