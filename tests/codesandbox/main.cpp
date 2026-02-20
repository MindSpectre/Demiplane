#include <cassert>
#include <memory>

#include <query_expressions.hpp>

using namespace demiplane::db;

// Constexpr query creation test
constexpr auto id   = col("id");
constexpr auto name = col("name");
constexpr static auto q = select(id, name).from("students").where(id == 1);
// constexpr static auto q2 = select("id").from("st");
// Verify the expression tree data at compile time
static_assert(q.condition().left().name() == "id");
static_assert(q.condition().right().value() == 1);

int main() {
    return 0;
}
