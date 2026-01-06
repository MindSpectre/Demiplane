#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <db_table.hpp>
#include <gtest/gtest.h>
#include <pg_sql_type_mapping.hpp>
#include <postgres_dialect.hpp>

using namespace demiplane::db;


// COMPILE-TIME API TESTS: sql_type_for<T, Provider>()


TEST(TypeMappingCompileTime, BoolMapsToBoolean) {
    constexpr auto type = sql_type_for<bool, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "BOOLEAN");
}

TEST(TypeMappingCompileTime, Int32MapsToInteger) {
    constexpr auto type = sql_type_for<std::int32_t, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCompileTime, Int64MapsToBigint) {
    constexpr auto type = sql_type_for<std::int64_t, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "BIGINT");
}

TEST(TypeMappingCompileTime, FloatMapsToReal) {
    constexpr auto type = sql_type_for<float, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "REAL");
}

TEST(TypeMappingCompileTime, DoubleMapsToDoublePrecision) {
    constexpr auto type = sql_type_for<double, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST(TypeMappingCompileTime, StringMapsToText) {
    constexpr auto type = sql_type_for<std::string, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingCompileTime, StringViewMapsToText) {
    constexpr auto type = sql_type_for<std::string_view, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingCompileTime, ByteVectorMapsToBytea) {
    constexpr auto type = sql_type_for<std::vector<std::uint8_t>, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "BYTEA");
}

TEST(TypeMappingCompileTime, ByteSpanMapsToBytea) {
    constexpr auto type = sql_type_for<std::span<const std::uint8_t>, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "BYTEA");
}


// RUNTIME API TESTS: sql_type<T>(SupportedProviders)


TEST(TypeMappingRuntime, BoolWithProviderEnum) {
    auto type = sql_type<bool>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST(TypeMappingRuntime, Int32WithProviderEnum) {
    auto type = sql_type<std::int32_t>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingRuntime, Int64WithProviderEnum) {
    auto type = sql_type<std::int64_t>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "BIGINT");
}

TEST(TypeMappingRuntime, FloatWithProviderEnum) {
    auto type = sql_type<float>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "REAL");
}

