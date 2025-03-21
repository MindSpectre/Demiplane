#include <gtest/gtest.h>

#include "chrono_utils.hpp"
#include "db_base.hpp" // Assuming the provided Field class is in "Field.hpp"

using namespace demiplane::database;

TEST(FieldTest, ConstructorAndValueAccess) {
    // Test int
    Field intField("age", 25);
    EXPECT_EQ(intField.get_name(), "age");
    EXPECT_EQ(intField.value(), 25);

    // Test string
    Field<std::string> stringField("name", "John");
    EXPECT_EQ(stringField.get_name(), "name");
    EXPECT_EQ(stringField.value(), "John");

    // Test boolean
    Field boolField("is_active", true);
    EXPECT_EQ(boolField.get_name(), "is_active");
    EXPECT_EQ(boolField.value(), true);

    // Test Uuid
    Uuid uuid("123e4567-e89b-12d3-a456-426614174000", false);
    Field uuidField("id", uuid);
    EXPECT_EQ(uuidField.get_name(), "id");
    EXPECT_EQ(uuidField.value().get_id(), "123e4567-e89b-12d3-a456-426614174000");
}

TEST(FieldTest, SetNameAndValue) {
    Field intField("age", 25);
    intField.set_name("new_age");
    EXPECT_EQ(intField.get_name(), "new_age");

    intField.set_value(30);
    EXPECT_EQ(intField.value(), 30);
}

TEST(FieldTest, ToString) {
    // Test int
    Field intField("age", 25);
    EXPECT_EQ(intField.to_string(), "25");

    // Test string
    Field<std::string> stringField("name", "John");
    EXPECT_EQ(stringField.to_string(), "John");

    // Test boolean
    Field boolField("is_active", true);
    EXPECT_EQ(boolField.to_string(), "TRUE");

    Field boolFieldFalse("is_active", false);
    EXPECT_EQ(boolFieldFalse.to_string(), "FALSE");

    // Test Uuid
    Uuid uuid("123e4567-e89b-12d3-a456-426614174000", false);
    Field uuidField("id", uuid);
    EXPECT_EQ(uuidField.to_string(), "123e4567-e89b-12d3-a456-426614174000");

    // Test chrono::system_clock::time_point
    auto now = std::chrono::system_clock::now();
    Field timeField("timestamp", now);
    EXPECT_EQ(timeField.to_string(), demiplane::utilities::chrono::UTCClock::current_time_ymd());
}

TEST(FieldTest, GetSqlType) {
    // Test int
    const Field intField("age", 25);
    EXPECT_EQ(intField.get_sql_type(), SqlType::INT);

    // Test string
    const Field<std::string> stringField("name", "John");
    EXPECT_EQ(stringField.get_sql_type(), SqlType::TEXT);

    // Test boolean
    const Field boolField("is_active", true);
    EXPECT_EQ(boolField.get_sql_type(), SqlType::BOOLEAN);

    // Test Uuid
    const Uuid uuid("123e4567-e89b-12d3-a456-426614174000", false);
    const Field uuidField("id", uuid);
    EXPECT_EQ(uuidField.get_sql_type(), SqlType::UUID);
}

TEST(FieldTest, GetSqlTypeInitialization) {
    // Test int
    Field intField("age", 25);
    EXPECT_EQ(intField.get_sql_type_initialization(), "INT");

    // Test string
    Field<std::string> stringField("name", "John");
    EXPECT_EQ(stringField.get_sql_type_initialization(), "TEXT");

    // Test boolean
    Field boolField("is_active", true);
    EXPECT_EQ(boolField.get_sql_type_initialization(), "BOOLEAN");

    // Test Uuid
    Uuid primaryUuid("123e4567-e89b-12d3-a456-426614174000", true);
    Field primaryUuidField("id", primaryUuid);
    EXPECT_EQ(primaryUuidField.get_sql_type_initialization(), "UUID DEFAULT gen_random_uuid() PRIMARY KEY");

    Uuid nullableUuid("null", false);
    nullableUuid.make_null();
    Field nullableUuidField("id", nullableUuid);
    EXPECT_EQ(nullableUuidField.get_sql_type_initialization(), "UUID NULL");
}

TEST(FieldTest, Clone) {
    const Field intField("age", 25);
    const auto clonedField = intField.clone();
    EXPECT_EQ(clonedField->get_name(), "age");
    EXPECT_EQ(clonedField->as<int>(), 25);
}

TEST(FieldTest, AsMethod) {
    Field intField("age", 25);
    EXPECT_NO_THROW({
        int value = intField.as<int>();
        EXPECT_EQ(value, 25);
    });

    Field<std::string> stringField("name", "John");
    EXPECT_NO_THROW({
        auto value = stringField.as<std::string>();
        EXPECT_EQ(value, "John");
    });

    // Invalid type
    EXPECT_THROW({
        intField.as<std::string>();
    }, std::runtime_error);
}
// Test for chrono::system_clock::time_point to cover TIMESTAMP handling
TEST(FieldTest, ChronoTimePointToStringRvalue) {
    const auto now = std::chrono::system_clock::now();
    Field timeField("timestamp", now);

    EXPECT_EQ(std::move(timeField).to_string(), demiplane::utilities::chrono::UTCClock::current_time_ymd());
}

// Test for UUID handling in SqlType initialization (UUID NOT NULL)
TEST(FieldTest, UUIDSqlTypeInitialization) {
    // UUID NOT NULL
    const Uuid uuidNotNull("123e4567-e89b-12d3-a456-426614174000", false);
    const Field uuidField("id", uuidNotNull);
    EXPECT_EQ(uuidField.get_sql_type_initialization(), "UUID NOT NULL");
}

// Test for TIMESTAMP SqlType and Initialization
TEST(FieldTest, SqlTypeTimestamp) {
    // TIMESTAMP SqlType
    const Field timeField("timestamp", std::chrono::system_clock::now());
    EXPECT_EQ(timeField.get_sql_type(), SqlType::TIMESTAMP);
    EXPECT_EQ(timeField.get_sql_type_initialization(), "TIMESTAMP");
}

// Test for rvalue `to_string()` for string type
TEST(FieldTest, RValueToStringString) {
    Field<std::string> stringField("name", "RValueTest");
    EXPECT_EQ(std::move(stringField).to_string(), "RValueTest");
}

// Test for rvalue `to_string()` for Uuid type
TEST(FieldTest, RValueToStringUuid) {
    const Uuid uuid("123e4567-e89b-12d3-a456-426614174000", false);
    Field uuidField("id", uuid);
    EXPECT_EQ(std::move(uuidField).to_string(), "123e4567-e89b-12d3-a456-426614174000");
}
TEST(FieldTest, RValueToStringBool) {
    Field boolField("is_active", true);
    EXPECT_EQ(std::move(boolField).to_string(), "TRUE");
}
TEST(FieldTest, RValueToStringInt) {
    Field intField("id", 12);
    EXPECT_EQ(std::move(intField).to_string(), "12");
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
