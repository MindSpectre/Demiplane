#include <string>

#include <db_constraints.hpp>
#include <db_table.hpp>
#include <db_typed_column.hpp>
#include <gtest/gtest.h>
#include <schema/db_static_field_schema.hpp>

using namespace demiplane::db;
using namespace demiplane::db::constraints;

using UsersTable = StaticTable<"users",
                               StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                               StaticFieldSchema<std::string, "name">,
                               StaticFieldSchema<int, "age">,
                               StaticFieldSchema<bool, "active", NotNull>>;

TEST(StaticTableTest, FieldCount) {
    UsersTable users{};
    EXPECT_EQ(users.field_count(), 4);
}

TEST(StaticTableTest, TableName) {
    UsersTable users{};
    EXPECT_EQ(users.table_name(), "users");
}

TEST(StaticTableTest, TableNameIsConstexpr) {
    static_assert(UsersTable::table_name() == "users");
}

TEST(StaticTableTest, ColumnByNameDeducesType) {
    UsersTable users{};

    auto id_col = users.column<"id">();
    static_assert(std::is_same_v<typename decltype(id_col)::value_type, int>);
    EXPECT_EQ(id_col.name(), "id");
    EXPECT_EQ(id_col.table_name(), "users");

    auto name_col = users.column<"name">();
    static_assert(std::is_same_v<typename decltype(name_col)::value_type, std::string>);
    EXPECT_EQ(name_col.name(), "name");
}

TEST(StaticTableTest, ColumnByIndex) {
    UsersTable users{};

    auto first = users.column<0>();
    static_assert(std::is_same_v<typename decltype(first)::value_type, int>);
    EXPECT_EQ(first.name(), "id");

    auto second = users.column<1>();
    static_assert(std::is_same_v<typename decltype(second)::value_type, std::string>);
    EXPECT_EQ(second.name(), "name");
}

TEST(StaticTableTest, WithProvider) {
    UsersTable users{Providers::PostgreSQL};
    EXPECT_EQ(users.provider(), Providers::PostgreSQL);
}

TEST(StaticTableTest, ColumnAsColumn) {
    UsersTable users{};
    auto col = users.column<"id">();
    auto dyn = col.as_column();
    EXPECT_EQ(dyn.name(), "id");
    EXPECT_EQ(dyn.table_name(), "users");
}

TEST(StaticTableTest, ForEachField) {
    UsersTable users{};
    std::size_t count = 0;
    users.for_each_field([&]<typename Schema>() { ++count; });
    EXPECT_EQ(count, 4);
}

TEST(StaticTableTest, ForEachFieldConstraints) {
    UsersTable users{};
    bool found_primary = false;
    users.for_each_field([&]<typename Schema>() {
        if constexpr (Schema::is_primary_key) {
            found_primary = true;
            static_assert(Schema::name() == "id");
        }
    });
    EXPECT_TRUE(found_primary);
}

TEST(StaticTableTest, IsStaticTableConcept) {
    static_assert(IsStaticTable<UsersTable>);
    static_assert(!IsStaticTable<int>);
}

TEST(StaticTableTest, ColumnByIndexLastField) {
    UsersTable users{};

    auto last = users.column<3>();
    static_assert(std::is_same_v<typename decltype(last)::value_type, bool>);
    EXPECT_EQ(last.name(), "active");
    EXPECT_EQ(last.table_name(), "users");
}

TEST(StaticTableTest, SingleFieldTable) {
    using SingleTable = StaticTable<"singles", StaticFieldSchema<double, "only_field">>;
    SingleTable t{};

    EXPECT_EQ(t.field_count(), 1);
    EXPECT_EQ(t.table_name(), "singles");

    auto col = t.column<"only_field">();
    static_assert(std::is_same_v<typename decltype(col)::value_type, double>);
    EXPECT_EQ(col.name(), "only_field");
}

TEST(StaticTableTest, StaticConstexprTable) {
    static constexpr UsersTable users{};
    static_assert(UsersTable::table_name() == "users");
    static_assert(users.field_count() == 4);
}
