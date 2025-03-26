#pragma once

#include <pqxx/pqxx>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

// Include your query definitions and field types.
#include "db_base.hpp"

namespace demiplane::database {

    // A helper struct to hold a generated SQL string and its associated parameters.
    struct QueryAndParams {
        std::string query;
        pqxx::params params;
        uint32_t param_counter{0};
    };

    /// \brief A processor that converts query::XXX objects to SQL strings + parameters.
    ///
    /// This class does not execute queries. It is solely responsible for translating
    /// the CRTP-based query objects into a valid SQL string with numbered placeholders
    /// and gathering the corresponding values.
    class PqxxQueryEngine {
    public:
        // A very basic escape: wrap the identifier in double quotes.
        static std::string escape_identifier(const std::string_view identifier) {
            return std::string("\"") + std::string(identifier) + "\"";
        }

        // Process a SELECT query.
        static QueryAndParams process_select(const query::SelectQuery& q) {
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
                    oss << escape_identifier(col.get_column_name());
                }
            }
            oss << " FROM " << escape_identifier(q.table());

            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << escape_identifier(clause.name()) << " " << clause.op() << " ";
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
                    oss << escape_identifier(order.column.get_column_name()) << (order.ascending ? " ASC" : " DESC");
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
        static QueryAndParams process_insert(query::InsertQuery q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "INSERT INTO " << escape_identifier(q.table()) << " ";
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
                oss << escape_identifier(field->get_name());
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
                    oss << escape_identifier(col.get_column_name());
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Insert Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process an UPSERT query.
        static QueryAndParams process_upsert(query::UpsertQuery q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "INSERT INTO " << escape_identifier(q.table()) << " ";
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
                oss << escape_identifier(field->get_name());
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
                    oss << escape_identifier(col.get_column_name());
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
                        oss << escape_identifier(col.get_column_name()) << " = EXCLUDED."
                            << escape_identifier(col.get_column_name());
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
                    oss << escape_identifier(col.get_column_name());
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Upsert Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process a DELETE query.
        static QueryAndParams process_delete(const query::DeleteQuery& q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "DELETE FROM " << escape_identifier(q.table());
            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << escape_identifier(clause.name()) << " " << clause.op() << " ";
                    oss << "$" << param_counter++;
                    params.append(clause.value());
                }
            }
            oss << ";";

            // //COMMENTS_TRACE + MESSAGE "Delete Query Generated: " << oss.str();
            return {oss.str(), std::move(params), param_counter};
        }

        // Process a COUNT query.
        static QueryAndParams process_count(const query::CountQuery& q) {
            std::ostringstream oss;
            pqxx::params params;
            uint32_t param_counter = 1;

            oss << "SELECT COUNT(*) FROM " << escape_identifier(q.table());
            if (q.has_where()) {
                oss << " WHERE ";
                bool first = true;
                for (const auto& clause : q.get_where_conditions()) {
                    if (!first) {
                        oss << " AND ";
                    }
                    first = false;
                    oss << escape_identifier(clause.name()) << " " << clause.op() << " ";
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
        static QueryAndParams process_create(const query::CreateQuery& q) {
            std::ostringstream oss;
            // Use q.table() from TableContext mixin.
            oss << "CREATE TABLE " << escape_identifier(q.get_table_name()) << " (";
            bool first = true;
            // Assume q.get_columns() returns a vector of Column objects.
            for (const auto& col : q.get_columns()) {
                if (!first) {
                    oss << ", ";
                }
                first = false;
                oss << escape_identifier(col.get_column_name()) << " " << col.get_sql_type_initialization();
            }
            oss << ");";

            // No parameters for a CREATE TABLE statement.
            // //COMMENTS_TRACE + MESSAGE "Create Table Query Generated: " << oss.str();
            return {oss.str(), {}};
        }

        // A templated helper to process any supported query type.
        template <typename QueryType>
        static QueryAndParams process(const QueryType& query) {
            if constexpr (std::is_same_v<QueryType, query::SelectQuery>) {
                return process_select(query);
            } else if constexpr (std::is_same_v<QueryType, query::InsertQuery>) {
                return process_insert(query);
            } else if constexpr (std::is_same_v<QueryType, query::UpsertQuery>) {
                return process_upsert(query);
            } else if constexpr (std::is_same_v<QueryType, query::DeleteQuery>) {
                return process_delete(query);
            } else if constexpr (std::is_same_v<QueryType, query::CountQuery>) {
                return process_count(query);
            } else if constexpr (std::is_same_v<QueryType, query::CreateQuery>) {
                return process_create(query);
            }
            throw std::invalid_argument("Unsupported query type");
        }
    };

} // namespace demiplane::database
