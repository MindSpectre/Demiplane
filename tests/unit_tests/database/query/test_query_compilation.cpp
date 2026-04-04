// =============================================================================
// Compile-time query DSL verification
//
// Ported from codesandbox/static_constexpr_audit.cpp into GTest format.
// Every static_assert fires at compile time — the TEST() wrappers make
// each section visible in the test runner and its output.
// =============================================================================

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-variable"

#include <gtest/gtest.h>
#include <postgres_dialect.hpp>
#include <postgres_type_mapping.hpp>
#include <query_compiler.hpp>
#include <query_expressions.hpp>

#include "db_table.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::db::constraints;
using demiplane::gears::FixedString;

// =============================================================================
// Shared definitions — tables, columns, compiler
// =============================================================================
static constexpr QueryCompiler<PostgresDialect> compiler;

using UsersTable = StaticTable<"users",
                               StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                               StaticFieldSchema<std::string, "name">,
                               StaticFieldSchema<int, "age">,
                               StaticFieldSchema<bool, "active">,
                               StaticFieldSchema<std::string, "email">,
                               StaticFieldSchema<int, "salary">,
                               StaticFieldSchema<std::string, "department">>;

using PostsTable = StaticTable<"posts",
                               StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                               StaticFieldSchema<int, "user_id">,
                               StaticFieldSchema<std::string, "title">,
                               StaticFieldSchema<bool, "published">>;

using OrdersTable = StaticTable<"orders",
                                StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                StaticFieldSchema<int, "user_id">,
                                StaticFieldSchema<double, "amount">,
                                StaticFieldSchema<std::string, "status">>;

using CommentsTable = StaticTable<"comments",
                                  StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                  StaticFieldSchema<int, "post_id">,
                                  StaticFieldSchema<std::string, "body">>;

static constexpr UsersTable users{Providers::PostgreSQL};
static constexpr PostsTable posts{Providers::PostgreSQL};
static constexpr OrdersTable orders{Providers::PostgreSQL};
static constexpr CommentsTable comments{Providers::PostgreSQL};

static constexpr auto u_id     = users.column<"id">();
static constexpr auto u_name   = users.column<"name">();
static constexpr auto u_age    = users.column<"age">();
static constexpr auto u_active = users.column<"active">();
static constexpr auto u_email  = users.column<"email">();
static constexpr auto u_salary = users.column<"salary">();
static constexpr auto u_dept   = users.column<"department">();

static constexpr auto p_id        = posts.column<"id">();
static constexpr auto p_user_id   = posts.column<"user_id">();
static constexpr auto p_title     = posts.column<"title">();
static constexpr auto p_published = posts.column<"published">();

static constexpr auto o_id      = orders.column<"id">();
static constexpr auto o_user_id = orders.column<"user_id">();
static constexpr auto o_amount  = orders.column<"amount">();
static constexpr auto o_status  = orders.column<"status">();

[[maybe_unused]] static constexpr auto cm_id = comments.column<"id">();
static constexpr auto cm_post_id = comments.column<"post_id">();
static constexpr auto cm_body    = comments.column<"body">();

// =============================================================================
// 1. Table type traits
// =============================================================================
TEST(QueryCompilation, TableTypeTraits) {
    static_assert(UsersTable::N == 7);
    static_assert(PostsTable::N == 4);
    static_assert(OrdersTable::N == 4);
    static_assert(CommentsTable::N == 3);

    static_assert(IsStaticTable<UsersTable>);
    static_assert(IsStaticTable<PostsTable>);
    static_assert(IsStaticTable<OrdersTable>);
    static_assert(IsStaticTable<CommentsTable>);
    static_assert(IsTable<UsersTable>);
    static_assert(IsTable<PostsTable>);
}

// =============================================================================
// 2. Column access — name-based and positional
// =============================================================================
TEST(QueryCompilation, ColumnAccess) {
    static_assert(std::is_same_v<decltype(u_id)::value_type, int>);
    static_assert(std::is_same_v<decltype(u_name)::value_type, std::string>);
    static_assert(std::is_same_v<decltype(u_age)::value_type, int>);
    static_assert(std::is_same_v<decltype(u_active)::value_type, bool>);
    static_assert(std::is_same_v<decltype(o_amount)::value_type, double>);

    [[maybe_unused]] static constexpr auto u_col0 = users.column<0>();
    [[maybe_unused]] static constexpr auto u_col1 = users.column<1>();
    static_assert(std::is_same_v<decltype(u_col0)::value_type, int>);
    static_assert(std::is_same_v<decltype(u_col1)::value_type, std::string>);

    static_assert(IsTypedColumn<decltype(u_id)>);
    static_assert(!IsColumn<decltype(u_id)>);
    static_assert(IsColumnLike<decltype(u_id)>);
    static_assert(IsDbOperand<decltype(u_id)>);
}

