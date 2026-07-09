#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <config_interface.hpp>
#include <json/json.hpp>

namespace demiplane::http {

    /**
     * @brief TLS settings consumed by build_ssl_context (spec §7.4 / §10.1),
     *        JSON-loadable via serialization::ConfigInterface.
     *
     * min_version encodes as a string ("tls12" | "tls13"); key_passphrase is
     * FieldPolicy::Secret — read from JSON, never written by dump. The full
     * constructor is a no-validation escape hatch (scaffold tests build empty
     * configs on purpose); Builder::finalize() and deserialize() validate.
     */
    class TlsConfig final : public serialization::ConfigInterface<TlsConfig, Json::Value> {
    public:
        enum class MinVersion : std::uint8_t { tls12, tls13 };

        constexpr TlsConfig(std::string cert_file,
                            std::string key_file,
                            std::string key_passphrase     = "",
                            std::string dh_params_file     = "",
                            std::string ca_file            = "",
                            const MinVersion min_version   = MinVersion::tls12,
                            const bool session_cache       = true,
                            const bool require_client_cert = false) noexcept
            : cert_file_{std::move(cert_file)},
              key_file_{std::move(key_file)},
              key_passphrase_{std::move(key_passphrase)},
              dh_params_file_{std::move(dh_params_file)},
              ca_file_{std::move(ca_file)},
              min_version_{min_version},
              session_cache_{session_cache},
              require_client_cert_{require_client_cert} {
        }

        constexpr void validate() const override {
            if (cert_file_.empty()) {
                throw std::invalid_argument("tls.cert_file must be set");
            }
            if (key_file_.empty()) {
                throw std::invalid_argument("tls.key_file must be set");
            }
            if (require_client_cert_ && ca_file_.empty()) {
                throw std::invalid_argument("tls.ca_file must be set when tls.require_client_cert is true");
            }
        }

        [[nodiscard]] const std::string& cert_file() const noexcept {
            return cert_file_;
        }
        [[nodiscard]] const std::string& key_file() const noexcept {
            return key_file_;
        }
        [[nodiscard]] const std::string& key_passphrase() const noexcept {
            return key_passphrase_;
        }
        [[nodiscard]] const std::string& dh_params_file() const noexcept {
            return dh_params_file_;
        }
        [[nodiscard]] const std::string& ca_file() const noexcept {
            return ca_file_;
        }
        [[nodiscard]] constexpr MinVersion min_version() const noexcept {
            return min_version_;
        }
        [[nodiscard]] constexpr bool session_cache() const noexcept {
            return session_cache_;
        }
        [[nodiscard]] constexpr bool require_client_cert() const noexcept {
            return require_client_cert_;
        }

        static constexpr auto fields() {
            return std::tuple{
                serialization::Field<&TlsConfig::cert_file_, "cert_file">{},
                serialization::Field<&TlsConfig::key_file_, "key_file">{},
                serialization::
                    Field<&TlsConfig::key_passphrase_, "key_passphrase", serialization::FieldPolicy::Secret>{},
                serialization::Field<&TlsConfig::dh_params_file_, "dh_params_file">{},
                serialization::Field<&TlsConfig::ca_file_, "ca_file">{},
                serialization::Field<&TlsConfig::min_version_, "min_version">{},
                serialization::Field<&TlsConfig::session_cache_, "session_cache">{},
                serialization::Field<&TlsConfig::require_client_cert_, "require_client_cert">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr TlsConfig() = default;

        std::string cert_file_;
        std::string key_file_;
        std::string key_passphrase_;
        std::string dh_params_file_;
        std::string ca_file_;
        MinVersion min_version_   = MinVersion::tls12;
        bool session_cache_       = true;
        bool require_client_cert_ = false;
    };

    // ── MinVersion <-> string codec ──────────────────────────────────────
    // ADL-found by the serialization machinery (a non-template overload beats
    // the generic int-encoding enum template). String-encoded per spec §14.1.

    [[nodiscard]] constexpr std::string_view to_string_view(const TlsConfig::MinVersion v) noexcept {
        switch (v) {
            case TlsConfig::MinVersion::tls13:
                return "tls13";
            case TlsConfig::MinVersion::tls12:
                return "tls12";
        }
        return "tls12";
    }

    inline void write_field(Json::Value& out, const serialization::FieldName key, const TlsConfig::MinVersion v) {
        out[key.str()] = std::string{to_string_view(v)};
    }

    /// @throws std::invalid_argument on an unknown string — a silent default
    /// would turn a typo into weaker TLS.
    inline bool read_field(const Json::Value& in, const serialization::FieldName key, TlsConfig::MinVersion& v) {
        const std::string k = key.str();
        if (!in.isMember(k)) {
            return false;
        }
        const std::string raw = in[k].asString();  // Json::LogicError on non-string
        if (raw == "tls12") {
            v = TlsConfig::MinVersion::tls12;
            return true;
        }
        if (raw == "tls13") {
            v = TlsConfig::MinVersion::tls13;
            return true;
        }
        throw std::invalid_argument{
            "config field '" + k + "': unknown min_version '" + raw + R"(' (expected "tls12" or "tls13"))"};
    }

    class TlsConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& cert_file(this Self&& self, std::string value) noexcept {
            self.config_.cert_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& key_file(this Self&& self, std::string value) noexcept {
            self.config_.key_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& key_passphrase(this Self&& self, std::string value) noexcept {
            self.config_.key_passphrase_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& dh_params_file(this Self&& self, std::string value) noexcept {
            self.config_.dh_params_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& ca_file(this Self&& self, std::string value) noexcept {
            self.config_.ca_file_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& min_version(this Self&& self, const MinVersion value) noexcept {
            self.config_.min_version_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& session_cache(this Self&& self, const bool value) noexcept {
            self.config_.session_cache_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& require_client_cert(this Self&& self, const bool value) noexcept {
            self.config_.require_client_cert_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] TlsConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class TlsConfig;
        friend class ConfigInterface;
        TlsConfig config_;
    };

}  // namespace demiplane::http
