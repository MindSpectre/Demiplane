#pragma once

#include <query_compiler.hpp>

#include "query_tags.hpp"
#include "test_schemas.hpp"

namespace demiplane::test {

    // Primary template declaration - specializations in producer files
    template <IsQueryTag>
    struct QueryProducer {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s,
                                                db::QueryCompiler<DialectT, DefaultMode>& c) = delete;
    };

}  // namespace demiplane::test