// =============================================================================
// 3. SELECT ... FROM — basic
// =============================================================================
TEST(QueryCompilation, SelectBasic) {
    static constexpr auto r = compiler.compile_static(select(users.column<"id">(), users.column<"name">()).from(users));
    static_assert(r.sql() == R"(SELECT "users"."id", "users"."name" FROM "users")");
    static_assert(r.size() == 0);
}

TEST(QueryCompilation, SelectDistinct) {
    static constexpr auto r =
        compiler.compile_static(select_distinct(users.column<"name">(), users.column<"age">()).from(users));
    static_assert(r.sql() == R"(SELECT DISTINCT "users"."name", "users"."age" FROM "users")");
}

TEST(QueryCompilation, SelectAll) {
    // Verifies select(all("users")).from(users) compiles
    static constexpr auto q = select(all("users")).from(users);
    (void)q;
}

// =============================================================================
// 4. WHERE — single condition
// =============================================================================
TEST(QueryCompilation, WhereSimple) {
    static constexpr auto r = compiler.compile_static(select(u_name).from(users).where(u_age > 18));
    static_assert(r.sql() == R"(SELECT "users"."name" FROM "users" WHERE ("users"."age" > $1))");
    static_assert(r.size() == 1);
}

// =============================================================================
// 5. WHERE — AND / OR / complex
// =============================================================================
TEST(QueryCompilation, WhereAnd) {
    static constexpr auto r =
        compiler.compile_static(select(u_id, u_name).from(users).where(u_age > 18 && u_active == true));
    static_assert(
        r.sql() ==
        R"(SELECT "users"."id", "users"."name" FROM "users" WHERE (("users"."age" > $1) AND ("users"."active" = $2)))");
    static_assert(r.size() == 2);
}

TEST(QueryCompilation, WhereOr) {
    static constexpr auto r =
        compiler.compile_static(select(u_name).from(users).where(u_age < 18 || u_active == false));
    static_assert(r.sql() ==
                  R"(SELECT "users"."name" FROM "users" WHERE (("users"."age" < $1) OR ("users"."active" = $2)))");
}

TEST(QueryCompilation, WhereComplex) {
    // (A && B) || (C && D) — verifies nesting compiles
    static constexpr auto q =
        select(u_id).from(users).where((u_age > 18 && u_age < 65) || (u_active == true && u_age >= 65));
    (void)q;
}

// =============================================================================
// 6. Complex conditions — IN, BETWEEN, IS NULL, LIKE, NOT, comparisons
// =============================================================================
TEST(QueryCompilation, ConditionIn) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).where(in(u_age, 18, 25, 30))).sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE "users"."age" IN (18, 25, 30))");
}

TEST(QueryCompilation, ConditionBetween) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).where(between(u_age, 18, 65))).sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE "users"."age" BETWEEN 18 AND 65)");
}

TEST(QueryCompilation, ConditionIsNull) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).where(is_null(u_email))).sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE "users"."email" IS NULL)");
}

TEST(QueryCompilation, ConditionIsNotNull) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).where(is_not_null(u_email))).sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE "users"."email" IS NOT NULL)");
}

TEST(QueryCompilation, ConditionLike) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).where(like(u_name, "%john%"))).sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE ("users"."name" LIKE '%john%'))");
}

TEST(QueryCompilation, ConditionNot) {
    static constexpr auto q = select(u_name).from(users).where(!(u_active == true));
    (void)q;
}

TEST(QueryCompilation, ConditionStringEquality) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_name == "john")).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = 'john'))");
}

TEST(QueryCompilation, AllComparisonOperators) {
    static constexpr auto eq  = u_age == 25;
    static constexpr auto neq = u_age != 25;
    static constexpr auto gt  = u_age > 18;
    static constexpr auto gte = u_age >= 18;
    static constexpr auto lt  = u_age < 65;
    static constexpr auto lte = u_age <= 65;
    (void)eq;
    (void)neq;
    (void)gt;
    (void)gte;
    (void)lt;
    (void)lte;
}

