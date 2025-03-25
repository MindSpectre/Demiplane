#pragma once

#include "ires.hpp"
#include "db_base.hpp"
#include "db_exceptions.hpp"
namespace demiplane::database {
    class TransactionTrait {
    public:
        virtual ~TransactionTrait()        = default;
        virtual IRes<> start_transaction() = 0;
        virtual IRes<> commit_transaction() = 0;
        virtual IRes<> rollback_transaction() = 0;\
    };
}