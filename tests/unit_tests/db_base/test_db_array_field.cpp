#include <gtest/gtest.h>

#include "db_base.hpp" // Include the provided ArrayField class here

using namespace demiplane::database;

// Constructor and Value Access
TEST(ArrayFieldTest, ConstructorAndValueAccess) {
    // Test int array
    Field<std::vector<int>> intArrayField("intArray", {1, 2, 3});
    EXPECT_EQ(intArrayField.get_name(), "intArray");
    EXPECT_EQ(intArrayField.value(), (std::vector{1, 2, 3}));

    // Test string array
    Field<std::vector<std::string>> stringArrayField("stringArray", {"a", "b", "c"});
    EXPECT_EQ(stringArrayField.get_name(), "stringArray");
    EXPECT_EQ(stringArrayField.value(), (std::vector<std::string>{"a", "b", "c"}));

    // Test Uuid array
    Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", false);
    Uuid uuid2("223e4567-e89b-12d3-a456-426614174001", false);
    Field<std::vector<Uuid>> uuidArrayField("uuidArray", {uuid1, uuid2});
    EXPECT_EQ(uuidArrayField.get_name(), "uuidArray");
    EXPECT_EQ(uuidArrayField.value()[0].get_id(), "123e4567-e89b-12d3-a456-426614174000");
    EXPECT_EQ(uuidArrayField.value()[1].get_id(), "223e4567-e89b-12d3-a456-426614174001");
}

// Set Name and Value
TEST(ArrayFieldTest, SetNameAndValue) {
    Field<std::vector<int>> intArrayField("arrayField", {1, 2, 3});
    intArrayField.set_name("newArrayField");
    EXPECT_EQ(intArrayField.get_name(), "newArrayField");

    intArrayField.set_value({4, 5, 6});
    EXPECT_EQ(intArrayField.value(), (std::vector<int>{4, 5, 6}));
}

// to_string Tests
TEST(ArrayFieldTest, ToString) {
    // Test int array
    Field<std::vector<int>> intArrayField("intArray", {1, 2, 3});
    EXPECT_EQ(intArrayField.to_string(), "ARRAY[1, 2, 3]");

    // Test string array
    Field<std::vector<std::string>> stringArrayField("stringArray", {"a", "b", "c"});
    EXPECT_EQ(stringArrayField.to_string(), "ARRAY[a, b, c]");

    // Test boolean array
    Field<std::vector<bool>> boolArrayField("boolArray", {true, false, true});
    EXPECT_EQ(boolArrayField.to_string(), "ARRAY[TRUE, FALSE, TRUE]");

    // Test Uuid array
    Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", false);
    Uuid uuid2("223e4567-e89b-12d3-a456-426614174001", false);
    Field<std::vector<Uuid>> uuidArrayField("uuidArray", {uuid1, uuid2});
    EXPECT_EQ(uuidArrayField.to_string(),
        "ARRAY[123e4567-e89b-12d3-a456-426614174000, 223e4567-e89b-12d3-a456-426614174001]");
}

// get_sql_type Tests
TEST(ArrayFieldTest, GetSqlType) {
    // Test int array
    const Field<std::vector<int>> intArrayField("intArray", {1, 2, 3});
    EXPECT_EQ(intArrayField.get_sql_type(), SqlType::ARRAY_INT);

    // Test string array
    const Field<std::vector<std::string>> stringArrayField("stringArray", {"a", "b", "c"});
    EXPECT_EQ(stringArrayField.get_sql_type(), SqlType::ARRAY_TEXT);

    // Test Uuid array
    Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", false);
    Uuid uuid2("223e4567-e89b-12d3-a456-426614174001", false);
    const Field<std::vector<Uuid>> uuidArrayField("uuidArray", {uuid1, uuid2});
    EXPECT_EQ(uuidArrayField.get_sql_type(), SqlType::ARRAY_UUID);
}

// get_sql_type_initialization Tests
TEST(ArrayFieldTest, GetSqlTypeInitialization) {
    // Test int array
    const Field<std::vector<int>> intArrayField("intArray", {1, 2, 3});
    EXPECT_EQ(intArrayField.get_sql_type_initialization(), "INT[]");

    // Test string array
    const Field<std::vector<std::string>> stringArrayField("stringArray", {"a", "b", "c"});
    EXPECT_EQ(stringArrayField.get_sql_type_initialization(), "TEXT[]");

    // Test Uuid array
    Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", false);
    const Field<std::vector<Uuid>> uuidArrayField("uuidArray", {uuid1});
    EXPECT_EQ(uuidArrayField.get_sql_type_initialization(), "UUID[] NULL");
}

// Clone Tests
TEST(ArrayFieldTest, Clone) {
    const Field<std::vector<int>> intArrayField("intArray", {1, 2, 3});
    const auto clonedField = intArrayField.clone();
    EXPECT_EQ(clonedField->get_name(), "intArray");
    EXPECT_EQ(clonedField->as<std::vector<int>>(), (std::vector{1, 2, 3}));
}

// Unsupported Type Handling
struct UnsupportedType {};

TEST(ArrayFieldTest, UnsupportedTypeHandling) {
    // Unsupported type throws at compile time (commented for demonstration)
    // Field<std::vector<UnsupportedType> unsupportedField("unsupportedArray", {});
    // EXPECT_EQ(unsupportedField.get_sql_type(), SqlType::UNSUPPORTED);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
