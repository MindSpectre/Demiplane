#include "db_column.hpp"

#include "db_field_schema.hpp"
namespace demiplane::db {

    DynamicColumn::DynamicColumn(std::string name, std::string table)
        : name_{std::move(name)},
          context_{std::move(table)} {
    }
    DynamicColumn::DynamicColumn(std::string name)
        : name_{std::move(name)} {
    }
    const std::string& DynamicColumn::name() const {
        return name_;
    }
    const std::string& DynamicColumn::context() const {
        return context_;
    }
    DynamicColumn& DynamicColumn::set_context(std::string table) {
        context_ = std::move(table);
        return *this;
    }
    DynamicColumn& DynamicColumn::set_name(std::string name) {
        name_ = std::move(name);
        return *this;
    }

    AllColumns::AllColumns(std::shared_ptr<std::string> table)
        : table_(std::move(table)) {
    }
    AllColumns::AllColumns(std::string table)
        : table_(std::make_shared<std::string>(std::move(table))) {
    }
    const std::string& AllColumns::table_name() const {
        return *table_;
    }
    const std::shared_ptr<std::string>& AllColumns::table() const {
        return table_;
    }
    DynamicColumn AllColumns::as_dynamic() const {
        return DynamicColumn{"*", table_name()};
    }
}  // namespace demiplane::db
