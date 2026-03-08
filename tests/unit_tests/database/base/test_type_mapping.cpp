// todo: compiler tests maybe should be removed

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <db_table.hpp>
#include <gtest/gtest.h>
#include <postgres_dialect.hpp>
#include <postgres_type_mapping.hpp>

using namespace demiplane::db;


// COMPILE-TIME API TESTS: sql_type_for<T, Provider>()


TEST(TypeMappingCompileTime, BoolMapsToBoolean) {
    constexpr auto type = sql_type_for<bool, Providers::PostgreSQL>();
    EXPECT_EQ(type, "BOOLEAN");
}

TEST(TypeMappingCompileTime, Int32MapsToInteger) {
    constexpr auto type = sql_type_for<std::int32_t, Providers::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCompileTime, Int64MapsToBigint) {
    constexpr auto type = sql_type_for<std::int64_t, Providers::PostgreSQL>();
    EXPECT_EQ(type, "BIGINT");
}

TEST(TypeMappingCompileTime, FloatMapsToReal) {
    constexpr auto type = sql_type_for<float, Providers::PostgreSQL>();
    EXPECT_EQ(type, "REAL");
}

TEST(TypeMappingCompileTime, DoubleMapsToDoublePrecision) {
    constexpr auto type = sql_type_for<double, Providers::PostgreSQL>();
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST(TypeMappingCompileTime, StringMapsToText) {
    constexpr auto type = sql_type_for<std::string, Providers::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingCompileTime, StringViewMapsToText) {
    constexpr auto type = sql_type_for<std::string_view, Providers::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingCompileTime, ByteVectorMapsToBytea) {
    constexpr auto type = sql_type_for<std::vector<std::uint8_t>, Providers::PostgreSQL>();
    EXPECT_EQ(type, "BYTEA");
}

TEST(TypeMappingCompileTime, ByteSpanMapsToBytea) {
    constexpr auto type = sql_type_for<std::span<const std::uint8_t>, Providers::PostgreSQL>();
    EXPECT_EQ(type, "BYTEA");
}


// RUNTIME API TESTS: sql_type<T>(Providers)


TEST(TypeMappingRuntime, BoolWithProviderEnum) {
    auto type = sql_type<bool>(Providers::PostgreSQL);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST(TypeMappingRuntime, Int32WithProviderEnum) {
    auto type = sql_type<std::int32_t>(Providers::PostgreSQL);
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingRuntime, Int64WithProviderEnum) {
    auto type = sql_type<std::int64_t>(Providers::PostgreSQL);
    EXPECT_EQ(type, "BIGINT");
}

TEST(TypeMappingRuntime, FloatWithProviderEnum) {
    auto type = sql_type<float>(Providers::PostgreSQL);
    EXPECT_EQ(type, "REAL");
}

TEST(TypeMappingRuntime, DoubleWithProviderEnum) {
    auto type = sql_type<double>(Providers::PostgreSQL);
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST(TypeMappingRuntime, StringWithProviderEnum) {
    auto type = sql_type<std::string>(Providers::PostgreSQL);
    EXPECT_EQ(type, "TEXT");
}

TEST(TypeMappingRuntime, ByteVectorWithProviderEnum) {
    auto type = sql_type<std::vector<std::uint8_t>>(Providers::PostgreSQL);
    EXPECT_EQ(type, "BYTEA");
}


// RUNTIME API TESTS: sql_type<T>(dialect)


class TypeMappingDialectTest : public ::testing::Test {
protected:
    // Use Providers enum since PostgresDialect no longer inherits SqlDialect
    Providers provider = Providers::PostgreSQL;
};

TEST_F(TypeMappingDialectTest, BoolWithDialectRef) {
    auto type = sql_type<bool>(provider);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST_F(TypeMappingDialectTest, Int32WithDialectRef) {
    auto type = sql_type<std::int32_t>(provider);
    EXPECT_EQ(type, "INTEGER");
}

TEST_F(TypeMappingDialectTest, Int64WithDialectRef) {
    auto type = sql_type<std::int64_t>(provider);
    EXPECT_EQ(type, "BIGINT");
}

TEST_F(TypeMappingDialectTest, FloatWithDialectRef) {
    auto type = sql_type<float>(provider);
    EXPECT_EQ(type, "REAL");
}

TEST_F(TypeMappingDialectTest, DoubleWithDialectRef) {
    auto type = sql_type<double>(provider);
    EXPECT_EQ(type, "DOUBLE PRECISION");
}

TEST_F(TypeMappingDialectTest, StringWithDialectRef) {
    auto type = sql_type<std::string>(provider);
    EXPECT_EQ(type, "TEXT");
}

TEST_F(TypeMappingDialectTest, ByteVectorWithDialectRef) {
    auto type = sql_type<std::vector<std::uint8_t>>(provider);
    EXPECT_EQ(type, "BYTEA");
}

TEST_F(TypeMappingDialectTest, BoolWithDialectPtr) {
    auto type = sql_type<bool>(provider);
    EXPECT_EQ(type, "BOOLEAN");
}

TEST_F(TypeMappingDialectTest, Int32WithDialectPtr) {
    auto type = sql_type<std::int32_t>(provider);
    EXPECT_EQ(type, "INTEGER");
}

TEST_F(TypeMappingDialectTest, StringWithDialectPtr) {
    auto type = sql_type<std::string>(provider);
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
    // Tests use Providers directly since PostgresDialect no longer inherits SqlDialect
};

TEST_F(TableAddFieldTest, AddFieldWithProviderEnum) {
    DynamicTable table("test_table", Providers::PostgreSQL);
    table.add_field<std::int32_t>("id");
    table.add_field<std::string>("name");
    table.add_field<double>("price");

    EXPECT_EQ(table.field_count(), 3);
    EXPECT_EQ(table.provider(), Providers::PostgreSQL);

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
    DynamicTable table("test_table", Providers::PostgreSQL);
    table.add_field<bool>("active");
    table.add_field<std::int64_t>("count");
    table.add_field<float>("rate");

    EXPECT_EQ(table.field_count(), 3);
    EXPECT_EQ(table.provider(), Providers::PostgreSQL);

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
    DynamicTable table("test_table", Providers::PostgreSQL);
    table.add_field<std::vector<std::uint8_t>>("data");
    table.add_field<std::string_view>("description");

    EXPECT_EQ(table.field_count(), 2);
    EXPECT_EQ(table.provider(), Providers::PostgreSQL);

    const auto* data_field = table.get_field_schema("data");
    ASSERT_NE(data_field, nullptr);
    EXPECT_EQ(data_field->db_type, "BYTEA");

    const auto* desc_field = table.get_field_schema("description");
    ASSERT_NE(desc_field, nullptr);
    EXPECT_EQ(desc_field->db_type, "TEXT");
}

TEST_F(TableAddFieldTest, MixedAddFieldApis) {
    DynamicTable table("test_table", Providers::PostgreSQL);

    // Two ways to add fields: explicit db_type or inferred from provider
    table.add_field<std::int32_t>("id", "SERIAL PRIMARY KEY");  // explicit (backward compat)
    table.add_field<std::string>("name");                       // inferred from provider
    table.add_field<double>("price");                           // inferred from provider
    table.add_field<bool>("active");                            // inferred from provider

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

TEST_F(TableAddFieldTest, AddFieldWithoutProviderThrows) {
    DynamicTable table("test_table");  // No provider set
    EXPECT_EQ(table.provider(), Providers::None);

    // Should throw when trying to infer SQL type without provider
    EXPECT_THROW(table.add_field<std::int32_t>("id"), std::logic_error);
}

TEST_F(TableAddFieldTest, AddFieldWithNullDialectPtr) {
    DynamicTable table("test_table");

    EXPECT_EQ(table.provider(), Providers::None);

    // Should throw because provider is None
    EXPECT_THROW(table.add_field<std::int32_t>("id"), std::logic_error);
}


// CONCEPT TESTS: HasSqlTypeMapping


TEST(TypeMappingConcept, SupportedTypesHaveMapping) {
    static_assert(HasSqlTypeMapping<bool, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<char, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int16_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int32_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::int64_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint16_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint32_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::uint64_t, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<float, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<double, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::string, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::string_view, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::vector<std::uint8_t>, Providers::PostgreSQL>);
    static_assert(HasSqlTypeMapping<std::span<const std::uint8_t>, Providers::PostgreSQL>);
}

TEST(TypeMappingConcept, UnsupportedTypesDoNotHaveMapping) {
    // These should not have mappings for PostgreSQL
    static_assert(!HasSqlTypeMapping<std::vector<int>, Providers::PostgreSQL>);

    // Nothing should have mapping for None provider
    static_assert(!HasSqlTypeMapping<bool, Providers::None>);
    static_assert(!HasSqlTypeMapping<int, Providers::None>);
    static_assert(!HasSqlTypeMapping<std::string, Providers::None>);
}


// CV-QUALIFIER HANDLING TESTS


TEST(TypeMappingCvQualifiers, ConstTypesWork) {
    constexpr auto type = sql_type_for<const int, Providers::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCvQualifiers, VolatileTypesWork) {
    constexpr auto type = sql_type_for<volatile int, Providers::PostgreSQL>();
    EXPECT_EQ(type, "INTEGER");
}

TEST(TypeMappingCvQualifiers, ConstRefTypesWork) {
    constexpr auto type = sql_type_for<const std::string&, Providers::PostgreSQL>();
    EXPECT_EQ(type, "TEXT");
}
