#include "bench_fixture.hpp"

#include <iostream>

#include "bench_config.hpp"
#include "bench_constants.hpp"

namespace bench::pg {

    PGconn* connect() {
        const auto connstr = make_connection_string();
        PGconn* conn       = PQconnectdb(connstr.c_str());

        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Failed to connect to PostgreSQL: " << PQerrorMessage(conn) << "\n";
            std::cerr << "Set environment variables: POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, "
                         "POSTGRES_USER, POSTGRES_PASSWORD\n";
            PQfinish(conn);
            return nullptr;
        }

        return conn;
    }

    bool setup_table(PGconn* conn) {
        PGresult* res = PQexec(conn, CREATE_TABLE);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "Failed to create table: " << PQerrorMessage(conn) << "\n";
            PQclear(res);
            return false;
        }
        PQclear(res);

        res = PQexec(conn, INSERT_DATA);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "Failed to insert data: " << PQerrorMessage(conn) << "\n";
            PQclear(res);
            return false;
        }
        PQclear(res);

        res = PQexec(conn, ANALYZE_BENCH);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "Failed to analyze table: " << PQerrorMessage(conn) << "\n";
            PQclear(res);
            return false;
        }
        PQclear(res);

        return true;
    }

    void teardown_table(PGconn* conn) {
        PGresult* res = PQexec(conn, DROP_TABLE);
        PQclear(res);
    }

}  // namespace bench::pg
