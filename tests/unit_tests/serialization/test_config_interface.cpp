#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <config_interface.hpp>
#include <json/json.hpp>

// Exercises the generic fields()/Builder machinery end-to-end against a
// FileSinkConfig-shaped pair of test configs — including the two shapes the
// HTTP config layer depends on: nested configs with PRIVATE framework
// default constructors (held directly, in std::optional, and in std::vector)
// and std::uint16_t fields. This is the machinery's own lock, independent of
// the HTTP component (it must keep passing with BUILD_COMPONENTS=OFF).

namespace {

    namespace ser = demiplane::serialization;

    class InnerConfig final : public ser::ConfigInterface<InnerConfig, Json::Value> {
    public:
        constexpr explicit InnerConfig(const int retries) noexcept
            : retries_{retries} {
        }

        constexpr void validate() const override {
            if (retries_ < 0) {
                throw std::invalid_argument("inner.retries must be non-negative");
            }
        }

        [[nodiscard]] constexpr int retries() const noexcept {
            return retries_;
        }

        static constexpr auto fields() {
            return std::tuple{
                ser::Field<&InnerConfig::retries_, "retries">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr InnerConfig() = default;

        int retries_ = 1;
    };

    class InnerConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        constexpr auto&& retries(this Self&& self, const int value) noexcept {
            self.config_.retries_ = value;
            return std::forward<Self>(self);
        }

        [[nodiscard]] InnerConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class InnerConfig;
        friend class ConfigInterface;
        InnerConfig config_;
    };

    class OuterConfig final : public ser::ConfigInterface<OuterConfig, Json::Value> {
    public:
        enum class Mode : std::uint8_t { fast, safe };

        void validate() const override {
            if (name_.empty()) {
                throw std::invalid_argument("outer.name must not be empty");
            }
            inner_.validate();
            if (maybe_inner_) {
                maybe_inner_->validate();
            }
            for (const auto& item : items_) {
                item.validate();
            }
        }

        [[nodiscard]] const std::string& name() const noexcept {
            return name_;
        }
        [[nodiscard]] std::size_t count() const noexcept {
            return count_;
        }
        [[nodiscard]] std::uint16_t port() const noexcept {
            return port_;
        }
        [[nodiscard]] bool flag() const noexcept {
            return flag_;
        }
        [[nodiscard]] std::chrono::milliseconds delay() const noexcept {
            return delay_;
        }
        [[nodiscard]] Mode mode() const noexcept {
            return mode_;
        }
        [[nodiscard]] const std::string& secret() const noexcept {
            return secret_;
        }
        [[nodiscard]] const InnerConfig& inner() const noexcept {
            return inner_;
        }
        [[nodiscard]] const std::optional<InnerConfig>& maybe_inner() const noexcept {
            return maybe_inner_;
        }
        [[nodiscard]] const std::vector<InnerConfig>& items() const noexcept {
            return items_;
        }
        [[nodiscard]] const std::vector<int>& numbers() const noexcept {
            return numbers_;
        }
        [[nodiscard]] const std::map<std::string, std::string>& labels() const noexcept {
            return labels_;
        }

