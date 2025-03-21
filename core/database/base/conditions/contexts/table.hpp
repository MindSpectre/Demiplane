#pragma once
#include <string>

namespace demiplane::database::query {
    template <typename Derived>
    class TableContext {
    public:
        TableContext()          = default;
        virtual ~TableContext() = default;
        /**
         *
         * @return Query with set table name
         * @brief this is alias for TableContext::from function for better compatibility(like select from, but insert to)
         * @link TableContext::to act same
         */
        Derived& from(const std::string_view table_name) {
            table_name_ = std::string(table_name);
            return static_cast<Derived&>(*this);
        }

        /**
         *
         * @return Query with set table name
         * @brief this is alias for TableContext::from function for better compatibility(like select from, but insert to)
         */
        Derived& to(const std::string_view table_name) {
            return from(table_name);
        }
        [[nodiscard]] const std::string& table() const noexcept {
            return table_name_;
        }

    protected:
        std::string table_name_;
    };
}