// =============================================================================
// 7. JOINs between static tables
// =============================================================================
TEST(QueryCompilation, JoinInner) {
    static_assert(
        compiler.compile_static(select(u_name, p_title).from(users).join(posts).on(u_id == p_user_id)).sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" INNER JOIN "posts" ON ("users"."id" = "posts"."user_id"))");
}

TEST(QueryCompilation, JoinLeft) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, p_title).from(users).join(posts, JoinType::LEFT).on(u_id == p_user_id))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" LEFT JOIN "posts" ON ("users"."id" = "posts"."user_id"))");
}

TEST(QueryCompilation, JoinRight) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, p_title).from(users).join(posts, JoinType::RIGHT).on(u_id == p_user_id))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" RIGHT JOIN "posts" ON ("users"."id" = "posts"."user_id"))");
}

TEST(QueryCompilation, JoinFull) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, p_title).from(users).join(posts, JoinType::FULL).on(u_id == p_user_id))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" FULL OUTER JOIN "posts" ON ("users"."id" = "posts"."user_id"))");
}

TEST(QueryCompilation, JoinCross) {
    static constexpr auto q = select(u_name, p_title).from(users).join(posts, JoinType::CROSS).on(u_id > 0);
    (void)q;
}

TEST(QueryCompilation, JoinMultiple) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(select(u_name, p_title, cm_body)
                                                   .from(users)
                                                   .join(posts)
                                                   .on(u_id == p_user_id)
                                                   .join(comments)
                                                   .on(p_id == cm_post_id))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title", "comments"."body" FROM "users" INNER JOIN "posts" ON ("users"."id" = "posts"."user_id") INNER JOIN "comments" ON ("posts"."id" = "comments"."post_id"))");
}

TEST(QueryCompilation, JoinComplexCondition) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, p_title).from(users).join(posts).on(u_id == p_user_id && p_published == true))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" INNER JOIN "posts" ON (("users"."id" = "posts"."user_id") AND ("posts"."published" = TRUE)))");
}

TEST(QueryCompilation, JoinWithWhere) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, p_title).from(users).join(posts).on(u_id == p_user_id).where(u_active == true))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" INNER JOIN "posts" ON ("users"."id" = "posts"."user_id") WHERE ("users"."active" = TRUE))");
}

TEST(QueryCompilation, JoinWithAggregateGroupBy) {
    static constexpr auto q = select(u_name, count(p_id).as("post_count"))
                                  .from(users)
                                  .join(posts, JoinType::LEFT)
                                  .on(u_id == p_user_id)
                                  .group_by(u_name);
    (void)q;
}

TEST(QueryCompilation, JoinWithOrderBy) {
    static constexpr auto q =
        select(u_name, p_title).from(users).join(posts).on(u_id == p_user_id).order_by(asc(u_name), desc(p_title));
    (void)q;
}

// =============================================================================
// 8. ORDER BY
// =============================================================================
TEST(QueryCompilation, OrderBy) {
    static_assert(
        compiler.compile_static(select(u_id, u_name).from(users).order_by(asc(u_name), desc(u_age))).sql() ==
        R"(SELECT "users"."id", "users"."name" FROM "users" ORDER BY "users"."name" ASC, "users"."age" DESC)");
}

TEST(QueryCompilation, OrderByMultiple) {
    static constexpr auto q =
        select(u_dept, u_salary, u_name).from(users).order_by(asc(u_dept), desc(u_salary), asc(u_name));
    (void)q;
}

// =============================================================================
// 9. LIMIT
// =============================================================================
TEST(QueryCompilation, Limit) {
    static_assert(compiler.compile_static(select(u_id).from(users).limit(10)).sql() ==
                  R"(SELECT "users"."id" FROM "users" LIMIT 10)");
}

TEST(QueryCompilation, LimitWithOrderBy) {
    static constexpr auto q = select(u_name).from(users).order_by(asc(u_name)).limit(5);
    (void)q;
}

TEST(QueryCompilation, LimitWithWhereOrderBy) {
    static constexpr auto q = select(u_name).from(users).where(u_active == true).order_by(desc(u_name)).limit(20);
    (void)q;
}

// =============================================================================
// 10. Aggregates
// =============================================================================
TEST(QueryCompilation, AggregateCount) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(count(u_id)).from(users)).sql() ==
                  R"(SELECT COUNT("users"."id") FROM "users")");
}

TEST(QueryCompilation, AggregateCountAll) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(count_all()).from(users)).sql() ==
                  R"(SELECT COUNT(*) FROM "users")");
}

TEST(QueryCompilation, AggregateSum) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(sum(u_age)).from(users)).sql() ==
                  R"(SELECT SUM("users"."age") FROM "users")");
}

