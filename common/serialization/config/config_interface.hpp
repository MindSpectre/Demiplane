#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "field.hpp"
#include "serial_concepts.hpp"

namespace demiplane::serialization {

    template <typename Derived, typename... Formats>
    class ConfigInterface {
    public:
        virtual ~ConfigInterface() = default;

        constexpr virtual void validate() const = 0;

        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        [[nodiscard]] Format serialize() const {
            static_cast<const Derived&>(*this).validate();
            if constexpr (HasCustomSerialize<Derived, Format>) {
                return static_cast<const Derived&>(*this).custom_serialize(std::type_identity<Format>{});
            } else {
                return auto_serialize<Format>();
            }
        }

        template <typename Format>
            requires(std::same_as<Format, Formats> || ...)
        static Derived deserialize(const Format& input) {
            if constexpr (HasCustomDeserialize<Derived, Format>) {
                return Derived::custom_deserialize(input);
            } else {
                return auto_deserialize<Format>(input);
            }
        }

    private:
        template <typename Format>
        [[nodiscard]] Format auto_serialize() const {
            Format out{};
            constexpr auto fs = Derived::fields();
            std::apply(
                [&](const auto&... f) { (serialize_one_field(out, static_cast<const Derived&>(*this), f), ...); }, fs);
            return out;
        }

        template <typename Format, typename F>
        static void serialize_one_field(Format& out, const Derived& d, F) {
            if constexpr (F::policy != FieldPolicy::Secret && F::policy != FieldPolicy::Excluded) {
                write_field(out, F::name.view(), d.*F::ptr);
            }
        }

        template <typename Format>
        static Derived auto_deserialize(const Format& input) {
            auto builder      = []() { return typename Derived::Builder{}; }();
            constexpr auto fs = Derived::fields();
            std::apply([&](const auto&... f) { (deserialize_one_field(input, builder, f), ...); }, fs);
            return std::move(builder).finalize();
        }

        template <typename Format, typename BuilderT, typename F>
        static void deserialize_one_field(const Format& input, BuilderT& builder, F) {
            if constexpr (F::policy != FieldPolicy::Excluded && F::policy != FieldPolicy::ReadOnly) {
                using ValueType = F::value_type;
                if (ValueType val{}; read_field(input, F::name.view(), val)) {
                    builder.config_.*F::ptr = std::move(val);
                }
            }
        }
    };

}  // namespace demiplane::serialization
