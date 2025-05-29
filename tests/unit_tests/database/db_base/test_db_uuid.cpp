#include <gtest/gtest.h>

#include "db_base.hpp" // Update the path to the Uuid header file

using namespace demiplane::database;

TEST(UuidTest, DefaultConstructor) {
    const Uuid uuid;
    EXPECT_EQ(uuid.get_id(), Uuid::use_generated);
    EXPECT_TRUE(uuid.is_generated());
    EXPECT_FALSE(uuid.is_null());
}

TEST(UuidTest, SetNull) {
    Uuid uuid;
    uuid.set_null();
    EXPECT_EQ(uuid.get_id(), Uuid::null_value);
    EXPECT_TRUE(uuid.is_null());
    EXPECT_FALSE(uuid.is_primary());
    EXPECT_FALSE(uuid.is_generated());
}

TEST(UuidTest, SetDefault) {
    Uuid uuid;
    uuid.set_generated();
    EXPECT_EQ(uuid.get_id(), Uuid::use_generated);
    EXPECT_TRUE(uuid.is_generated());
    EXPECT_FALSE(uuid.is_null());
}

TEST(UuidTest, SetId) {
    Uuid uuid;
    constexpr std::string_view id = "123e4567-e89b-12d3-a456-426614174000";
    uuid.set_id(id);
    EXPECT_EQ(uuid.get_id(), id);
    EXPECT_FALSE(uuid.is_null());
    EXPECT_FALSE(uuid.is_generated());
    EXPECT_TRUE(uuid.is_primary());
}

TEST(UuidTest, EqualityOperators) {
    const Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", true);
    const Uuid uuid2("123e4567-e89b-12d3-a456-426614174000", false);
    const Uuid uuid3("00000000-0000-0000-0000-000000000000", true);

    EXPECT_TRUE(uuid1 == uuid2);
    EXPECT_FALSE(uuid1 == uuid3);
    EXPECT_TRUE(uuid1 != uuid3);
}

TEST(UuidTest, SetPrimary) {
    Uuid uuid("123e4567-e89b-12d3-a456-426614174000", false);
    EXPECT_FALSE(uuid.is_primary());
    uuid.set_primary();
    EXPECT_TRUE(uuid.is_primary());
}

TEST(UuidTest, ConversionOperators) {
    Uuid uuid("123e4567-e89b-12d3-a456-426614174000", true);
    const auto id = uuid.pull_id();
    EXPECT_EQ(id, "123e4567-e89b-12d3-a456-426614174000");
}

TEST(UuidTest, ComparisonOperators) {
    const Uuid uuid1("123e4567-e89b-12d3-a456-426614174000", true);
    const Uuid uuid2("223e4567-e89b-12d3-a456-426614174000", false);

    EXPECT_TRUE(uuid1 < uuid2);
    EXPECT_FALSE(uuid1 > uuid2);
    EXPECT_TRUE(uuid1 <= uuid2);
    EXPECT_FALSE(uuid1 >= uuid2);
}

TEST(UuidTest, StreamOperator) {
    const Uuid uuid("123e4567-e89b-12d3-a456-426614174000", true);
    std::ostringstream os;
    os << uuid;
    EXPECT_EQ(os.str(), "123e4567-e89b-12d3-a456-426614174000");
}
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}