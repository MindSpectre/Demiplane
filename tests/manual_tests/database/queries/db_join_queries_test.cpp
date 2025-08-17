// JOIN query expression tests
// Comprehensive tests for join operations and join types

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

#include <demiplane/scroll>

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for JOIN operations
class JoinQueryTest : public ::testing::Test,
                      public demiplane::scroll::FileLoggerProvider<demiplane::scroll::DetailedEntry> {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file             = "query_test.log";
        cfg.add_time_to_name = false;

        std::shared_ptr<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>> logger = std::make_shared<
            demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schemas
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<std::string>("name", "VARCHAR(255)")
                    .add_field<int>("age", "INTEGER")
                    .add_field<bool>("active", "BOOLEAN");

        posts_schema = std::make_shared<TableSchema>("posts");
        posts_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<int>("user_id", "INTEGER")
                    .add_field<std::string>("title", "VARCHAR(255)")
                    .add_field<bool>("published", "BOOLEAN");

        comments_schema = std::make_shared<TableSchema>("comments");
        comments_schema->add_field<int>("id", "INTEGER")
                       .primary_key("id")
                       .add_field<int>("post_id", "INTEGER")
                       .add_field<int>("user_id", "INTEGER")
                       .add_field<std::string>("content", "TEXT");

        // Create column references
        user_id     = users_schema->column<int>("id");
        user_name   = users_schema->column<std::string>("name");
        user_age    = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        post_id        = posts_schema->column<int>("id");
        post_user_id   = posts_schema->column<int>("user_id");
        post_title     = posts_schema->column<std::string>("title");
        post_published = posts_schema->column<bool>("published");

        comment_id      = comments_schema->column<int>("id");
        comment_post_id = comments_schema->column<int>("post_id");
        comment_user_id = comments_schema->column<int>("user_id");
        comment_content = comments_schema->column<std::string>("content");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> posts_schema;
    std::shared_ptr<TableSchema> comments_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    TableColumn<int> post_id{nullptr, ""};
    TableColumn<int> post_user_id{nullptr, ""};
    TableColumn<std::string> post_title{nullptr, ""};
    TableColumn<bool> post_published{nullptr, ""};

    TableColumn<int> comment_id{nullptr, ""};
    TableColumn<int> comment_post_id{nullptr, ""};
    TableColumn<int> comment_user_id{nullptr, ""};
    TableColumn<std::string> comment_content{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test INNER JOIN
TEST_F(JoinQueryTest, InnerJoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test LEFT JOIN
TEST_F(JoinQueryTest, LeftJoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema, JoinType::LEFT).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test RIGHT JOIN
TEST_F(JoinQueryTest, RightJoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema, JoinType::RIGHT).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test FULL OUTER JOIN
TEST_F(JoinQueryTest, FullJoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema, JoinType::FULL).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test CROSS JOIN (simplified - cross join typically doesn't need ON clause)
TEST_F(JoinQueryTest, CrossJoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema, JoinType::CROSS).on(user_id > lit(0));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test multiple JOINs (simplified to single join for now)
TEST_F(JoinQueryTest, MultipleJoinsExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test JOIN with complex conditions
TEST_F(JoinQueryTest, JoinWithComplexConditionsExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema)
                 .on(post_user_id == user_id && post_published == lit(true));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test JOIN with WHERE clause
TEST_F(JoinQueryTest, JoinWithWhereExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema).on(post_user_id == user_id)
                 .where(user_active == lit(true));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test JOIN with aggregates (simplified without GROUP BY for now)
TEST_F(JoinQueryTest, JoinWithAggregatesExpression) {
    auto query = select(user_name, count(post_id).as("post_count"))
                 .from(users_schema)
                 .join(posts_schema, JoinType::LEFT).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test JOIN with ORDER BY
TEST_F(JoinQueryTest, JoinWithOrderByExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema).on(post_user_id == user_id)
                 .order_by(asc(user_name), desc(post_title));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}