TEST(QueryCompilation, AggregateAvg) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(avg(u_age)).from(users)).sql() ==
                  R"(SELECT AVG("users"."age") FROM "users")");
}

TEST(QueryCompilation, AggregateMax) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(max(u_age)).from(users)).sql() ==
                  R"(SELECT MAX("users"."age") FROM "users")");
}

TEST(QueryCompilation, AggregateMin) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(min(u_age)).from(users)).sql() ==
                  R"(SELECT MIN("users"."age") FROM "users")");
}

TEST(QueryCompilation, AggregateCountDistinct) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(count_distinct(u_name)).from(users)).sql() ==
                  R"(SELECT COUNT(DISTINCT "users"."name") FROM "users")");
}

TEST(QueryCompilation, AggregateWithAliases) {
    static_assert(compiler
                      .compile_static<ParamMode::Inline>(
                          select(count(u_id).as("total_users"), avg(u_age).as("avg_age")).from(users))
                      .sql() ==
                  R"(SELECT COUNT("users"."id") AS "total_users", AVG("users"."age") AS "avg_age" FROM "users")");
}

TEST(QueryCompilation, AggregateMultiple) {
    static constexpr auto q =
        select(count(u_id), sum(u_age), avg(u_age), min(u_age), max(u_age), count_distinct(u_name)).from(users);
    (void)q;
}

// =============================================================================
// 11. GROUP BY / HAVING
// =============================================================================
TEST(QueryCompilation, GroupBySingle) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_dept, count(u_id).as("cnt")).from(users).group_by(u_dept))
            .sql() ==
        R"(SELECT "users"."department", COUNT("users"."id") AS "cnt" FROM "users" GROUP BY "users"."department")");
}

TEST(QueryCompilation, GroupByMultiple) {
    static constexpr auto q = select(u_dept, u_active, count(u_id).as("cnt")).from(users).group_by(u_dept, u_active);
    (void)q;
}

TEST(QueryCompilation, GroupByWithWhere) {
    static constexpr auto q =
        select(u_dept, count(u_id).as("cnt")).from(users).where(u_active == true).group_by(u_dept);
    (void)q;
}

TEST(QueryCompilation, Having) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_dept, count(u_id).as("cnt")).from(users).group_by(u_dept).having(count(u_id) > 5))
            .sql() ==
        R"(SELECT "users"."department", COUNT("users"."id") AS "cnt" FROM "users" GROUP BY "users"."department" HAVING (COUNT("users"."id") > 5))");
}

TEST(QueryCompilation, HavingWithWhere) {
    static constexpr auto q = select(u_dept, count(u_id).as("cnt"))
                                  .from(users)
                                  .where(u_active == true)
                                  .group_by(u_dept)
                                  .having(count(u_id) > 5);
    (void)q;
}

TEST(QueryCompilation, KitchenSink) {
    static constexpr auto q = select(u_dept, count(u_id).as("user_count"), avg(u_salary).as("avg_salary"))
                                  .from(users)
                                  .where(u_active == true)
                                  .group_by(u_dept)
                                  .having(count(u_id) > 2)
                                  .order_by(desc(u_dept))
                                  .limit(10);
    (void)q;
}

// =============================================================================
// 12. Subqueries — EXISTS
// =============================================================================
TEST(QueryCompilation, SubqueryExists) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_id, u_name).from(users).where(exists(select(p_id).from(posts).where(p_published == true))))
            .sql() ==
        R"(SELECT "users"."id", "users"."name" FROM "users" WHERE EXISTS (SELECT "posts"."id" FROM "posts" WHERE ("posts"."published" = TRUE)))");
}

TEST(QueryCompilation, SubqueryHelpers) {
    // These compile — inner queries used for subquery composition
    static constexpr auto q_active    = select(u_id).from(users).where(u_active == true);
    static constexpr auto q_published = select(p_user_id).from(posts).where(p_published == true);
    static constexpr auto q_pending   = select(o_id).from(orders).where(o_status == "pending");
    static constexpr auto q_high_val =
        select(o_user_id).from(orders).group_by(o_user_id).having(sum(o_amount) > lit(1000.0));
    (void)q_active;
    (void)q_published;
    (void)q_pending;
    (void)q_high_val;
}

// =============================================================================
// 13. CASE WHEN expressions
// =============================================================================
TEST(QueryCompilation, CaseSimple) {
    static constexpr auto q = case_when(u_active == true, lit("Active"));
    (void)q;
}