        static constexpr auto fields() {
            return std::tuple{
                ser::Field<&OuterConfig::name_, "name">{},
                ser::Field<&OuterConfig::count_, "count">{},
                ser::Field<&OuterConfig::port_, "port">{},
                ser::Field<&OuterConfig::flag_, "flag">{},
                ser::Field<&OuterConfig::delay_, "delay_ms">{},
                ser::Field<&OuterConfig::mode_, "mode">{},
                ser::Field<&OuterConfig::secret_, "secret", ser::FieldPolicy::Secret>{},
                ser::Field<&OuterConfig::inner_, "inner">{},
                ser::Field<&OuterConfig::maybe_inner_, "maybe_inner">{},
                ser::Field<&OuterConfig::items_, "items">{},
                ser::Field<&OuterConfig::numbers_, "numbers">{},
                ser::Field<&OuterConfig::labels_, "labels">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        OuterConfig() = default;

        std::string name_  = "default";
        std::size_t count_ = 0;
        std::uint16_t port_ = 80;
        bool flag_ = false;
        std::chrono::milliseconds delay_{250};
        Mode mode_ = Mode::fast;
        std::string secret_;
        // InnerConfig's framework default ctor is private — the member default
        // goes through the public full ctor (the exact shape ServerConfig uses
        // for its nested Timeouts).
        InnerConfig inner_ = InnerConfig{1};
        std::optional<InnerConfig> maybe_inner_{};
        std::vector<InnerConfig> items_{};
        std::vector<int> numbers_{};
        std::map<std::string, std::string> labels_ = {{"env", "dev"}};
    };

    class OuterConfig::Builder {
    public:
        Builder() = default;

        template <typename Self>
        auto&& name(this Self&& self, std::string value) noexcept {
            self.config_.name_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& count(this Self&& self, const std::size_t value) noexcept {
            self.config_.count_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.port_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& flag(this Self&& self, const bool value) noexcept {
            self.config_.flag_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& delay(this Self&& self, const std::chrono::milliseconds value) noexcept {
            self.config_.delay_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& mode(this Self&& self, const Mode value) noexcept {
            self.config_.mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& secret(this Self&& self, std::string value) noexcept {
            self.config_.secret_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& inner(this Self&& self, InnerConfig value) noexcept {
            self.config_.inner_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& maybe_inner(this Self&& self, InnerConfig value) noexcept {
            self.config_.maybe_inner_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& items(this Self&& self, std::vector<InnerConfig> value) noexcept {
            self.config_.items_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& numbers(this Self&& self, std::vector<int> value) noexcept {
            self.config_.numbers_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        auto&& labels(this Self&& self, std::map<std::string, std::string> value) noexcept {
            self.config_.labels_ = std::move(value);
            return std::forward<Self>(self);
        }

        [[nodiscard]] OuterConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class OuterConfig;
        friend class ConfigInterface;
        OuterConfig config_;
    };

    OuterConfig make_full_config() {
        return OuterConfig::Builder{}
            .name("svc")
            .count(7)
            .port(8443)
            .flag(true)
            .delay(std::chrono::milliseconds{1500})
            .mode(OuterConfig::Mode::safe)
            .secret("hunter2")
            .inner(InnerConfig{2})
            .maybe_inner(InnerConfig{5})
            .items({InnerConfig{10}, InnerConfig{20}})
            .numbers({1, 2, 3})
            .finalize();
    }

}  // namespace

TEST(ConfigInterfaceTest, RoundTripPreservesEveryField) {
    const auto cfg   = make_full_config();
    Json::Value json = cfg.serialize<Json::Value>();
    // Secret fields do not survive the trip by design — re-inject for the
    // deserialize leg so this test covers the Secret READ path too.
    json["secret"] = "hunter2";
    const auto back = OuterConfig::deserialize<Json::Value>(json);

    EXPECT_EQ(back.name(), "svc");
    EXPECT_EQ(back.count(), 7u);
    EXPECT_EQ(back.port(), 8443);
    EXPECT_TRUE(back.flag());
    EXPECT_EQ(back.delay(), std::chrono::milliseconds{1500});
    EXPECT_EQ(back.mode(), OuterConfig::Mode::safe);
    EXPECT_EQ(back.secret(), "hunter2");
    EXPECT_EQ(back.inner().retries(), 2);
    ASSERT_TRUE(back.maybe_inner().has_value());
    EXPECT_EQ(back.maybe_inner()->retries(), 5);
    ASSERT_EQ(back.items().size(), 2u);
    EXPECT_EQ(back.items()[0].retries(), 10);
    EXPECT_EQ(back.items()[1].retries(), 20);
    EXPECT_EQ(back.numbers(), (std::vector{1, 2, 3}));
}

TEST(ConfigInterfaceTest, SecretFieldIsNotSerialized) {
    const auto json = make_full_config().serialize<Json::Value>();
    EXPECT_FALSE(json.isMember("secret"));
}

TEST(ConfigInterfaceTest, MissingKeysKeepDeclaredDefaults) {
    const auto cfg = OuterConfig::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(cfg.name(), "default");
    EXPECT_EQ(cfg.count(), 0u);
    EXPECT_EQ(cfg.port(), 80);
    EXPECT_FALSE(cfg.flag());
    EXPECT_EQ(cfg.delay(), std::chrono::milliseconds{250});
    EXPECT_EQ(cfg.mode(), OuterConfig::Mode::fast);
    EXPECT_EQ(cfg.inner().retries(), 1);
    EXPECT_FALSE(cfg.maybe_inner().has_value());
    EXPECT_TRUE(cfg.items().empty());
    EXPECT_TRUE(cfg.numbers().empty());
    ASSERT_EQ(cfg.labels().size(), 1u);
    EXPECT_EQ(cfg.labels().at("env"), "dev");
}

TEST(ConfigInterfaceTest, UnknownJsonKeysAreIgnored) {
    Json::Value json{Json::objectValue};
    json["name"]        = "svc";
    json["not_a_field"] = 42;
    const auto cfg      = OuterConfig::deserialize<Json::Value>(json);
    EXPECT_EQ(cfg.name(), "svc");
}

TEST(ConfigInterfaceTest, TypeMismatchThrowsJsonException) {
    Json::Value json{Json::objectValue};
    json["count"] = "not-a-number";
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), Json::Exception);
}

TEST(ConfigInterfaceTest, VectorFieldRejectsNonArray) {
    Json::Value json{Json::objectValue};
    json["items"] = 42;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, Uint16RangeIsEnforced) {
    Json::Value json{Json::objectValue};
    json["port"] = 70000;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, DeserializeRunsValidate) {
    Json::Value inner{Json::objectValue};
    inner["retries"] = -3;
    Json::Value json{Json::objectValue};
    json["inner"] = inner;
    EXPECT_THROW((void)OuterConfig::deserialize<Json::Value>(json), std::invalid_argument);
}

TEST(ConfigInterfaceTest, SerializeRunsValidate) {
    // The full ctor is the no-validation escape hatch; serialize() validates.
    EXPECT_THROW((void)InnerConfig{-1}.serialize<Json::Value>(), std::invalid_argument);
}

TEST(ConfigInterfaceTest, MapFieldReplacesNonEmptyDefault) {
    Json::Value labels{Json::objectValue};
    labels["region"] = "eu";
    Json::Value json{Json::objectValue};
    json["labels"] = labels;
    const auto cfg = OuterConfig::deserialize<Json::Value>(json);
    ASSERT_EQ(cfg.labels().size(), 1u);  // default {"env","dev"} REPLACED, not merged
    EXPECT_EQ(cfg.labels().at("region"), "eu");
}
