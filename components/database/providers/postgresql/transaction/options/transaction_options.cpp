#include "transaction_options.hpp"

#include <stdexcept>

namespace demiplane::db::postgres {

    std::string TransactionOptions::to_begin_sql() const {
        if (deferrable && (isolation != IsolationLevel::SERIALIZABLE || access != AccessMode::READ_ONLY)) {
            throw std::invalid_argument("DEFERRABLE is only valid with SERIALIZABLE READ ONLY");
        }

        std::string sql  = "BEGIN";
        sql             += " ISOLATION LEVEL ";
        sql             += to_string(isolation);
        sql             += ' ';
        sql             += to_string(access);

        if (deferrable) {
            sql += " DEFERRABLE";
        }

        return sql;
    }

}  // namespace demiplane::db::postgres