TEST(QueryCompilation, CaseWithElse) {
    static constexpr auto q = case_when(u_active == true, lit("Active")).else_(lit("Inactive"));
    (void)q;
}

TEST(QueryCompilation, CaseMultipleBranches) {
    static constexpr auto q = case_when(u_age < 18, lit("Minor"))
                                  .when(u_age < 65, lit("Adult"))
                                  .when(u_age >= 65, lit("Senior"))
                                  .else_(lit("Unknown"));
    (void)q;
}

TEST(QueryCompilation, CaseInSelect) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(select(u_id,
                                                      case_when(u_age < 18, lit("Minor"))
                                                          .when(u_age < 65, lit("Adult"))
                                                          .else_(lit("Senior"))
                                                          .as("age_group"))
                                                   .from(users))
            .sql() ==
        R"(SELECT "users"."id", CASE WHEN ("users"."age" < 18) THEN 'Minor' WHEN ("users"."age" < 65) THEN 'Adult' ELSE 'Senior' END AS "age_group" FROM "users")");
}

TEST(QueryCompilation, CaseNumericResult) {
    static constexpr auto q = case_when(o_amount > lit(1000.0), lit(1))
                                  .when(o_amount > lit(500.0), lit(2))
                                  .when(o_amount > lit(100.0), lit(3))
                                  .else_(lit(4));
    (void)q;
}

TEST(QueryCompilation, CaseNested) {
    static constexpr auto q =
        case_when(u_active == true, case_when(u_age >= 65, lit("Active Senior")).else_(lit("Active")))
            .else_(lit("Inactive"));
    (void)q;
}

// =============================================================================
// 14. SET operations — UNION
// =============================================================================
TEST(QueryCompilation, SetOperationHelpers) {
    static constexpr auto q_young  = select(u_name, u_age).from(users).where(u_age < 30);
    static constexpr auto q_senior = select(u_name, u_age).from(users).where(u_age >= 60);
    static constexpr auto q_middle = select(u_name, u_age).from(users).where(u_age >= 30 && u_age < 60);
    (void)q_young;
    (void)q_senior;
    (void)q_middle;
}

// =============================================================================
// 15. CTEs (Common Table Expressions)
// =============================================================================
TEST(QueryCompilation, CteRecursive) {
    static constexpr auto q = with_recursive("numbers", select(lit(1).as("n")).from("dual"));
    static_assert(q.name() == "numbers");
    static_assert(q.recursive());
}

TEST(QueryCompilation, CteInnerQueries) {
    static constexpr auto q_active = select(u_id, u_name).from(users).where(u_active == true);
    static constexpr auto q_agg = select(o_user_id, sum(o_amount).as("total_amount")).from(orders).group_by(o_user_id);
    static constexpr auto q_filter = select(p_id, p_title, p_user_id).from(posts).where(p_published == true);
    (void)q_active;
    (void)q_agg;
    (void)q_filter;
}

// =============================================================================
// 16. DDL — CREATE TABLE / DROP TABLE
// =============================================================================
TEST(QueryCompilation, DdlCreateTable) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(create_table(users)).sql() ==
        R"(CREATE TABLE "users" ("id" INTEGER PRIMARY KEY, "name" TEXT, "age" INTEGER, "active" BOOLEAN, "email" TEXT, "salary" INTEGER, "department" TEXT))");
}

TEST(QueryCompilation, DdlCreateTableIfNotExists) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(create_table(users, true)).sql() ==
        R"(CREATE TABLE IF NOT EXISTS "users" ("id" INTEGER PRIMARY KEY, "name" TEXT, "age" INTEGER, "active" BOOLEAN, "email" TEXT, "salary" INTEGER, "department" TEXT))");
}

TEST(QueryCompilation, DdlDropTable) {
    static_assert(compiler.compile_static<ParamMode::Inline>(drop_table(users)).sql() == R"(DROP TABLE "users")");
}

TEST(QueryCompilation, DdlDropTableIfExists) {
    static_assert(compiler.compile_static<ParamMode::Inline>(drop_table(users, true)).sql() ==
                  R"(DROP TABLE IF EXISTS "users")");
}

TEST(QueryCompilation, DdlDropTableCascade) {
    static_assert(compiler.compile_static<ParamMode::Inline>(drop_table(users, true, true)).sql() ==
                  R"(DROP TABLE IF EXISTS "users" CASCADE)");
}

