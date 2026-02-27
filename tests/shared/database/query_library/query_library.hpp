#pragma once

#include <dialect_concepts.hpp>

#include "producers/aggregate_producers.hpp"
#include "producers/case_producers.hpp"
#include "producers/clause_producers.hpp"
#include "producers/condition_producers.hpp"
#include "producers/cte_producers.hpp"
#include "producers/ddl_producers.hpp"
#include "producers/delete_producers.hpp"
#include "producers/insert_producers.hpp"
#include "producers/join_producers.hpp"
#include "producers/select_producers.hpp"
#include "producers/set_op_producers.hpp"
#include "producers/subquery_producers.hpp"
#include "producers/update_producers.hpp"
#include "query_producer.hpp"

namespace demiplane::test {

    template <db::IsSqlDialect DialectTp>
    class QueryLibrary {
    public:
        explicit QueryLibrary(db::Providers provider)
            : schemas_{TestSchemas::create(provider)},
              compiler_{false} {
        }

        template <IsQueryTag Tag>
        [[nodiscard]] db::CompiledQuery produce() {
            return QueryProducer<Tag>::template produce<DialectTp>(schemas_, compiler_);
        }

        [[nodiscard]] const TestSchemas& schemas() const noexcept {
            return schemas_;
        }

        [[nodiscard]] db::QueryCompiler<DialectTp>& compiler() noexcept {
            return compiler_;
        }

        [[nodiscard]] const db::QueryCompiler<DialectTp>& compiler() const noexcept {
            return compiler_;
        }

    private:
        TestSchemas schemas_;
        db::QueryCompiler<DialectTp> compiler_;
    };

}  // namespace demiplane::test
