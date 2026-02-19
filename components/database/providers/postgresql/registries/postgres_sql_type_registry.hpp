#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace demiplane::db::postgres {

    namespace detail {

        // Helper to compute number of digits for a compile-time integer
        template <std::size_t N>
        constexpr std::size_t digit_count() {
            if constexpr (N < 10)
                return 1;
            else if constexpr (N < 100)
                return 2;
            else if constexpr (N < 1000)
                return 3;
            else if constexpr (N < 10000)
                return 4;
            else
                return 5;
        }

        // Generic storage for TYPE(N) patterns
        template <std::size_t PrefixLen, std::size_t N, char... Prefix>
        struct SingleParamStorage {
            static_assert(N <= 99999, "Parameter value too large");

            static constexpr std::size_t length = PrefixLen + 1 + digit_count<N>() + 1;  // PREFIX + ( + digits + )

            static constexpr auto value = []() {
                std::array<char, 32> result{};
                std::size_t pos = 0;

                for (const char c : {Prefix...}) {
                    result[pos++] = c;
                }
                result[pos++] = '(';

                // Write digits
                if constexpr (N >= 10000)
                    result[pos++] = static_cast<char>('0' + (N / 10000) % 10);
                if constexpr (N >= 1000)
                    result[pos++] = static_cast<char>('0' + (N / 1000) % 10);
                if constexpr (N >= 100)
                    result[pos++] = static_cast<char>('0' + (N / 100) % 10);
                if constexpr (N >= 10)
                    result[pos++] = static_cast<char>('0' + (N / 10) % 10);
                result[pos++] = static_cast<char>('0' + N % 10);

                result[pos++] = ')';
                return result;
            }();
        };

        // CHAR(N)
        template <std::size_t N>
        using CharStorage = SingleParamStorage<4, N, 'C', 'H', 'A', 'R'>;

        // VARCHAR(N)
        template <std::size_t N>
        using VarcharStorage = SingleParamStorage<7, N, 'V', 'A', 'R', 'C', 'H', 'A', 'R'>;

        // BIT(N)
        template <std::size_t N>
        using BitStorage = SingleParamStorage<3, N, 'B', 'I', 'T'>;

        // VARBIT(N)
        template <std::size_t N>
        using VarbitStorage = SingleParamStorage<6, N, 'V', 'A', 'R', 'B', 'I', 'T'>;

        // TIME(P)
        template <std::size_t P>
        using TimeStorage = SingleParamStorage<4, P, 'T', 'I', 'M', 'E'>;

        // TIMESTAMP(P)
        template <std::size_t P>
        using TimestampStorage = SingleParamStorage<9, P, 'T', 'I', 'M', 'E', 'S', 'T', 'A', 'M', 'P'>;

        // TIMESTAMPTZ(P) - TIMESTAMP(P) WITH TIME ZONE
        template <std::size_t P>
        struct TimestampTzStorage {
            static_assert(P <= 6, "TIMESTAMPTZ precision must be 0-6");
            static constexpr std::size_t length         = 30;  // "TIMESTAMP(P) WITH TIME ZONE"
            static constexpr std::array<char, 32> value = {
                'T', 'I', 'M', 'E', 'S', 'T', 'A', 'M', 'P', '(', static_cast<char>('0' + P),
                ')', ' ', 'W', 'I', 'T', 'H', ' ', 'T', 'I', 'M', 'E',
                ' ', 'Z', 'O', 'N', 'E'};
        };

        // INTERVAL FIELD(P)
        template <std::size_t P>
        using IntervalStorage = SingleParamStorage<8, P, 'I', 'N', 'T', 'E', 'R', 'V', 'A', 'L'>;

        // NUMERIC(P,S)
        template <std::size_t Precision, std::size_t Scale>
        struct NumericStorage {
            static_assert(Precision <= 99 && Scale <= 99, "NUMERIC supports up to 2 digits for precision/scale");

            static constexpr std::size_t length = 8 + digit_count<Precision>() + 1 + digit_count<Scale>() + 1;

            static constexpr auto value = []() {
                std::array<char, 16> result{};
                std::size_t pos = 0;

                for (const char c : {'N', 'U', 'M', 'E', 'R', 'I', 'C', '('}) {
                    result[pos++] = c;
                }

                if constexpr (Precision >= 10) {
                    result[pos++] = static_cast<char>('0' + (Precision / 10));
                }
                result[pos++] = static_cast<char>('0' + (Precision % 10));

                result[pos++] = ',';

                if constexpr (Scale >= 10) {
                    result[pos++] = static_cast<char>('0' + (Scale / 10));
                }
                result[pos++] = static_cast<char>('0' + (Scale % 10));

                result[pos++] = ')';

                return result;
            }();
        };

    }  // namespace detail

    struct SqlTypeRegistry {
        // Numeric types

        constexpr static std::string_view smallint         = "SMALLINT";
        constexpr static std::string_view integer          = "INTEGER";
        constexpr static std::string_view bigint           = "BIGINT";
        constexpr static std::string_view real             = "REAL";
        constexpr static std::string_view double_precision = "DOUBLE PRECISION";
        constexpr static std::string_view smallserial      = "SMALLSERIAL";
        constexpr static std::string_view serial           = "SERIAL";
        constexpr static std::string_view bigserial        = "BIGSERIAL";
        constexpr static std::string_view money            = "MONEY";


        // Character types

        constexpr static std::string_view text = "TEXT";


        // Binary types

        constexpr static std::string_view bytea = "BYTEA";


        // Boolean type

        constexpr static std::string_view boolean = "BOOLEAN";


        // Date/Time types

        constexpr static std::string_view date        = "DATE";
        constexpr static std::string_view time        = "TIME";
        constexpr static std::string_view timetz      = "TIME WITH TIME ZONE";
        constexpr static std::string_view timestamp   = "TIMESTAMP";
        constexpr static std::string_view timestamptz = "TIMESTAMP WITH TIME ZONE";
        constexpr static std::string_view interval    = "INTERVAL";


        // UUID type

        constexpr static std::string_view uuid = "UUID";


        // JSON types

        constexpr static std::string_view json  = "JSON";
        constexpr static std::string_view jsonb = "JSONB";


        // Network types

        constexpr static std::string_view inet    = "INET";
        constexpr static std::string_view cidr    = "CIDR";
        constexpr static std::string_view macaddr = "MACADDR";


        // Other types

        constexpr static std::string_view xml = "XML";
        constexpr static std::string_view oid = "OID";


        // Parameterized types - compile-time generation


        // CHAR(N) - fixed-length character
        template <std::size_t N>
        static constexpr std::string_view char_type() {
            return {detail::CharStorage<N>::value.data(), detail::CharStorage<N>::length};
        }

        // VARCHAR(N) - variable-length character with limit
        template <std::size_t N>
        static constexpr std::string_view varchar() {
            return {detail::VarcharStorage<N>::value.data(), detail::VarcharStorage<N>::length};
        }

        // NUMERIC(P,S) - exact numeric with precision and scale
        template <std::size_t Precision, std::size_t Scale>
        static constexpr std::string_view numeric() {
            return {detail::NumericStorage<Precision, Scale>::value.data(),
                    detail::NumericStorage<Precision, Scale>::length};
        }

        // BIT(N) - fixed-length bit string
        template <std::size_t N>
        static constexpr std::string_view bit() {
            return {detail::BitStorage<N>::value.data(), detail::BitStorage<N>::length};
        }

        // VARBIT(N) / BIT VARYING(N) - variable-length bit string
        template <std::size_t N>
        static constexpr std::string_view varbit() {
            return {detail::VarbitStorage<N>::value.data(), detail::VarbitStorage<N>::length};
        }

        // TIME(P) - time with fractional seconds precision
        template <std::size_t P>
        static constexpr std::string_view time_p() {
            static_assert(P <= 6, "TIME precision must be 0-6");
            return {detail::TimeStorage<P>::value.data(), detail::TimeStorage<P>::length};
        }

        // TIMESTAMP(P) - timestamp with fractional seconds precision
        template <std::size_t P>
        static constexpr std::string_view timestamp_p() {
            static_assert(P <= 6, "TIMESTAMP precision must be 0-6");
            return {detail::TimestampStorage<P>::value.data(), detail::TimestampStorage<P>::length};
        }

        // TIMESTAMP(P) WITH TIME ZONE
        template <std::size_t P>
        static constexpr std::string_view timestamptz_p() {
            return {detail::TimestampTzStorage<P>::value.data(), detail::TimestampTzStorage<P>::length};
        }
    };

}  // namespace demiplane::db::postgres