// =============================================================================
// 17. SQL generation — Inline mode (comprehensive)
// =============================================================================
TEST(QueryCompilation, InlineJoin) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(select(u_name, p_title).from(users).join(posts).on(u_id == p_user_id))
            .sql() ==
        R"(SELECT "users"."name", "posts"."title" FROM "users" INNER JOIN "posts" ON ("users"."id" = "posts"."user_id"))");
}

TEST(QueryCompilation, InlineAggregateGroupByHaving) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_dept, count(u_id).as("cnt")).from(users).group_by(u_dept).having(count(u_id) > 5))
            .sql() ==
        R"(SELECT "users"."department", COUNT("users"."id") AS "cnt" FROM "users" GROUP BY "users"."department" HAVING (COUNT("users"."id") > 5))");
}

TEST(QueryCompilation, InlineOrderByLimit) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_name).from(users).order_by(asc(u_name)).limit(10)).sql() ==
        R"(SELECT "users"."name" FROM "users" ORDER BY "users"."name" ASC LIMIT 10)");
}

TEST(QueryCompilation, InlineBooleanEquality) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_active == true)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."active" = TRUE))");
}

TEST(QueryCompilation, InlineComplexConditions) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name).from(users).where((u_age > 18 && u_active == true) || in(u_age, 25, 30, 35)))
            .sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE ((("users"."age" > 18) AND ("users"."active" = TRUE)) OR "users"."age" IN (25, 30, 35)))");
}

// =============================================================================
// 18. SQL generation — Tuple mode (parameterized)
// =============================================================================
TEST(QueryCompilation, TupleNoParams) {
    static_assert(compiler.compile_static(select(u_id, u_name).from(users)).sql() ==
                  R"(SELECT "users"."id", "users"."name" FROM "users")");
}

TEST(QueryCompilation, TupleSinglePlaceholder) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(u_age > 18)).sql() ==
                  R"(SELECT "users"."name" FROM "users" WHERE ("users"."age" > $1))");
}

TEST(QueryCompilation, TupleCaseWhenPlaceholders) {
    static_assert(
        compiler
            .compile_static(select(u_id,
                                   case_when(u_age < 18, lit("Minor"))
                                       .when(u_age < 65, lit("Adult"))
                                       .else_(lit("Senior"))
                                       .as("age_group"))
                                .from(users))
            .sql() ==
        R"(SELECT "users"."id", CASE WHEN ("users"."age" < $1) THEN $2 WHEN ("users"."age" < $3) THEN $4 ELSE $5 END AS "age_group" FROM "users")");
}

TEST(QueryCompilation, TupleUnionPlaceholders) {
    static_assert(
        compiler
            .compile_static(union_query(select(u_name).from(users).where(u_age < 30),
                                        select(u_name).from(users).where(u_age >= 60)))
            .sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE ("users"."age" < $1) UNION SELECT "users"."name" FROM "users" WHERE ("users"."age" >= $2))");
}

TEST(QueryCompilation, TupleComplexConditions) {
    static_assert(
        compiler
            .compile_static(select(u_name).from(users).where((u_age > 18 && u_active == true) || in(u_age, 25, 30, 35)))
            .sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE ((("users"."age" > $1) AND ("users"."active" = $2)) OR "users"."age" IN ($3, $4, $5)))");
}

TEST(QueryCompilation, TupleBetween) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(between(u_age, 18, 65))).sql() ==
                  R"(SELECT "users"."name" FROM "users" WHERE "users"."age" BETWEEN $1 AND $2)");
}

TEST(QueryCompilation, TupleIsNull) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(is_null(u_email))).sql() ==
                  R"(SELECT "users"."name" FROM "users" WHERE "users"."email" IS NULL)");
}

TEST(QueryCompilation, TupleStringEquality) {
    static_assert(compiler.compile_static(select(u_id).from(users).where(u_name == "john")).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = $1))");
}

TEST(QueryCompilation, TupleExists) {
    static_assert(
        compiler
            .compile_static(
                select(u_name).from(users).where(exists(select(p_id).from(posts).where(p_published == true))))
            .sql() ==
        R"(SELECT "users"."name" FROM "users" WHERE EXISTS (SELECT "posts"."id" FROM "posts" WHERE ("posts"."published" = $1)))");
}

// =============================================================================
// 19. Static parameter extraction
// =============================================================================
TEST(QueryCompilation, ParamsSingleInt) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(u_age > 18)).params() == std::tuple{18});
    static_assert(compiler.compile_static(select(u_name).from(users).where(u_age > 18)).size() == 1);
}

