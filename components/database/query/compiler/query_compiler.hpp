#pragma once

#include <demiplane/scroll>

#include "compiled_query.hpp"
#include "query_visitor/sql_generator_visitor.hpp"

namespace demiplane::db {

    template <IsSqlDialect DialectTp>
    class QueryCompiler {
    public:
        constexpr explicit QueryCompiler(const bool use_params = true)
            : use_parameters_{use_params} {
        }

        // Constexpr path -- SQL text only, values inlined
        template <IsQuery Expr>
        constexpr std::string compile_sql(const Expression<Expr>& expr) const {
            // TODO: params is false by default
            SqlGeneratorVisitor<DialectTp> v{use_parameters_ && false};
            expr.accept(v);
            return std::move(v).sql();
        }

        template <IsQuery Expr>
        constexpr std::string compile_sql(Expression<Expr>&& expr) const {
            // TODO: params is false by default
            SqlGeneratorVisitor<DialectTp> v{use_parameters_ && false};
            std::move(expr).accept(v);
            return std::move(v).sql();
        }

        // Runtime path -- full compilation with PMR and params
        template <IsQuery Expr>
        CompiledQuery compile(const Expression<Expr>& expr) {
            auto arena       = std::make_shared<std::pmr::monotonic_buffer_resource>();
            auto bind_packet = DialectTp::make_param_sink(arena.get());

            SqlGeneratorVisitor<DialectTp, std::pmr::string> v{use_parameters_, bind_packet.sink.get(), arena.get()};
            expr.accept(v);
            auto [sql, count] = std::move(v).decompose();
            COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
            return {std::move(sql), std::move(bind_packet.packet), DialectTp::type(), std::move(arena)};
        }

        template <IsQuery Expr>
        CompiledQuery compile(Expression<Expr>&& expr) {
            auto arena       = std::make_shared<std::pmr::monotonic_buffer_resource>();
            auto bind_packet = DialectTp::make_param_sink(arena.get());

            SqlGeneratorVisitor<DialectTp, std::pmr::string> v{use_parameters_, bind_packet.sink.get(), arena.get()};
            std::move(expr).accept(v);
            auto [sql, count] = std::move(v).decompose();
            COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
            return {std::move(sql), std::move(bind_packet.packet), DialectTp::type(), std::move(arena)};
        }

    private:
        bool use_parameters_ = true;
    };
}  // namespace demiplane::db