TEST(TypeMappingRuntime, DoubleWithProviderEnum) {
    auto type = sql_type<double>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST(TypeMappingRuntime, StringWithProviderEnum) {
    auto type = sql_type<std::string>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingRuntime, ByteVectorWithProviderEnum) {
    auto type = sql_type<std::vector<std::uint8_t>>(SupportedProviders::PostgreSQL);
    EXPECT_EQ(type, "BYTEA");
}


// RUNTIME API TESTS: sql_type<T>(dialect)


class TypeMappingDialectTest : public ::testing::Test {
protected:
    postgres::Dialect dialect;
};

TEST_F(TypeMappingDialectTest, BoolWithDialectRef) {
    auto type = sql_type<bool>(dialect);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST_F(TypeMappingDialectTest, Int32WithDialectRef) {
    auto type = sql_type<std::int32_t>(dialect);
    EXPECT_EQ(type, "INTEGER");
}

TEST_F(TypeMappingDialectTest, Int64WithDialectRef) {
    auto type = sql_type<std::int64_t>(dialect);
    EXPECT_EQ(type, "BIGINT");
}

TEST_F(TypeMappingDialectTest, FloatWithDialectRef) {
    auto type = sql_type<float>(dialect);
    EXPECT_EQ(type, "REAL");
}

TEST_F(TypeMappingDialectTest, DoubleWithDialectRef) {
    auto type = sql_type<double>(dialect);
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST_F(TypeMappingDialectTest, StringWithDialectRef) {
    auto type = sql_type<std::string>(dialect);
    EXPECT_EQ(type, "TEXT");
}

TEST_F(TypeMappingDialectTest, ByteVectorWithDialectRef) {
    auto type = sql_type<std::vector<std::uint8_t>>(dialect);
    EXPECT_EQ(type, "BYTEA");
}

TEST_F(TypeMappingDialectTest, BoolWithDialectPtr) {
    auto type = sql_type<bool>(&dialect);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST_F(TypeMappingDialectTest, Int32WithDialectPtr) {
    auto type = sql_type<std::int32_t>(&dialect);
    EXPECT_EQ(type, "INTEGER");
}

TEST_F(TypeMappingDialectTest, StringWithDialectPtr) {
    auto type = sql_type<std::string>(&dialect);
    EXPECT_EQ(type, "TEXT");
}


// CONVENIENCE API TESTS: postgres::sql_type_for<T>()


TEST(PostgresConvenienceApi, BoolMapsToBoolean) {
    constexpr auto type = postgres::sql_type_for<bool>();
    EXPECT_EQ(type, "BOOLEAN");
}

TEST(PostgresConvenienceApi, Int32MapsToInteger) {
    constexpr auto type = postgres::sql_type_for<std::int32_t>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(PostgresConvenienceApi, DoubleMapsToDoublePrecision) {
    constexpr auto type = postgres::sql_type_for<double>();
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST(PostgresConvenienceApi, StringMapsToText) {
    constexpr auto type = postgres::sql_type_for<std::string>();
    EXPECT_EQ(type, "TEXT");
}


// TABLE ADD_FIELD INTEGRATION TESTS


class TableAddFieldTest : public ::testing::Test {
protected:
    postgres::Dialect dialect;
};

TEST_F(TableAddFieldTest, AddFieldWithProviderEnum) {
    Table table("test_table");
    table.add_field<std::int32_t>("id", SupportedProviders::PostgreSQL);
    table.add_field<std::string>("name", SupportedProviders::PostgreSQL);
    table.add_field<double>("price", SupportedProviders::PostgreSQL);

    EXPECT_EQ(table.field_count(), 3);

    const auto* id_field = table.get_field_schema("id");
    ASSERT_NE(id_field, nullptr);
    EXPECT_EQ(id_field->db_type, "INTEGER");

    const auto* name_field = table.get_field_schema("name");
    ASSERT_NE(name_field, nullptr);
    EXPECT_EQ(name_field->db_type, "TEXT");

    const auto* price_field = table.get_field_schema("price");
    ASSERT_NE(price_field, nullptr);
    EXPECT_EQ(price_field->db_type, "DOUBLE PRECISION");
}

TEST_F(TableAddFieldTest, AddFieldWithDialectRef) {
    Table table("test_table");
    table.add_field<bool>("active", dialect);
    table.add_field<std::int64_t>("count", dialect);
    table.add_field<float>("rate", dialect);

    EXPECT_EQ(table.field_count(), 3);

    const auto* active_field = table.get_field_schema("active");
    ASSERT_NE(active_field, nullptr);
    EXPECT_EQ(active_field->db_type, "BOOLEAN");

    const auto* count_field = table.get_field_schema("count");
    ASSERT_NE(count_field, nullptr);
    EXPECT_EQ(count_field->db_type, "BIGINT");

    const auto* rate_field = table.get_field_schema("rate");
    ASSERT_NE(rate_field, nullptr);
    EXPECT_EQ(rate_field->db_type, "REAL");
}

TEST_F(TableAddFieldTest, AddFieldWithDialectPtr) {
    Table table("test_table");
    table.add_field<std::vector<std::uint8_t>>("data", &dialect);
    table.add_field<std::string_view>("description", &dialect);

    EXPECT_EQ(table.field_count(), 2);

    const auto* data_field = table.get_field_schema("data");
    ASSERT_NE(data_field, nullptr);
    EXPECT_EQ(data_field->db_type, "BYTEA");

    const auto* desc_field = table.get_field_schema("description");
    ASSERT_NE(desc_field, nullptr);
    EXPECT_EQ(desc_field->db_type, "TEXT");
}

TEST_F(TableAddFieldTest, MixedAddFieldApis) {
    Table table("test_table");

    // All three ways to add fields
    table.add_field<std::int32_t>("id", "SERIAL PRIMARY KEY");        // explicit (backward compat)
    table.add_field<std::string>("name", dialect);                    // dialect ref
    table.add_field<double>("price", &dialect);                       // dialect ptr
    table.add_field<bool>("active", SupportedProviders::PostgreSQL);  // enum

    EXPECT_EQ(table.field_count(), 4);

    const auto* id_field = table.get_field_schema("id");
    ASSERT_NE(id_field, nullptr);
    EXPECT_EQ(id_field->db_type, "SERIAL PRIMARY KEY");  // explicit type preserved

    const auto* name_field = table.get_field_schema("name");
    ASSERT_NE(name_field, nullptr);
    EXPECT_EQ(name_field->db_type, "TEXT");  // inferred

    const auto* price_field = table.get_field_schema("price");
    ASSERT_NE(price_field, nullptr);
    EXPECT_EQ(price_field->db_type, "DOUBLE PRECISION");  // inferred

    const auto* active_field = table.get_field_schema("active");
    ASSERT_NE(active_field, nullptr);
    EXPECT_EQ(active_field->db_type, "BOOLEAN");  // inferred
}


// CONCEPT TESTS: HasSqlTypeMapping


TEST(TypeMappingConcept, SupportedTypesHaveMapping) {
    static_assert(HasSqlTypeMapping<bool, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<char, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int16_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int32_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int64_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint16_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint32_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint64_t, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<float, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<double, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::string, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::string_view, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::vector<std::uint8_t>, SupportedProviders::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::span<const std::uint8_t>, SupportedProviders::PostgreSQL>);
}

TEST(TypeMappingConcept, UnsupportedTypesDoNotHaveMapping) {
    // These should not have mappings for PostgreSQL
    static_assert(!HasSqlTypeMapping<std::vector<int>, SupportedProviders::PostgreSQL>);

    // Nothing should have mapping for None provider
    static_assert(!HasSqlTypeMapping<bool, SupportedProviders::None>);
    static_assert(!HasSqlTypeMapping<int, SupportedProviders::None>);
    static_assert(!HasSqlTypeMapping<std::string, SupportedProviders::None>);
}


// CV-QUALIFIER HANDLING TESTS


TEST(TypeMappingCvQualifiers, ConstTypesWork) {
    constexpr auto type = sql_type_for<const int, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCvQualifiers, VolatileTypesWork) {
    constexpr auto type = sql_type_for<volatile int, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCvQualifiers, ConstRefTypesWork) {
    constexpr auto type = sql_type_for<const std::string&, SupportedProviders::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}