TEST(QueryCompilation, ParamsIntAndBool) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(u_age > 18 && u_active == true)).params() ==
                  std::tuple{18, true});
}

TEST(QueryCompilation, ParamsInList) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(in(u_age, 25, 30, 35))).params() ==
                  std::tuple{25, 30, 35});
}

TEST(QueryCompilation, ParamsBetween) {
    static_assert(compiler.compile_static(select(u_name).from(users).where(between(u_age, 18, 65))).params() ==
                  std::tuple{18, 65});
}

TEST(QueryCompilation, ParamsBoolean) {
    static_assert(compiler.compile_static(select(u_id).from(users).where(u_active == true)).params() ==
                  std::tuple{true});
}

TEST(QueryCompilation, ParamsCaseWhenCount) {
    static constexpr auto r = compiler.compile_static(
        select(u_id,
               case_when(u_age < 18, lit("Minor")).when(u_age < 65, lit("Adult")).else_(lit("Senior")).as("group"))
            .from(users));
    static_assert(r.size() == 5);
}

TEST(QueryCompilation, ParamsUnion) {
    static_assert(compiler
                      .compile_static(union_query(select(u_name).from(users).where(u_age < 30),
                                                  select(u_name).from(users).where(u_age >= 60)))
                      .params() == std::tuple{30, 60});
}

TEST(QueryCompilation, ParamsExistsInner) {
    static_assert(compiler
                      .compile_static(
                          select(u_name).from(users).where(exists(select(p_id).from(posts).where(p_published == true))))
                      .params() == std::tuple{true});
}

TEST(QueryCompilation, ParamsEmpty) {
    static constexpr auto r = compiler.compile_static(select(u_id, u_name).from(users));
    static_assert(r.size() == 0);
}

// =============================================================================
// 20. ParamPlaceholder tests
// =============================================================================
TEST(QueryCompilation, ParamPlaceholderSingle) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(select(u_id).from(users).where(p_published == ParamPlaceholder<bool>()))
            .sql() == R"(SELECT "users"."id" FROM "users" WHERE ("posts"."published" = $1))");
}

TEST(QueryCompilation, ParamPlaceholderMixed) {
    static_assert(compiler
                      .compile_static<ParamMode::Inline>(
                          select(u_id).from(users).where(u_age > 18 && p_published == ParamPlaceholder<bool>()))
                      .sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE (("users"."age" > 18) AND ("posts"."published" = $1)))");
}

TEST(QueryCompilation, ParamPlaceholderMultiple) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_id).from(users).where(u_age > ParamPlaceholder<int>() && u_active == ParamPlaceholder<bool>()))
            .sql() == R"(SELECT "users"."id" FROM "users" WHERE (("users"."age" > $1) AND ("users"."active" = $2)))");
}

TEST(QueryCompilation, ParamPlaceholderNoValues) {
    static constexpr auto r = compiler.compile_static<ParamMode::Inline>(
        select(u_id).from(users).where(p_published == ParamPlaceholder<bool>()));
    static_assert(r.size() == 0);
}

// =============================================================================
// 21. Mixed static + dynamic columns
// =============================================================================
TEST(QueryCompilation, MixedStaticDynamic) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_name, col("extra_field")).from(users)).sql() ==
                  R"(SELECT "users"."name", "extra_field" FROM "users")");
}

TEST(QueryCompilation, MixedStaticDynamicWhere) {
    static_assert(compiler
                      .compile_static<ParamMode::Inline>(
                          select(u_id).from(users).where(u_active == true && col("role") == "admin"))
                      .sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE (("users"."active" = TRUE) AND ("role" = 'admin')))");
}

TEST(QueryCompilation, MixedStaticTableStringJoin) {
    static_assert(
        compiler
            .compile_static<ParamMode::Inline>(
                select(u_name, col("title")).from(users).join("articles").on(u_id == col("author_id")))
            .sql() ==
        R"(SELECT "users"."name", "title" FROM "users" INNER JOIN "articles" ON ("users"."id" = "author_id"))");
}

// =============================================================================
// 22. format_value branch coverage (Inline mode)
//     Tests every branch of PostgresDialect::format_value_impl
// =============================================================================

// -- monostate → NULL --
TEST(QueryCompilation, FormatValueNull) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_name == std::monostate{})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = NULL))");
}

// -- bool true → TRUE --
TEST(QueryCompilation, FormatValueBoolTrue) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_active == true)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."active" = TRUE))");
}

