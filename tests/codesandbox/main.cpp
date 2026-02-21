#include <cassert>
#include <memory>

#include <query_expressions.hpp>

using namespace demiplane::db;

int main() {
    // Constexpr query creation test with static constexpr (std::string has full constexpr support)
    static constexpr auto id   = col("id");
    static constexpr auto name = col("name");
    static constexpr auto q    = select(id, name).from("students").as("s").where(id == 1);

    static_assert(q.condition().left().name() == "id");
    static_assert(q.condition().right().value() == 1);
    static_assert(q.query().alias() == "s");

    return 0;
}
