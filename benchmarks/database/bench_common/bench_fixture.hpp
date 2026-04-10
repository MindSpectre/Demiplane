#pragma once

#include <libpq-fe.h>

namespace bench::pg {

    /**
     * @brief Connect to PostgreSQL using env vars (raw libpq)
     * @return PGconn* on success, nullptr on failure (prints error to stderr)
     */
    PGconn* connect();

    /**
     * @brief Create and populate the bench table
     * @return true on success
     */
    bool setup_table(PGconn* conn);

    /**
     * @brief Drop the bench table
     */
    void teardown_table(PGconn* conn);

}  // namespace bench::pg
