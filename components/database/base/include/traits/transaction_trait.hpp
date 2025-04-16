#pragma once

#include "result.hpp"
namespace demiplane::database {
    class TransactionTrait {
    public:
        virtual ~TransactionTrait()           = default;
        virtual Result start_transaction()    = 0;
        virtual Result commit_transaction()   = 0;
        virtual Result rollback_transaction() = 0;
    };
} // namespace demiplane::database