// -- bool false → FALSE --
TEST(QueryCompilation, FormatValueBoolFalse) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_active == false)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."active" = FALSE))");
}

// -- char → 'X' --
TEST(QueryCompilation, FormatValueChar) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("flag") == char{'A'})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("flag" = 'A'))");
}

// -- char escape: single quote → '''' --
TEST(QueryCompilation, FormatValueCharEscapeSingleQuote) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("flag") == char{'\''})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("flag" = ''''))");
}

// -- char escape: backslash → '\\' --
TEST(QueryCompilation, FormatValueCharEscapeBackslash) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("flag") == char{'\\'})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("flag" = '\\'))");
}

// -- signed int positive --
TEST(QueryCompilation, FormatValueSignedIntPositive) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_age == 42)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."age" = 42))");
}

// -- signed int zero --
TEST(QueryCompilation, FormatValueSignedIntZero) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_age == 0)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."age" = 0))");
}

// -- signed int negative --
TEST(QueryCompilation, FormatValueSignedIntNegative) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_age > -5)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("users"."age" > -5))");
}

// -- signed int16_t --
TEST(QueryCompilation, FormatValueInt16) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_age == int16_t{100})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."age" = 100))");
}

// -- signed int64_t --
TEST(QueryCompilation, FormatValueInt64) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("big") == int64_t{999999}))
            .sql() == R"(SELECT "users"."id" FROM "users" WHERE ("big" = 999999))");
}

// -- unsigned int (uint16_t) --
TEST(QueryCompilation, FormatValueUint16) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_age == uint16_t{65535})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."age" = 65535))");
}

// -- unsigned int (uint32_t) --
TEST(QueryCompilation, FormatValueUint32) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("val") == uint32_t{42})).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("val" = 42))");
}

// -- unsigned int (uint64_t) --
TEST(QueryCompilation, FormatValueUint64) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("val") == uint64_t{12345}))
            .sql() == R"(SELECT "users"."id" FROM "users" WHERE ("val" = 12345))");
}

// -- string with single quote → escape to '' --
TEST(QueryCompilation, FormatValueStringEscapeSingleQuote) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_name == "O'Brien")).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = 'O''Brien'))");
}

// -- string with backslash → escape to \\\\ --
TEST(QueryCompilation, FormatValueStringEscapeBackslash) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_name == "path\\file")).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = 'path\\file'))");
}

// -- string with both escapes --
TEST(QueryCompilation, FormatValueStringEscapeBoth) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(u_name == "it's a \\test")).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("users"."name" = 'it''s a \\test'))");
}

// -- binary data (vector<uint8_t>) --
TEST(QueryCompilation, FormatValueBinaryData) {
    static_assert(compiler
                      .compile_static<ParamMode::Inline>(select(u_id).from(users).where(
                          col("data") == std::vector<std::uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}))
                      .sql() == R"(SELECT "users"."id" FROM "users" WHERE ("data" = '\xdeadbeef'))");
}

TEST(QueryCompilation, FormatValueBinaryDataEmpty) {
    static_assert(compiler
                      .compile_static<ParamMode::Inline>(
                          select(u_id).from(users).where(col("data") == std::vector<std::uint8_t>{}))
                      .sql() == R"(SELECT "users"."id" FROM "users" WHERE ("data" = '\x'))");
}

// -- float (via constexpr_to_string digit extraction at compile time) --
TEST(QueryCompilation, FormatValueFloat) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("val") > 3.14f)).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("val" > 3.14))");
}

TEST(QueryCompilation, FormatValueFloatNegative) {
    static_assert(
        compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(col("val") > -1.5f)).sql() ==
        R"(SELECT "users"."id" FROM "users" WHERE ("val" > -1.5))");
}

// -- double (via constexpr_to_string digit extraction at compile time) --
TEST(QueryCompilation, FormatValueDouble) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(o_amount > 99.5)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("orders"."amount" > 99.5))");
}

TEST(QueryCompilation, FormatValueDoubleSmallFraction) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(o_amount == 0.001)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("orders"."amount" = 0.001))");
}

TEST(QueryCompilation, FormatValueDoubleWhole) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(o_amount == 42.0)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("orders"."amount" = 42))");
}

TEST(QueryCompilation, FormatValueDoubleNegative) {
    static_assert(compiler.compile_static<ParamMode::Inline>(select(u_id).from(users).where(o_amount > -3.14)).sql() ==
                  R"(SELECT "users"."id" FROM "users" WHERE ("orders"."amount" > -3.14))");
}

#pragma clang diagnostic pop
