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
                // FieldName (not a bare string) keeps demiplane::serialization
                // an associated namespace of this dependent call — the only
                // route by which two-phase lookup reaches the format overloads
                // (see field.hpp).
                write_field(out, FieldName{F::name.view()}, d.*F::ptr);
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
                // Read straight into the builder's member: no temporary, so a
                // field's type need not be default-constructible at namespace
                // scope (nested ConfigInterface types keep their private
                // framework constructors), and a missing key leaves the
                // member's declared default untouched.
                read_field(input, FieldName{F::name.view()}, builder.config_.*F::ptr);
            }
        }
    };

}  // namespace demiplane::serialization
