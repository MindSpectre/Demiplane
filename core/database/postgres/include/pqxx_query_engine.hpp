#pragma once

#include <pqxx/pqxx>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <traits/search_trait.hpp>
#include <traits/table_management_trait.hpp>
#include <traits/unique_constraint_trait.hpp>
// Include your query definitions and field types.

#include "pqxx_configurator.hpp"
namespace demiplane::database {

    // A helper struct to hold a generated SQL string and its associated parameters.
    struct PostgresRequest {
        std::string query;
        pqxx::params params;
        uint32_t param_counter{0};
    };

    /// \brief A processor that converts query::XXX objects to SQL strings + parameters.
    ///
    /// This class does not execute queries. It is solely responsible for translating
    /// the CRTP-based query objects into a valid SQL string with numbered placeholders
    /// and gathering the corresponding values.
    namespace query::engine::postgres {
        namespace detail {
            inline std::string make_fts_index_name(const std::string_view table_name) {
                std::ostringstream fts_ind;
                fts_ind << "fts_" << table_name << "_idx";
                return fts_ind.str();
            }

            inline std::string make_trgm_index_name(const std::string_view table_name) {
                std::ostringstream trgm_ind;
                trgm_ind << "trgm_" << table_name << "_idx";
                return trgm_ind.str();
            }
            inline std::string make_constraint_index_name(const std::string_view table_name) {
                std::ostringstream constraint_ind;
                constraint_ind << "constraint_" << table_name << "_idx";
                return constraint_ind.str();
            }
        } // namespace detail
        namespace util {
            //TODO: possible pass config instead of single bool
            inline std::string escape_string(const std::string_view input, const bool escape_backslash = false) {
                std::ostringstream out;
                out << '\'';
                for (const char ch : input) {
                    switch (ch) {
                    case '\'':
                        out << "''"; // Escape single quote by doubling it
                        break;
                    case '\\':
                        if (escape_backslash) {
                            out << "\\\\";
                        } else {
                            out << '\\';
                        }
                        break;
                    default:
                        // Escape non-printable ASCII
                        if (static_cast<unsigned char>(ch) < 0x20 || static_cast<unsigned char>(ch) == 0x7F) {
                            out << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                                << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec;
                        } else {
                            out << ch;
                        }
                    }
                }

                out << '\'';
                return out.str();
            }
            inline std::string escape_identifier(const std::string_view input) {
                std::ostringstream out;
                out << '"';
                for (const char ch : input) {
                    if (ch == '"') {
                        out << "\"\""; // Escape double quotes by doubling
                    } else {
                        out << ch;
                    }
                }
                out << '"';
                return out.str();
            }

        } // namespace util

