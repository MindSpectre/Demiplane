#include <string>

#include <gtest/gtest.h>
#include <schema/db_static_field_schema.hpp>

using namespace demiplane::db;
using namespace demiplane::db::constraints;

TEST(StaticFieldSchemaTest, BasicFieldHasCorrectDefaults) {
    using NameField = StaticFieldSchema<std::string, "name">;

    static_assert(std::is_same_v<NameField::value_type, std::string>);
    static_assert(NameField::name() == "name");
    static_assert(!NameField::is_primary_key);
    static_assert(NameField::is_nullable);
    static_assert(!NameField::is_unique);
    static_assert(!NameField::is_indexed);
    static_assert(!NameField::is_foreign_key);
    static_assert(!NameField::has_default);
    static_assert(!NameField::has_max_length);
    static_assert(!NameField::has_db_type);
}

TEST(StaticFieldSchemaTest, PrimaryKeyConstraint) {
    using IdField = StaticFieldSchema<int, "id", PrimaryKey>;

    static_assert(IdField::is_primary_key);
    static_assert(IdField::is_nullable);  // PrimaryKey alone doesn't imply NotNull
}

TEST(StaticFieldSchemaTest, MultipleConstraints) {
    using IdField = StaticFieldSchema<int, "id", PrimaryKey, NotNull, Unique>;

    static_assert(IdField::is_primary_key);
    static_assert(!IdField::is_nullable);
    static_assert(IdField::is_unique);
    static_assert(!IdField::is_indexed);
}

TEST(StaticFieldSchemaTest, NotNullConstraint) {
    using RequiredField = StaticFieldSchema<std::string, "required", NotNull>;

    static_assert(!RequiredField::is_nullable);
    static_assert(!RequiredField::is_primary_key);
}

TEST(StaticFieldSchemaTest, IsStaticFieldSchemaConcept) {
    static_assert(IsStaticFieldSchema<StaticFieldSchema<int, "id">>);
    static_assert(IsStaticFieldSchema<StaticFieldSchema<int, "id", PrimaryKey>>);
    static_assert(!IsStaticFieldSchema<int>);
    static_assert(!IsStaticFieldSchema<std::string>);
}

TEST(StaticFieldSchemaTest, ForeignKeyConstraint) {
    using UserIdField = StaticFieldSchema<int, "user_id", NotNull, ForeignKey<"users", "id">>;

    static_assert(UserIdField::is_foreign_key);
    static_assert(!UserIdField::is_nullable);
    static_assert(UserIdField::foreign_table() == "users");
    static_assert(UserIdField::foreign_column() == "id");
}

TEST(StaticFieldSchemaTest, DefaultValueConstraint) {
    using StatusField = StaticFieldSchema<std::string, "status", Default<"active">>;

    static_assert(StatusField::has_default);
    static_assert(StatusField::default_value() == "active");
    static_assert(StatusField::is_nullable);
}

TEST(StaticFieldSchemaTest, MaxLengthConstraint) {
    using NameField = StaticFieldSchema<std::string, "name", NotNull, MaxLength<255>>;

    static_assert(NameField::has_max_length);
    static_assert(NameField::max_length() == 255);
    static_assert(!NameField::is_nullable);
}

TEST(StaticFieldSchemaTest, DbTypeOverride) {
    using PriceField = StaticFieldSchema<double, "price", DbType<"NUMERIC(10,2)">>;

    static_assert(PriceField::has_db_type);
    static_assert(PriceField::db_type() == "NUMERIC(10,2)");
}

TEST(StaticFieldSchemaTest, AllConstraintsCombined) {
    using Field = StaticFieldSchema<std::string,
                                    "email",
                                    PrimaryKey,
                                    NotNull,
                                    Unique,
                                    Indexed,
                                    ForeignKey<"accounts", "email">,
                                    Default<"unknown">,
                                    MaxLength<320>,
                                    DbType<"VARCHAR(320)">>;

    static_assert(Field::is_primary_key);
    static_assert(!Field::is_nullable);
    static_assert(Field::is_unique);
    static_assert(Field::is_indexed);
    static_assert(Field::is_foreign_key);
    static_assert(Field::foreign_table() == "accounts");
    static_assert(Field::foreign_column() == "email");
    static_assert(Field::has_default);
    static_assert(Field::default_value() == "unknown");
    static_assert(Field::has_max_length);
    static_assert(Field::max_length() == 320);
    static_assert(Field::has_db_type);
    static_assert(Field::db_type() == "VARCHAR(320)");
}

TEST(StaticFieldSchemaTest, ConstraintOrderDoesNotMatter) {
    using Field1 = StaticFieldSchema<int, "x", NotNull, ForeignKey<"t", "c">, Default<"0">>;
    using Field2 = StaticFieldSchema<int, "x", Default<"0">, ForeignKey<"t", "c">, NotNull>;

    static_assert(!Field1::is_nullable);
    static_assert(!Field2::is_nullable);
    static_assert(Field1::is_foreign_key);
    static_assert(Field2::is_foreign_key);
    static_assert(Field1::foreign_table() == Field2::foreign_table());
    static_assert(Field1::default_value() == Field2::default_value());
}
