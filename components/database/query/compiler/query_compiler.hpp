#pragma once

#include <demiplane/scroll>

#include "param_mode.hpp"
#include "query/compiled_dynamic_query.hpp"
#include "query/compiled_static_query.hpp"
#include "query_visitor/sql_generator_visitor.hpp"

namespace demiplane::db {

    template <IsSqlDialect DialectT, ParamMode DefaultMode = ParamMode::Tuple>
    class QueryCompiler {
        // Auto-resolve: Sink->Tuple for CT, Tuple->Sink for RT
        static constexpr ParamMode StaticDefault  = (DefaultMode == ParamMode::Sink) ? ParamMode::Tuple : DefaultMode;
        static constexpr ParamMode RuntimeDefault = (DefaultMode == ParamMode::Tuple) ? ParamMode::Sink : DefaultMode;

    public:
        constexpr QueryCompiler() = default;

        // Constexpr path -- SQL text + typed parameter tuple
        template <ParamMode Mode = StaticDefault, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        constexpr auto compile_static(const Expression<Expr>& expr) const {
            SqlGeneratorVisitor<DialectT, std::string, Mode> v{};
            auto params = expr.accept(v);
            auto sql    = std::move(v).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        template <ParamMode Mode = StaticDefault, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        constexpr auto compile_static(Expression<Expr>&& expr) const {
            SqlGeneratorVisitor<DialectT, std::string, Mode> v{};
            auto params = std::move(expr).accept(v);
            auto sql    = std::move(v).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        // Runtime path -- full compilation with PMR and params
        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        CompiledDynamicQuery compile_dynamic(const Expression<Expr>& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Inline> v{arena.get()};
                expr.accept(v);
                auto [sql, count] = std::move(v).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> v{bind_packet.sink.get(), arena.get()};
                expr.accept(v);
                auto [sql, count] = std::move(v).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
        }

        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        CompiledDynamicQuery compile_dynamic(Expression<Expr>&& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Inline> v{arena.get()};
                std::move(expr).accept(v);
                auto [sql, count] = std::move(v).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> v{bind_packet.sink.get(), arena.get()};
                std::move(expr).accept(v);
                auto [sql, count] = std::move(v).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
        }
    };
}  // namespace demiplane::db
