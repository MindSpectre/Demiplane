#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <typeinfo>

#include <../../../../components/database/core/field/include/db_field.hpp>
#include <../../../../components/database/core/field/include/db_field_schema.hpp>

#include <gtest/gtest.h>

using namespace demiplane::db;

class FieldTest : public ::testing::Test {
protected:
    void SetUp() override {
        int_schema.name        = "id";
        int_schema.db_type     = "INTEGER";
        int_schema.cpp_type    = std::type_index(typeid(int));
        int_schema.is_nullable = false;

        string_schema.name        = "name";
        string_schema.db_type     = "VARCHAR(255)";
        string_schema.cpp_type    = std::type_index(typeid(std::string));
        string_schema.is_nullable = true;

        nullable_int_schema.name        = "nullable_id";
        nullable_int_schema.db_type     = "INTEGER";
        nullable_int_schema.cpp_type    = std::type_index(typeid(int));
        nullable_int_schema.is_nullable = true;
    }

    FieldSchema int_schema;
    FieldSchema string_schema;
    FieldSchema nullable_int_schema;
};

TEST_F(FieldTest, FieldConstruction) {
    const Field field(&int_schema);

    EXPECT_EQ(field.name(), "id");
    EXPECT_EQ(&field.schema(), &int_schema);
    EXPECT_TRUE(field.is_null());  // Default constructed fields are null
}

TEST_F(FieldTest, FieldCopyConstruction) {
    Field original(&int_schema);
    original.set(42);

    const Field copy = original;

    EXPECT_EQ(copy.name(), "id");
    EXPECT_EQ(&copy.schema(), &int_schema);
    EXPECT_FALSE(copy.is_null());
    EXPECT_EQ(copy.get<int>(), 42);
}

TEST_F(FieldTest, FieldMoveConstruction) {
    Field original(&int_schema);
    original.set(42);

    const Field moved = std::move(original);

    EXPECT_EQ(moved.name(), "id");
    EXPECT_EQ(&moved.schema(), &int_schema);
    EXPECT_FALSE(moved.is_null());
    EXPECT_EQ(moved.get<int>(), 42);
}

TEST_F(FieldTest, FieldCopyAssignment) {
    Field original(&int_schema);
    original.set(42);

    Field assigned(&string_schema);
    assigned = original;

    EXPECT_EQ(assigned.name(), "id");
    EXPECT_EQ(&assigned.schema(), &int_schema);
    EXPECT_FALSE(assigned.is_null());
    EXPECT_EQ(assigned.get<int>(), 42);
}

TEST_F(FieldTest, FieldMoveAssignment) {
    Field original(&int_schema);
    original.set(42);

    Field assigned(&string_schema);
    assigned = std::move(original);

    EXPECT_EQ(assigned.name(), "id");
    EXPECT_EQ(&assigned.schema(), &int_schema);
    EXPECT_FALSE(assigned.is_null());
    EXPECT_EQ(assigned.get<int>(), 42);
}

TEST_F(FieldTest, SetAndGetInteger) {
    Field field(&int_schema);

    field.set(123);
    EXPECT_FALSE(field.is_null());
    EXPECT_EQ(field.get<int>(), 123);
}

TEST_F(FieldTest, SetAndGetString) {
    Field field(&string_schema);

    field.set(std::string("Hello World"));
    EXPECT_FALSE(field.is_null());
    EXPECT_EQ(field.get<std::string>(), "Hello World");
}

TEST_F(FieldTest, SetAndGetStringLiteral) {
    Field field(&string_schema);

    field.set("Hello Literal");
    EXPECT_FALSE(field.is_null());
    EXPECT_EQ(field.get<std::string>(), "Hello Literal");
}

TEST_F(FieldTest, SetAndGetMoveSemantics) {
    Field field(&string_schema);

    std::string value = "Move Me";
    field.set(std::move(value));

    EXPECT_FALSE(field.is_null());
    EXPECT_EQ(field.get<std::string>(), "Move Me");
    // Note: value may or may not be empty after move, depends on implementation
}

TEST_F(FieldTest, TryGetValidType) {
    Field field(&int_schema);
    field.set(456);

    const auto result = field.try_get<int>();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 456);
}

TEST_F(FieldTest, TryGetNullField) {
    const Field field(&nullable_int_schema);  // Nullable field, starts as null

    const auto result = field.try_get<int>();
    EXPECT_FALSE(result.has_value());
}

TEST_F(FieldTest, IsNullDefaultConstruction) {
    const Field field(&int_schema);
    EXPECT_TRUE(field.is_null());
}

TEST_F(FieldTest, IsNullAfterSet) {
    Field field(&int_schema);
    field.set(789);
    EXPECT_FALSE(field.is_null());
}

TEST_F(FieldTest, SetBinaryData) {
    Field field(&string_schema);  // Using string schema for binary data

    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0xFF};
    field.set(std::span<const uint8_t>(data));

    EXPECT_FALSE(field.is_null());
    // Note: Getting binary data back would depend on FieldValue implementation
}

TEST_F(FieldTest, RawValueAccess) {
    Field field(&int_schema);
    field.set(999);

    const auto& raw_value = field.raw_value();
    EXPECT_EQ(std::get<int>(raw_value), 999);
    // The exact interface of FieldValue would depend on its implementation
    // This test verifies we can access the raw value without exceptions
    EXPECT_FALSE(field.is_null());
}

TEST_F(FieldTest, SchemaAccess) {
    const Field field(&int_schema);

    const auto& schema = field.schema();
    EXPECT_EQ(&schema, &int_schema);
    EXPECT_EQ(schema.name, "id");
    EXPECT_EQ(schema.db_type, "INTEGER");
}

TEST_F(FieldTest, NameAccess) {
    const Field field(&string_schema);

    EXPECT_EQ(field.name(), "name");
}

TEST_F(FieldTest, MultipleFieldsFromSameSchema) {
    Field field1(&int_schema);
    Field field2(&int_schema);

    field1.set(100);
    field2.set(200);

    EXPECT_EQ(field1.get<int>(), 100);
    EXPECT_EQ(field2.get<int>(), 200);
    EXPECT_EQ(&field1.schema(), &field2.schema());
}

TEST_F(FieldTest, FieldWithDifferentTypes) {
    FieldSchema double_schema;
    double_schema.name     = "price";
    double_schema.db_type  = "DECIMAL(10,2)";
    double_schema.cpp_type = std::type_index(typeid(double));

    Field int_field(&int_schema);
    Field string_field(&string_schema);
    Field double_field(&double_schema);

    int_field.set(42);
    string_field.set("test");
    double_field.set(99.99);

    EXPECT_EQ(int_field.get<int>(), 42);
    EXPECT_EQ(string_field.get<std::string>(), "test");
    EXPECT_EQ(double_field.get<double>(), 99.99);
}

TEST_F(FieldTest, ReassignField) {
    Field field(&int_schema);

    field.set(100);
    EXPECT_EQ(field.get<int>(), 100);

    field.set(200);
    EXPECT_EQ(field.get<int>(), 200);
}

TEST_F(FieldTest, FieldSchemaProperties) {
    EXPECT_FALSE(int_schema.is_nullable);
    EXPECT_TRUE(string_schema.is_nullable);
    EXPECT_TRUE(nullable_int_schema.is_nullable);

    Field non_nullable_field(&int_schema);
    Field nullable_field(&nullable_int_schema);

    // Both fields start as null regardless of schema nullability
    EXPECT_TRUE(non_nullable_field.is_null());
    EXPECT_TRUE(nullable_field.is_null());
}
