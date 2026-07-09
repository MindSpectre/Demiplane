#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <config_interface.hpp>
#include <json/json.hpp>
#include <listener_config.hpp>
#include <timeouts.hpp>

namespace demiplane::http {

    /**
     * @brief Server-level configuration (spec §10.1), JSON-loadable via
     *        serialization::ConfigInterface.
     *
     * `threads` is consumed only by run_standalone-style callers — the
     * injected-executor path takes its threads from whoever drives the
     * executor (spec §10.3). `path_normalization` mirrors routing's enum; the
     * Server maps it (map_normalization) so the config layer carries no
     * routing dependency. Builder-only construction: every default is valid,
     * so `ServerConfig::Builder{}.finalize()` is the canonical empty config.
     */
    class ServerConfig final : public serialization::ConfigInterface<ServerConfig, Json::Value> {
    public:
        enum class PathNormalization : std::uint8_t {
            none,                     ///< exact byte match
            collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
            collapse_multi_slash,     ///< + "/users//42" == "/users/42"
        };

        void validate() const override {
            if (threads_ == 0) {
                throw std::invalid_argument("server.threads must be >= 1");
            }
            if (body_limit_ == 0) {
                throw std::invalid_argument("server.body_limit must be positive");
            }
            if (request_arena_size_ == 0) {
                throw std::invalid_argument("server.request_arena_size must be positive");
            }
            if (drain_timeout_ < std::chrono::milliseconds::zero()) {
                throw std::invalid_argument("server.drain_timeout_ms must be non-negative");
            }
            timeouts_.validate();
            for (const auto& listener : listeners_) {
                listener.validate();
            }
        }

        [[nodiscard]] const std::vector<ListenerConfig>& listeners() const noexcept {
            return listeners_;
        }
        [[nodiscard]] constexpr std::size_t threads() const noexcept {
            return threads_;
        }
        [[nodiscard]] constexpr const Timeouts& timeouts() const noexcept {
            return timeouts_;
        }
        [[nodiscard]] constexpr std::size_t body_limit() const noexcept {
            return body_limit_;
        }
        [[nodiscard]] constexpr std::size_t request_arena_size() const noexcept {
            return request_arena_size_;
        }
        [[nodiscard]] constexpr std::chrono::milliseconds drain_timeout() const noexcept {
            return drain_timeout_;
        }
        [[nodiscard]] constexpr PathNormalization path_normalization() const noexcept {
            return path_normalization_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&ServerConfig::listeners_, "listeners">{},
                serialization::Field<&ServerConfig::threads_, "threads">{},
                serialization::Field<&ServerConfig::timeouts_, "timeouts">{},
                serialization::Field<&ServerConfig::body_limit_, "body_limit">{},
                serialization::Field<&ServerConfig::request_arena_size_, "request_arena_size">{},
                serialization::Field<&ServerConfig::drain_timeout_, "drain_timeout_ms">{},
                serialization::Field<&ServerConfig::path_normalization_, "path_normalization">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        ServerConfig() = default;

        std::vector<ListenerConfig> listeners_{};
        std::size_t threads_ = 1;
        // Timeouts' framework default ctor is private — the member default
        // goes through its public full ctor.
        Timeouts timeouts_ =
            Timeouts{std::chrono::seconds{10}, std::chrono::seconds{30}, std::chrono::seconds{60}};
        std::size_t body_limit_         = 16 * 1024 * 1024;
        std::size_t request_arena_size_ = 8192;
        std::chrono::milliseconds drain_timeout_{std::chrono::seconds{30}};
        PathNormalization path_normalization_ = PathNormalization::collapse_trailing_slash;
    };

    // ── PathNormalization <-> string codec ───────────────────────────────

    [[nodiscard]] constexpr std::string_view to_string_view(const ServerConfig::PathNormalization p) noexcept {
        switch (p) {
            case ServerConfig::PathNormalization::none:
                return "none";
            case ServerConfig::PathNormalization::collapse_multi_slash:
                return "collapse_multi_slash";
            case ServerConfig::PathNormalization::collapse_trailing_slash:
                return "collapse_trailing_slash";
        }
        return "collapse_trailing_slash";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key,
                            const ServerConfig::PathNormalization v) {
        out[key.str()] = std::string{to_string_view(v)};
    }

    inline bool read_field(const Json::Value& in, const serialization::FieldName key,
                           ServerConfig::PathNormalization& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "none") {
            v = ServerConfig::PathNormalization::none;
            return true;
        }
        if (raw == "collapse_trailing_slash") {
            v = ServerConfig::PathNormalization::collapse_trailing_slash;
            return true;
        }
        if (raw == "collapse_multi_slash") {
            v = ServerConfig::PathNormalization::collapse_multi_slash;
            return true;
        }
        throw std::invalid_argument{"config field '" + k + "': unknown path_normalization '" + raw +
                                    R"(' (expected "none"|"collapse_trailing_slash"|"collapse_multi_slash"))"};
    }

    class ServerConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& listeners(this Self&& self, std::vector<ListenerConfig> value) noexcept {
            self.config_.listeners_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& add_listener(this Self&& self, ListenerConfig value) noexcept {
            self.config_.listeners_.push_back(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& threads(this Self&& self, const std::size_t value) noexcept {
            self.config_.threads_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& timeouts(this Self&& self, Timeouts value) noexcept {
            self.config_.timeouts_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& body_limit(this Self&& self, const std::size_t value) noexcept {
            self.config_.body_limit_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& request_arena_size(this Self&& self, const std::size_t value) noexcept {
            self.config_.request_arena_size_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& drain_timeout(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.drain_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& path_normalization(this Self&& self, const PathNormalization value) noexcept {
            self.config_.path_normalization_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] ServerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ServerConfig;
        friend class ConfigInterface;
        ServerConfig config_;
    };

}  // namespace demiplane::http
