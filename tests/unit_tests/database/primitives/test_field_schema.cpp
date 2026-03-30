#include <chrono>
#include <string>

#include <db_core_objects.hpp>
#include <gtest/gtest.h>
#include <schema/db_dynamic_field_schema.hpp>

using namespace demiplane::db;

class FieldSchemaTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(FieldSchemaTest, DynamicFieldSchemaDefaultConstruction) {
    DynamicFieldSchema schema;

    EXPECT_EQ(schema.name, "");
    EXPECT_EQ(schema.db_type, "");
    EXPECT_TRUE(schema.is_nullable);
    EXPECT_FALSE(schema.is_primary_key);
    EXPECT_FALSE(schema.is_foreign_key);
    EXPECT_FALSE(schema.is_unique);
    EXPECT_FALSE(schema.is_indexed);
    EXPECT_EQ(schema.foreign_table, "");
    EXPECT_EQ(schema.foreign_column, "");
    EXPECT_EQ(schema.default_value, "");
    EXPECT_EQ(schema.max_length, 0);
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaParameterizedConstruction) {
    DynamicFieldSchema schema;
    schema.name           = "test_field";
    schema.db_type        = "VARCHAR(100)";
    schema.is_nullable    = false;
    schema.is_primary_key = true;
    schema.max_length     = 100;

    EXPECT_EQ(schema.name, "test_field");
    EXPECT_EQ(schema.db_type, "VARCHAR(100)");
    EXPECT_FALSE(schema.is_nullable);
    EXPECT_TRUE(schema.is_primary_key);
    EXPECT_EQ(schema.max_length, 100);
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaForeignKeySetup) {
    DynamicFieldSchema schema;
    schema.name           = "user_id";
    schema.db_type        = "INTEGER";
    schema.is_foreign_key = true;
    schema.foreign_table  = "users";
    schema.foreign_column = "id";

    EXPECT_TRUE(schema.is_foreign_key);
    EXPECT_EQ(schema.foreign_table, "users");
    EXPECT_EQ(schema.foreign_column, "id");
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaDbAttributes) {
    DynamicFieldSchema schema;
    schema.db_attributes["auto_increment"] = "true";
    schema.db_attributes["comment"]        = "Primary key field";

    EXPECT_EQ(schema.db_attributes.size(), 2);
    EXPECT_EQ(schema.db_attributes["auto_increment"], "true");
    EXPECT_EQ(schema.db_attributes["comment"], "Primary key field");
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaWithComplexTypes) {
    DynamicFieldSchema timestamp_schema;
    timestamp_schema.name          = "created_at";
    timestamp_schema.db_type       = "TIMESTAMP";
    timestamp_schema.is_nullable   = false;
    timestamp_schema.default_value = "CURRENT_TIMESTAMP";

    EXPECT_EQ(timestamp_schema.name, "created_at");
    EXPECT_EQ(timestamp_schema.db_type, "TIMESTAMP");
    EXPECT_FALSE(timestamp_schema.is_nullable);
    EXPECT_EQ(timestamp_schema.default_value, "CURRENT_TIMESTAMP");
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaIndexedAndUnique) {
    DynamicFieldSchema schema;
    schema.name        = "email";
    schema.db_type     = "VARCHAR(255)";
    schema.is_unique   = true;
    schema.is_indexed  = true;
    schema.is_nullable = false;

    EXPECT_TRUE(schema.is_unique);
    EXPECT_TRUE(schema.is_indexed);
    EXPECT_FALSE(schema.is_nullable);
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaCopyAndAssignment) {
    DynamicFieldSchema original;
    original.name                     = "original_field";
    original.db_type                  = "TEXT";
    original.is_nullable              = false;
    original.is_primary_key           = true;
    original.db_attributes["charset"] = "utf8";

    // Copy constructor
    DynamicFieldSchema copy = original;
    EXPECT_EQ(copy.name, original.name);
    EXPECT_EQ(copy.db_type, original.db_type);
    EXPECT_EQ(copy.is_nullable, original.is_nullable);
    EXPECT_EQ(copy.is_primary_key, original.is_primary_key);
    EXPECT_EQ(copy.db_attributes.size(), original.db_attributes.size());

    // Assignment operator
    DynamicFieldSchema assigned;
    assigned = original;
    EXPECT_EQ(assigned.name, original.name);
    EXPECT_EQ(assigned.db_type, original.db_type);
    EXPECT_EQ(assigned.is_nullable, original.is_nullable);
    EXPECT_EQ(assigned.is_primary_key, original.is_primary_key);
}

TEST_F(FieldSchemaTest, DynamicFieldSchemaMultipleAttributes) {
    DynamicFieldSchema schema;
    schema.name                       = "complex_field";
    schema.db_type                    = "DECIMAL(10,2)";
    schema.is_nullable                = true;
    schema.is_indexed                 = true;
    schema.default_value              = "0.00";
    schema.db_attributes["precision"] = "10";
    schema.db_attributes["scale"]     = "2";
    schema.db_attributes["unsigned"]  = "true";

    EXPECT_EQ(schema.name, "complex_field");
    EXPECT_EQ(schema.db_type, "DECIMAL(10,2)");
    EXPECT_TRUE(schema.is_nullable);
    EXPECT_TRUE(schema.is_indexed);
    EXPECT_FALSE(schema.is_primary_key);
    EXPECT_FALSE(schema.is_foreign_key);
    EXPECT_FALSE(schema.is_unique);
    EXPECT_EQ(schema.default_value, "0.00");
    EXPECT_EQ(schema.db_attributes.size(), 3);
}
