#pragma once

#include <compiled_query.hpp>
#include <query_compiler.hpp>

#include "query_tags.hpp"
#include "test_schemas.hpp"

namespace demiplane::test {

    // Primary template declaration - specializations in producer files
    template <IsQueryTag Tag>
    struct QueryProducer {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) = delete;
    };

}  // namespace demiplane::test
