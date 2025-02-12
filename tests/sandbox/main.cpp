#include "db_query.hpp"


using namespace demiplane::database::query;
using namespace demiplane::database;
int main() {
    SelectQuery query;
    query.select({utility_factory::shared_field<int>({"stuff", {}}), utility_factory::shared_field<int>("stuff1", {})})
        .limit(5)
        .offset(2)
        .where("123", WhereClause::Operator::EQUAL, std::string("12"));
    query.select(Field<int>({"stuff1", {}}), Field<std::string>({"stuff2", {}}))
        .limit(5)
        .offset(2)
        .where("123", WhereClause::Operator::EQUAL, std::string("12")).similar("123");

    return 0;
}
