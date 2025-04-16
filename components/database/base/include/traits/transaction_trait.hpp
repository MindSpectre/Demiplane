#pragma once

#include "result.hpp"
namespace demiplane::database {
    struct TransactionTrait {
        virtual ~TransactionTrait()                  = default;
        virtual gears::Result start_transaction()    = 0;
        virtual gears::Result commit_transaction()   = 0;
        virtual gears::Result rollback_transaction() = 0;
    };
} // namespace demiplane::database
