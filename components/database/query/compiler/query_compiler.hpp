#pragma once

#include <demiplane/scroll>

#include "compiled_query/compiled_dynamic_query.hpp"
#include "compiled_query/compiled_static_query.hpp"
#include "param_mode.hpp"
#include "query_visitor/sql_generator_visitor.hpp"

namespace demiplane::db {

    template <IsSqlDialect DialectT, ParamMode DefaultMode = ParamMode::Tuple>
    class QueryCompiler {
        // Auto-resolve: Sink->Tuple for CT, Tuple->Sink for RT
        static constexpr ParamMode StaticDefault  = (DefaultMode == ParamMode::Sink) ? ParamMode::Tuple : DefaultMode;
        static constexpr ParamMode RuntimeDefault = (DefaultMode == ParamMode::Tuple) ? ParamMode::Sink : DefaultMode;

    public:
        constexpr QueryCompiler() = default;

        // Constexpr path -- InlineString, no heap allocation, true static constexpr
        template <ParamMode Mode = StaticDefault, std::size_t MaxLen = 1024, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        [[nodiscard]] constexpr auto compile_static(const Expression<Expr>& expr) const {
            SqlGeneratorVisitor<DialectT, gears::InlineString<MaxLen>, Mode> visitor{};
            auto params = expr.accept(visitor);
            auto sql    = std::move(visitor).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        template <ParamMode Mode = StaticDefault, std::size_t MaxLen = 1024, IsQuery Expr>
            requires(Mode != ParamMode::Sink)
        [[nodiscard]] constexpr auto compile_static(Expression<Expr>&& expr) const {
            SqlGeneratorVisitor<DialectT, gears::InlineString<MaxLen>, Mode> visitor{};
            auto params = std::move(expr).accept(visitor);
            auto sql    = std::move(visitor).sql();
            return CompiledStaticQuery{std::move(sql), std::move(params)};
        }

        // Runtime path -- full compilation with PMR and params
        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        [[nodiscard]] CompiledDynamicQuery compile_dynamic(const Expression<Expr>& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string> visitor{arena.get()};
                expr.accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else if constexpr (Mode == ParamMode::Sink) {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> visitor{bind_packet.sink.get(),
                                                                                         arena.get()};
                expr.accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
            std::unreachable();
        }

        template <ParamMode Mode = RuntimeDefault, IsQuery Expr>
            requires(Mode != ParamMode::Tuple)
        [[nodiscard]] CompiledDynamicQuery compile_dynamic(Expression<Expr>&& expr) {
            auto arena = std::make_shared<std::pmr::monotonic_buffer_resource>();

            if constexpr (Mode == ParamMode::Inline) {
                SqlGeneratorVisitor<DialectT, std::pmr::string> visitor{arena.get()};
                std::move(expr).accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), nullptr, DialectT::type(), std::move(arena)};
            } else if (Mode == ParamMode::Sink) {
                auto bind_packet = DialectT::make_param_sink(arena.get());
                SqlGeneratorVisitor<DialectT, std::pmr::string, ParamMode::Sink> visitor{bind_packet.sink.get(),
                                                                                         arena.get()};
                std::move(expr).accept(visitor);
                auto [sql, count] = std::move(visitor).decompose();
                COMPONENT_LOG_TRC() << SCROLL_PARAMS(sql);
                return {std::move(sql), std::move(bind_packet.packet), DialectT::type(), std::move(arena)};
            }
            std::unreachable();
        }
    };
}  // namespace demiplane::db
