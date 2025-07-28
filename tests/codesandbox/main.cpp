#include <iostream>

#include "db_record_factory.hpp"
#include "pq_dialect.hpp"
#include "query_compiler.hpp"
using namespace demiplane::db;

void complete_usage_example() {
    // ============================================================================
    // Step 1: Define Schema with Type Information
    // ============================================================================

    auto users_schema = std::make_shared<TableSchema>("users");
    users_schema->add_field<int>("id", "INTEGER")
                .primary_key("id")
                .add_field<std::string>("name", "VARCHAR(255)")
                .nullable("name", false)
                .add_field<std::string>("email", "VARCHAR(255)")
                .unique("email")
                .add_field<int>("age", "INTEGER")
                .add_field<bool>("active", "BOOLEAN")
                .add_field<double>("balance", "DECIMAL(10,2)");

    auto posts_schema = std::make_shared<TableSchema>("posts");
    posts_schema->add_field<int>("id", "INTEGER")
                .primary_key("id")
                .add_field<int>("user_id", "INTEGER")
                .foreign_key("user_id", "users", "id")
                .add_field<std::string>("title", "VARCHAR(255)")
                .add_field<std::string>("content", "TEXT")
                .add_field<bool>("published", "BOOLEAN");

    // ============================================================================
    // Step 2: Create Typed Column References
    // ============================================================================

    // Type-safe column references
    auto user_id = users_schema->column<int>("id");
    auto user_name = users_schema->column<std::string>("name");
    auto user_email = users_schema->column<std::string>("email");
    auto user_age = users_schema->column<int>("age");
    auto user_active = users_schema->column<bool>("active");

    // auto post_id = posts_schema->column<int>("id");
    auto post_user_id = posts_schema->column<int>("user_id");
    auto post_title = posts_schema->column<std::string>("title");
    auto post_published = posts_schema->column<bool>("published");

    // ============================================================================
    // Step 3: Build Queries with Natural Syntax
    // ============================================================================

    // Simple SELECT with WHERE
    auto query1 = select(user_id, user_name, user_email)
        .from(users_schema)
        .where(user_age > lit(18) && user_active == lit(true))
        .order_by(desc(user_name))
        .limit(10);

    // SELECT with JOIN
    // auto query2 = select(user_name, post_title)
    //     .from(users_schema)
    //     .join(posts_schema).on(user_id == post_user_id);
    // Aggregate query
    auto query3 = select(user_active, count(user_id).as("user_count"))
        .from(users_schema)
        .group_by(user_active)
        .having(count(user_id) > lit(5));

    // Complex query with subquery
    auto active_users = select(user_id)
        .from(users_schema)
        .where(user_active == lit(true));

    auto query4 = select(post_title)
        .from(posts_schema)
        .where(in(post_user_id, subquery(active_users)));

    // INSERT query
    auto insert_query = insert_into(users_schema)
        .into({"name", "email", "age", "active"})
        .values({"John Doe", "john@example.com", 25, true})
        .values({"Jane Smith", "jane@example.com", 30, true});

    // UPDATE query
    auto update_query = update(users_schema)
        .set("active", false)
        .set("balance", 0.0)
        .where(user_age < 18);

    // DELETE query
    auto delete_query = delete_from(users_schema)
        .where(user_active == lit(false) && user_age > lit(90));

    // ============================================================================
    // Step 4: Compile Queries to SQL
    // ============================================================================

    // PostgreSQL compilation
    auto pg_dial = std::make_unique<PostgresDialect>();
    QueryCompiler pg_compiler(std::make_unique<PostgresDialect>());

    auto compiled1 = pg_compiler.compile(query1);
    std::cout << "PostgreSQL: " << compiled1.sql << std::endl;
    // Output: SELECT "id", "name", "email" FROM "users" WHERE "age" > $1 AND "active" = $2 ORDER BY "name" DESC LIMIT 10
    // Parameters: [18, true]
    for (const auto& v: compiled1.parameters) {
        std::cout << pg_dial->format_value(v) << " ";
    }
    std::cout << std::endl;
    auto compiled2 = pg_compiler.compile(query3);
    std::cout << "PostgreSQL: " << compiled2.sql << std::endl;
    // Output: SELECT "name", "title" FROM "users" JOIN "posts" ON "id" = "user_id" WHERE "published" = $1 ORDER BY "name" ASC, "title" DESC


    // ============================================================================
    // Step 5: Integration with Records
    // ============================================================================

    // Create records using existing RecordFactory
    RecordFactory factory(users_schema);
    auto user_record = factory.create_record();
    user_record["name"].set("Alice Johnson");
    user_record["email"].set("alice@example.com");
    user_record["age"].set(28);
    user_record["active"].set(true);
    user_record["balance"].set(1000.50);
    user_record["id"].set(123);

    // Build query from record
    auto query_from_record = select(all())
        .from(user_record)
        .where(user_id == user_record["id"].get<int32_t>());

    // Insert from record
    auto insert_from_record = insert_into(users_schema)
        .into({"name", "email", "age", "active", "balance"})
        .values(user_record);

    // ============================================================================
    // Step 6: Advanced Queries
    // ============================================================================

    // WITH clause (CTE)
    auto high_value_users = with("high_value_users",
        select(user_id, user_name)
        .from(users_schema)
        .where(user_active == true && user_age > 25)
    );

    // UNION query
    auto union_query = union_all(
        select(user_name).from(users_schema).where(user_active == true),
        select(user_name).from(users_schema).where(user_age > 65)
    );

    // EXISTS query
    auto exists_query = select(user_name)
        .from(users_schema)
        .where(exists(
            select(lit(1))
            .from(posts_schema)
            .where(post_user_id == user_id && post_published == true)
        ));

    // CASE expression (if you implement it)
    // auto case_query = select(
    //     user_name,
    //     case_when(user_age < 18, lit("minor"))
    //         .when(user_age < 65, lit("adult"))
    //         .else_(lit("senior"))
    //         .as("age_group")
    // ).from(users_schema);

    // ============================================================================
    // Step 7: Error Handling
    // ============================================================================

    try {
        // This will throw because types don't match
        // auto bad_column = users_schema->column<double>("id"); // id is int, not double

        // This will throw because column doesn't exist
        // auto missing = users_schema->column<std::string>("missing_column");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // ============================================================================
    // Step 8: Dynamic Query Building
    // ============================================================================

    // Build WHERE clause dynamically
    auto base_query = select(user_id, user_name).from(users_schema);

    // Add conditions based on runtime values
    bool include_active_only = true;
    int min_age = 21;

    if (include_active_only && min_age > 0) {
        auto filtered = base_query.where(user_active == lit(true) && user_age >= lit(min_age));
        auto compiled = pg_compiler.compile(filtered);
        std::cout << "Dynamic query: " << compiled.sql << std::endl;
    }
}
class X {
public:
};

int main() {
    complete_usage_example();
    return 0;
}
