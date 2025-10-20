#pragma once

#include <demiplane/scroll>

#include "compiled_query.hpp"
#include "sql_generator_visitor.hpp"

namespace demiplane::db {
    class QueryCompiler {
    public:
        explicit constexpr  QueryCompiler(std::shared_ptr<SqlDialect> dialect, const bool use_params = true)
            : dialect_(std::move(dialect)),
              use_parameters_(use_params) {
        }

        // Compile any expression to SQL
        template <IsQuery Expr>
        CompiledQuery compile(const Expression<Expr>& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();
            SqlGeneratorVisitor v{dialect_, arena.get(), use_parameters_};
            expr.accept(v);
            auto [sql, pkt] = std::move(v).decompose();
            COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
            return {std::move(sql), std::move(pkt), dialect_->type(), std::move(arena)};
        }

        template <IsQuery Expr>
        CompiledQuery compile(Expression<Expr>&& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();
            SqlGeneratorVisitor v{dialect_, arena.get(), use_parameters_};
            std::move(expr).accept(v);
            auto [sql, pkt] = std::move(v).decompose();
            COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
            return {std::move(sql), std::move(pkt), dialect_->type(), std::move(arena)};
        }

        // Get dialect for feature checking
        [[nodiscard]] constexpr const SqlDialect& dialect() const {
            return *dialect_;
        }

    private:
        std::shared_ptr<SqlDialect> dialect_;
        bool use_parameters_;
    };
}  // namespace demiplane::db