        // Process a SELECT query.
        inline PostgresRequest process_select(const SelectQuery& q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "SELECT ";
            if (q.get_select_columns().empty()) {
                oss << "*";
            } else {
                bool first = true;
                for (const auto& col : q.get_select_columns()) {
                    if (!first) {
                        oss << ", ";
                    }
                    first = false;
                    // Assume Column has get_column_name()
                    oss << util::escape_identifier(col.get_column_name());
                }
            }
            oss << " FROM " << util::escape_identifier(q.table());

            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << util::escape_identifier(clause.name()) << " " << clause.op() << " ";
                    oss << "$" << param_counter++;
                    // Append the clause’s value as string.
                    params.append(clause.value());
                }
            }
            if (q.has_order_by()) {
                oss << " ORDER BY ";
                bool first = true;
                for (const auto& order : q.get_order_by_clauses()) {
                    if (!first) {
                        oss << ", ";
                    }
                    first = false;
                    oss << pqxx::connection().quote(order.column.get_column_name())
                        << (order.ascending ? " ASC" : " DESC");
                }
            }
            if (q.has_limit()) {
                oss << " LIMIT " << q.get_limit().value();
            }
            if (q.has_offset()) {
                oss << " OFFSET " << q.get_offset().value();
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Select Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process an INSERT query.
        inline PostgresRequest process_insert(InsertQuery q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "INSERT INTO " << util::escape_identifier(q.table()) << " ";
            auto records = std::move(q).extract_records();
            if (records.empty()) {
                throw std::runtime_error("No records provided for insert");
            }
            // Use the first record to list column names.
            const auto& record = records.front();
            oss << "(";
            bool first = true;
            for (const auto& field : record) {
                if (!first) {
                    oss << ", ";
                }
                first = false;
                oss << util::escape_identifier(field->get_name());
            }
            oss << ") VALUES ";

            // Process each record.
            bool first_record = true;
            for (auto& rec : records) {
                if (!first_record) {
                    oss << ", ";
                }
                first_record = false;
                oss << "(";
                bool first_field = true;
                for (const auto& field : rec) {
                    if (!first_field) {
                        oss << ", ";
                    }
                    first_field = false;
                    // Special handling for UUID fields.
                    if (field->get_sql_type() == SqlType::UUID) {
                        const std::string val = field->to_string();
                        if (val == Uuid::use_generated) {
                            oss << "DEFAULT";
                            continue;
                        }
                        if (val == Uuid::null_value) {
                            oss << "NULL";
                            continue;
                        }
                    }
                    if (q.use_params) {
                        oss << "$" << param_counter++;
                        params.append(field->pull_to_string());
                    } else {
                        oss << field->to_string();
                    }
                }
                oss << ")";
            }
            if (q.has_returning_fields()) {
                oss << " RETURNING ";
                bool first_ret = true;
                for (const auto& col : q.returning_fields()) {
                    if (!first_ret) {
                        oss << ", ";
                    }
                    first_ret = false;
                    oss << util::escape_identifier(col.get_column_name());
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Insert Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process an UPSERT query.
        inline PostgresRequest process_upsert(UpsertQuery q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "INSERT INTO " << util::escape_identifier(q.table()) << " ";
            auto records = std::move(q).extract_records();
            if (records.empty()) {
                throw std::runtime_error("No records provided for upsert");
            }
            const auto& record = records.front();
            oss << "(";
            bool first = true;
            for (const auto& field : record) {
                if (!first) {
                    oss << ", ";
                }
                first = false;
                oss << util::escape_identifier(field->get_name());
            }
            oss << ") VALUES ";
            bool first_record = true;
            for (auto& rec : records) {
                if (!first_record) {
                    oss << ", ";
                }
                first_record = false;
                oss << "(";
                bool first_field = true;
                for (auto& field : rec) {
                    if (!first_field) {
                        oss << ", ";
                    }
                    first_field = false;
                    if (field->get_sql_type() == SqlType::UUID) {
                        const std::string val = field->to_string();
                        if (val == Uuid::use_generated) {
                            oss << "DEFAULT";
                            continue;
                        }
                        if (val == Uuid::null_value) {
                            oss << "NULL";
                            continue;
                        }
                    }
                    if (q.use_params) {
                        oss << "$" << param_counter++;
                        params.append(field->pull_to_string());
                    } else {
                        oss << field->to_string();
                    }
                }
                oss << ")";
            }
            if (!q.get_conflict_columns().empty()) {
                oss << " ON CONFLICT (";
                bool first_conflict = true;
                for (const auto& col : q.get_conflict_columns()) {
                    if (!first_conflict) {
                        oss << ", ";
                    }
                    first_conflict = false;
                    oss << util::escape_identifier(col.get_column_name());
                }
                oss << ") ";
                if (!q.get_update_columns().empty()) {
                    oss << "DO UPDATE SET ";
                    bool first_update = true;
                    for (const auto& col : q.get_update_columns()) {
                        if (!first_update) {
                            oss << ", ";
                        }
                        first_update = false;
                        oss << util::escape_identifier(col.get_column_name()) << " = EXCLUDED."
                            << util::escape_identifier(col.get_column_name());
                    }
                } else {
                    oss << "DO NOTHING";
                }
            }
            if (q.has_returning_fields()) {
                oss << " RETURNING ";
                bool first_ret = true;
                for (const auto& col : q.returning_fields()) {
                    if (!first_ret) {
                        oss << ", ";
                    }
                    first_ret = false;
                    oss << util::escape_identifier(col.get_column_name());
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Upsert Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process a DELETE query.
        inline PostgresRequest process_remove(const RemoveQuery& q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "DELETE FROM " << util::escape_identifier(q.table());
            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << util::escape_identifier(clause.name()) << " " << clause.op() << " ";
                    oss << "$" << param_counter++;
                    params.append(clause.value());
                }
            }
            oss << ";";

            return {oss.str(), std::move(params), param_counter};
        }

        // Process a COUNT query.
        inline PostgresRequest process_count(const CountQuery& q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "SELECT COUNT(*) FROM " << util::escape_identifier(q.table());
            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << util::escape_identifier(clause.name()) << " " << clause.op() << " ";
                    if (q.use_params) {
                        oss << "$" << param_counter++;
                        params.append(clause.value());
                    } else {
                        oss << clause.value();
                    }
                    params.append();
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Count Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process a CREATE TABLE query.
        inline PostgresRequest process_create(const CreateTableQuery& q) {
            std::ostringstream oss;
            // Use q.table() from TableContext mixin.
            oss << "CREATE TABLE " << util::escape_identifier(q.table()) << " (";
            bool first = true;
            // Assume q.get_columns() returns a vector of Column objects.
            for (const auto& col : q.get_columns()) {
                if (!first) {
                    oss << ", ";
                }
                first = false;
                oss << util::escape_identifier(col.get_column_name()) << " " << col.get_sql_type_initialization();
            }
            oss << ");";

            return {oss.str(), {}};
        }

        inline PostgresRequest process_update(const UpdateQuery& q) {
            return {};
        }


        inline std::queue<PostgresRequest> process_set_search_index(const SetIndexQuery& q, const PostgresConfig& config) {
            auto create_fts_index_query = [](std::string_view table_name, const FieldCollection& fts_fields) {
                const std::string table = util::escape_identifier(table_name);
                std::ostringstream fields_stream;
                std::ostringstream index_query;
                for (const auto& field : fts_fields) {
                    fields_stream << "coalesce(" << util::escape_identifier(field->get_name())
                                  << "::text, '') || ' ' || ";
                }
                std::string concatenated = fields_stream.str();
                concatenated.erase(concatenated.size() - 11);
                index_query << "CREATE INDEX IF NOT EXISTS " << detail::make_fts_index_name(table_name) << " ON "
                            << table << " USING gin (to_tsvector('simple', " << concatenated << "));";
                return index_query.str();
            };

            auto create_trgm_index_query = [](std::string_view table_name, const FieldCollection& trgm_fields) {
                const std::string table = util::escape_identifier(table_name);
                std::ostringstream fields_stream;
                std::ostringstream index_query;
                for (const auto& field : trgm_fields) {
                    fields_stream << "coalesce(" << util::escape_identifier(field->get_name())
                                  << "::text, '') || ' ' || ";
                }
                std::string concatenated = fields_stream.str();
                concatenated.erase(concatenated.size() - 11);
                index_query << "CREATE INDEX IF NOT EXISTS " << detail::make_trgm_index_name(table_name) << " ON "
                            << table << " USING gin ((" << concatenated << ") gin_trgm_ops);";
                return index_query.str();
            };
            //TODO: move this to RIME aka rime::unused()
            static_cast<void>(create_trgm_index_query);
            static_cast<void>(create_fts_index_query);
            return {};
        }

        inline std::queue<PostgresRequest> process_drop_search_index(const DropIndexQuery& q, const PostgresConfig& config) {
            //TODO:config check
            std::ostringstream oss;
            oss << "DROP INDEX IF EXISTS " << detail::make_fts_index_name(q.table()) << ";";
            std::ostringstream oss2;
            oss2 << "DROP INDEX IF EXISTS " << detail::make_trgm_index_name(q.table()) << ";";
            std::queue<PostgresRequest> requests;
            requests.push({oss.str()});
            requests.push({oss2.str()});
            return requests;
        }

        inline PostgresRequest process_drop_table(const DropTableQuery& q) {
            std::ostringstream oss;
            oss << "DROP TABLE IF EXISTS " << util::escape_identifier(q.table()) << ";";
            return {oss.str()};
        }

        inline PostgresRequest process_truncate_table(const TruncateTableQuery& q) {
            std::ostringstream oss;
            oss << "TRUNCATE TABLE " << util::escape_identifier(q.table()) << ";";
            return {oss.str()};
        }

        inline PostgresRequest process_check_table(const CheckTableQuery& q) {
            std::ostringstream oss;
            oss << "SELECT to_regclass(" << util::escape_identifier(q.table()) << ");";
            return {oss.str()};
        }

        inline PostgresRequest process_set_unique_constraint(const SetUniqueConstraint& q) {
            std::ostringstream query_stream;
            query_stream << "ALTER TABLE " << util::escape_identifier(q.table())
                         << " ADD CONSTRAINT "
                         << detail::make_constraint_index_name(q.table());
            std::ostringstream sub_stream;
            for (const auto& cols : q.get_unique_columns()) {
                sub_stream << util::escape_identifier(cols.get_column_name()) << ", ";
            }
            std::string columns = sub_stream.str();
            columns.erase(columns.size() - 2);
            query_stream << " UNIQUE (" << columns << ");";
            return {query_stream.str()};
        }

        inline PostgresRequest process_delete_unique_constraint(const DeleteUniqueConstraint& q) {
            std::ostringstream query_stream;

            return {query_stream.str()};
        }
    }; // namespace query::engine::postgres

} // namespace demiplane::database
