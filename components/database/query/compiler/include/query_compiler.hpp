#pragma once

#include <demiplane/scroll>

#include "sql_generator_visitor.hpp"


namespace demiplane::db {
    struct CompiledQuery {
        std::string sql;
        std::vector<FieldValue> parameters;
    };

    class QueryCompiler {
    public:
        explicit QueryCompiler(std::shared_ptr<SqlDialect> dialect, const bool use_params = true)
            : dialect_(std::move(dialect)),
              use_parameters_(use_params) {
        }

        // Compile any expression to SQL
        template <IsQuery Expr>
        CompiledQuery compile(Expr&& expr) {
            SqlGeneratorVisitor visitor(dialect_, use_parameters_);
            std::forward<Expr>(expr).accept(visitor);
            COMPONENT_LOG_DBG() << SCROLL_PARAMS(visitor.sql());
            auto [sql, params] = std::move(visitor).decompose();
            return {std::move(sql), std::move(params)};
        }

        // Get dialect for feature checking
        [[nodiscard]] const SqlDialect& dialect() const {
            return *dialect_;
        }

    private:
        std::shared_ptr<SqlDialect> dialect_;
        bool use_parameters_;
    };
}  // namespace demiplane::db
