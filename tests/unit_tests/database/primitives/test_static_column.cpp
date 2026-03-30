#include <string>

#include <db_typed_column.hpp>
#include <gtest/gtest.h>

using namespace demiplane::db;

TEST(TypedColumnTest, Construction) {
    TypedColumn<int> col{"id", "users"};

    static_assert(std::is_same_v<typename decltype(col)::value_type, int>);
    EXPECT_EQ(col.name(), "id");
    EXPECT_EQ(col.table_name(), "users");
    EXPECT_TRUE(col.alias().empty());
}

TEST(TypedColumnTest, NameIsRuntime) {
    TypedColumn<int> col{"id", "users"};
    EXPECT_EQ(col.name(), "id");
}

TEST(TypedColumnTest, Aliasing) {
    TypedColumn<int> col{"id", "users"};
    auto aliased = col.as("user_id");

    EXPECT_EQ(aliased.alias(), "user_id");
    EXPECT_EQ(aliased.name(), "id");
    EXPECT_EQ(aliased.table_name(), "users");
}

TEST(TypedColumnTest, IsTypedColumnConcept) {
    static_assert(IsTypedColumn<TypedColumn<int>>);
    static_assert(IsTypedColumn<TypedColumn<int>>);  // backwards-compat alias
    static_assert(!IsTypedColumn<int>);
    static_assert(!IsTypedColumn<std::string>);
}
