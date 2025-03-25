#pragma once

#include "db_base.hpp"
#include "ires.hpp"
#include <boost/container/flat_map.hpp>
namespace demiplane::database {
    class SearchTrait {
    public:
        virtual ~SearchTrait()                                                                               = default;
        [[nodiscard]] virtual IRes<> setup_search_index(std::string_view table_name, FieldCollection fields) = 0;
        [[nodiscard]] virtual IRes<> drop_search_index(std::string_view table_name) const                    = 0;
        [[nodiscard]] virtual IRes<> remove_search_index(std::string_view table_name)                        = 0;
        [[nodiscard]] virtual IRes<> restore_search_index(std::string_view table_name) const                 = 0;

    protected:
        boost::container::flat_map<std::string, FieldCollection> search_fields_;
    };
} // namespace demiplane::database
