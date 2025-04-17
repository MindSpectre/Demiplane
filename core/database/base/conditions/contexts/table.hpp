#pragma once
#include <string>
#define SILENT_TABLE_CONSTRUCTOR(ClassName)                                             \
    explicit ClassName(const std::string_view table_name) : TableContext(table_name) {}
namespace demiplane::database::query {
    template <typename Derived>
    class TableContext {
    public:
        TableContext()          = default;
        virtual ~TableContext() = default;

        explicit TableContext(std::string table_name) : table_name_(std::move(table_name)) {}
        explicit TableContext(const std::string_view table_name) : table_name_(table_name) {}
        /**
         *
         * @return Query with set table name
         * @brief this is alias for TableContext::from function for better compatibility(like select from, but insert
         * to)
         */
        Derived& table(const std::string_view name) {
            table_name_ = name;
            return static_cast<Derived&>(*this);
        }

        Derived& table(const std::string& name) {
            table_name_ = name;
            return static_cast<Derived&>(*this);
        }

        Derived& table(const char* name) {
            table_name_ = name;
            return static_cast<Derived&>(*this);
        }

        [[nodiscard]] std::string_view table() const noexcept {
            return table_name_;
        }


    protected:
        std::string table_name_;
    };
} // namespace demiplane::database::query
