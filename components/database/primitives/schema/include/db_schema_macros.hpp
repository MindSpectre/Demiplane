#pragma once

#include <gears_macros.hpp>

// ═══════════════════════════════════════════════════════════════════════════
// DB_ENTITY - Define a database entity schema in one line
// ═══════════════════════════════════════════════════════════════════════════
//
// Usage:
//   struct Customer {
//       int id;
//       std::string name;
//       bool active;
//
//       DB_ENTITY(Customer, "customers", id, name, active);
//   };
//
//   auto table = Table::make<Customer::Schema>();
//   auto col = table->column(Customer::Schema::id);  // TableColumn<int>
//
// ═══════════════════════════════════════════════════════════════════════════

// clang-format off

// --- Internal implementation (do not use directly) ---

#define DMP_DB_EXPAND_(x) x
#define DMP_DB_NARG_(...) DMP_DB_EXPAND_(DMP_DB_ARG_N_(__VA_ARGS__))
#define DMP_DB_NARG(...) DMP_DB_NARG_(__VA_ARGS__, 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define DMP_DB_ARG_N_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N

#define DMP_DB_FIELD_(f) static constexpr auto f = ::demiplane::db::field<::demiplane::gears::FixedString{#f}>(&Self::f)
#define DMP_DB_TYPE_(f) decltype(f)

// For-each with semicolons (statements)
#define DMP_DB_FE_1_(M, a)       M(a)
#define DMP_DB_FE_2_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_1_(M, __VA_ARGS__))
#define DMP_DB_FE_3_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_2_(M, __VA_ARGS__))
#define DMP_DB_FE_4_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_3_(M, __VA_ARGS__))
#define DMP_DB_FE_5_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_4_(M, __VA_ARGS__))
#define DMP_DB_FE_6_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_5_(M, __VA_ARGS__))
#define DMP_DB_FE_7_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_6_(M, __VA_ARGS__))
#define DMP_DB_FE_8_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_7_(M, __VA_ARGS__))
#define DMP_DB_FE_9_(M, a, ...)  M(a); DMP_DB_EXPAND_(DMP_DB_FE_8_(M, __VA_ARGS__))
#define DMP_DB_FE_10_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_9_(M, __VA_ARGS__))
#define DMP_DB_FE_11_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_10_(M, __VA_ARGS__))
#define DMP_DB_FE_12_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_11_(M, __VA_ARGS__))
#define DMP_DB_FE_13_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_12_(M, __VA_ARGS__))
#define DMP_DB_FE_14_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_13_(M, __VA_ARGS__))
#define DMP_DB_FE_15_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_14_(M, __VA_ARGS__))
#define DMP_DB_FE_16_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_15_(M, __VA_ARGS__))
#define DMP_DB_FE_17_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_16_(M, __VA_ARGS__))
#define DMP_DB_FE_18_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_17_(M, __VA_ARGS__))
#define DMP_DB_FE_19_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_18_(M, __VA_ARGS__))
#define DMP_DB_FE_20_(M, a, ...) M(a); DMP_DB_EXPAND_(DMP_DB_FE_19_(M, __VA_ARGS__))
#define DMP_DB_FOR_EACH_(M, ...) DMP_DB_EXPAND_(CONCAT(DMP_DB_FE_, CONCAT(DMP_DB_NARG(__VA_ARGS__), _))(M, __VA_ARGS__))

// For-each with commas (type lists)
#define DMP_DB_TL_1_(M, a)       M(a)
#define DMP_DB_TL_2_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_1_(M, __VA_ARGS__))
#define DMP_DB_TL_3_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_2_(M, __VA_ARGS__))
#define DMP_DB_TL_4_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_3_(M, __VA_ARGS__))
#define DMP_DB_TL_5_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_4_(M, __VA_ARGS__))
#define DMP_DB_TL_6_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_5_(M, __VA_ARGS__))
#define DMP_DB_TL_7_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_6_(M, __VA_ARGS__))
#define DMP_DB_TL_8_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_7_(M, __VA_ARGS__))
#define DMP_DB_TL_9_(M, a, ...)  M(a), DMP_DB_EXPAND_(DMP_DB_TL_8_(M, __VA_ARGS__))
#define DMP_DB_TL_10_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_9_(M, __VA_ARGS__))
#define DMP_DB_TL_11_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_10_(M, __VA_ARGS__))
#define DMP_DB_TL_12_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_11_(M, __VA_ARGS__))
#define DMP_DB_TL_13_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_12_(M, __VA_ARGS__))
#define DMP_DB_TL_14_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_13_(M, __VA_ARGS__))
#define DMP_DB_TL_15_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_14_(M, __VA_ARGS__))
#define DMP_DB_TL_16_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_15_(M, __VA_ARGS__))
#define DMP_DB_TL_17_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_16_(M, __VA_ARGS__))
#define DMP_DB_TL_18_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_17_(M, __VA_ARGS__))
#define DMP_DB_TL_19_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_18_(M, __VA_ARGS__))
#define DMP_DB_TL_20_(M, a, ...) M(a), DMP_DB_EXPAND_(DMP_DB_TL_19_(M, __VA_ARGS__))
#define DMP_DB_TYPE_LIST_(M, ...) DMP_DB_EXPAND_(CONCAT(DMP_DB_TL_, CONCAT(DMP_DB_NARG(__VA_ARGS__), _))(M, __VA_ARGS__))

// clang-format on

// --- Public API ---

#define DB_ENTITY(ClassName, table, ...)                                                                               \
    struct Schema {                                                                                                    \
        using Self                                   = ClassName;                                                      \
        static constexpr std::string_view table_name = table;                                                          \
        DMP_DB_FOR_EACH_(DMP_DB_FIELD_, __VA_ARGS__);                                                                  \
        using fields = ::demiplane::gears::type_list<DMP_DB_TYPE_LIST_(DMP_DB_TYPE_, __VA_ARGS__)>;                    \
    }
