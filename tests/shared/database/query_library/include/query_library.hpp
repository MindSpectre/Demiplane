#pragma once

#include "query_producer.hpp"

#include "producers/aggregate_producers.hpp"
#include "producers/case_producers.hpp"
#include "producers/clause_producers.hpp"
#include "producers/condition_producers.hpp"
#include "producers/cte_producers.hpp"
#include "producers/delete_producers.hpp"
#include "producers/insert_producers.hpp"
#include "producers/join_producers.hpp"
#include "producers/select_producers.hpp"
#include "producers/set_op_producers.hpp"
#include "producers/subquery_producers.hpp"
#include "producers/update_producers.hpp"

#include <sql_dialect.hpp>

namespace demiplane::test {

    class QueryLibrary {
    public:
        explicit QueryLibrary(std::unique_ptr<db::SqlDialect> dialect)
            : schemas_{TestSchemas::create()},
              compiler_{std::move(dialect), false} {
        }

        template <IsQueryTag Tag>
        [[nodiscard]] db::CompiledQuery produce() {
            return QueryProducer<Tag>::produce(schemas_, compiler_);
        }

        [[nodiscard]] const TestSchemas& schemas() const noexcept {
            return schemas_;
        }

        [[nodiscard]] db::QueryCompiler& compiler() noexcept {
            return compiler_;
        }

        [[nodiscard]] const db::QueryCompiler& compiler() const noexcept {
            return compiler_;
        }

    private:
        TestSchemas schemas_;
        db::QueryCompiler compiler_;
    };

}  // namespace demiplane::test
