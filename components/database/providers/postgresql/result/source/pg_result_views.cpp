#include "pg_result_views.hpp"

#include <variant>

#include <db_field_value.hpp>
#include <netinet/in.h>
#include <pg_type_registry.hpp>

namespace demiplane::db::postgres {
    namespace {
        template <typename... Ts>
        // ReSharper disable once CppDFAUnreachableFunctionCall
        [[maybe_unused]] consteval void compile_time_test(const std::variant<Ts...>&) {
            const FieldView fv{nullptr, 0, true, 0, 0};
            // This will fail to compile if any as<T> is missing
            ((void)fv.as<Ts>(), ...);
        }

        // Force instantiation
        [[maybe_unused]] consteval void test() {
            compile_time_test(FieldValue{});
        }
    }  // namespace

    namespace detail {
        std::uint64_t ntoh64(const std::uint64_t net) {
            if constexpr (std::endian::native == std::endian::big) {
                // ReSharper disable once CppDFAUnreachableCode
                return net;
            } else {
                return (static_cast<uint64_t>(ntohl(net >> 32)) |
                        (static_cast<uint64_t>(ntohl(net & 0xFFFFFFFF)) << 32));
            }
        }

        std::uint64_t hton64(std::uint64_t host) {
            return ntoh64(host);  // Same operation
        }
    }  // namespace detail

    bool FieldView::decode_bool() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_bool) {
            return *ptr_ != 0;
        }
        auto sv = as_sv();
        return sv == "t" || sv == "true" || sv == "1" || sv == "TRUE" || sv == "T";
    }
    int16_t FieldView::decode_int16() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_int2) {
            uint16_t be;
            std::memcpy(&be, ptr_, 2);
            return static_cast<int16_t>(ntohs(be));
        }
        return decode_integer_text<int16_t>();
    }
    int32_t FieldView::decode_int32() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_int4) {
            uint32_t be;
            std::memcpy(&be, ptr_, 4);
            return static_cast<int32_t>(ntohl(be));
        }
        return decode_integer_text<int32_t>();
    }
    int64_t FieldView::decode_int64() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_int8) {
            uint64_t be;
            std::memcpy(&be, ptr_, 8);
            return static_cast<int64_t>(detail::ntoh64(be));
        }
        return decode_integer_text<int64_t>();
    }
    float FieldView::decode_float() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_float8) {
            uint32_t be;
            std::memcpy(&be, ptr_, 4);
            const uint32_t host = ntohl(be);
            float f;
            std::memcpy(&f, &host, 4);
            return f;
        }
        // Use from_chars for better performance
        float result;
        const auto sv = as_sv();
        if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result); ec != std::errc{}) {
            // Fallback for special values
            const std::string s(sv);
            if (s == "NaN" || s == "nan")
                return std::numeric_limits<float>::quiet_NaN();
            if (s == "Infinity" || s == "inf")
                return std::numeric_limits<float>::infinity();
            if (s == "-Infinity" || s == "-inf")
                return -std::numeric_limits<float>::infinity();
            throw std::runtime_error("Failed to parse float: " + s);
        }
        return result;
    }
    double FieldView::decode_double() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_float8) {
            uint64_t be;
            std::memcpy(&be, ptr_, 8);
            const uint64_t host = detail::ntoh64(be);
            double d;
            std::memcpy(&d, &host, 8);
            return d;
        }
        // Use from_chars for better performance
        double result;
        auto sv        = as_sv();
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
        if (ec != std::errc{}) {
            // Fallback for special values
            std::string s(sv);
            if (s == "NaN" || s == "nan")
                return std::numeric_limits<double>::quiet_NaN();
            if (s == "Infinity" || s == "inf")
                return std::numeric_limits<double>::infinity();
            if (s == "-Infinity" || s == "-inf")
                return -std::numeric_limits<double>::infinity();
            throw std::runtime_error("Failed to parse double: " + s);
        }
        return result;
    }
    std::string FieldView::decode_string() const {
        return std::string{ptr_, len_};
    }
    std::string_view FieldView::decode_string_view() const {
        // DANGER: View is only valid while PGresult lives!
        return as_sv();
    }
    std::span<const uint8_t> FieldView::decode_binary_span() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_bytea) {
            // Binary format bytea - direct span
            return {reinterpret_cast<const uint8_t*>(ptr_), static_cast<size_t>(len_)};
        }
        // Text format bytea needs hex decoding - can't return span
        throw std::runtime_error("Cannot decode text-format bytea as span. Use vector<uint8_t> instead.");
    }
    std::vector<uint8_t> FieldView::decode_binary_vector() const {
        if (format_ == FormatRegistry::binary && oid_ == TypeRegistry::oid_bytea) {
            // Binary format - direct copy
            return {reinterpret_cast<const uint8_t*>(ptr_), reinterpret_cast<const uint8_t*>(ptr_) + len_};
        }
        // Text format - need to decode hex
        return decode_hex_bytea();
    }
    std::vector<uint8_t> FieldView::decode_hex_bytea() const {
        auto sv = as_sv();
        if (sv.size() >= 2 && sv[0] == '\\' && sv[1] == 'x') {
            // PostgreSQL hex format: \xDEADBEEF
            sv.remove_prefix(2);
            std::vector<uint8_t> result;
            result.reserve(sv.size() / 2);

            for (size_t i = 0; i < sv.size(); i += 2) {
                uint8_t byte;
                auto [ptr, ec] = std::from_chars(sv.data() + i, sv.data() + i + 2, byte, 16);
                if (ec != std::errc{}) {
                    throw std::runtime_error("Invalid hex in bytea");
                }
                result.push_back(byte);
            }
            return result;
        }
        // Legacy escape format - implement if needed
        throw std::runtime_error("Unsupported bytea format");
    }
}  // namespace demiplane::db::postgres
