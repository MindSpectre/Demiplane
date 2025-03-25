#pragma once

#pragma once

#include "db_base.hpp"
#include "ires.hpp"
#include <boost/container/flat_map.hpp>
namespace demiplane::database {
    class UniqueConstraintTrait {
    public:
        virtual ~UniqueConstraintTrait()                                                               = default;
        virtual IRes<> make_unique_constraint(std::string_view table_name, FieldCollection key_fields) = 0;
        [[nodiscard]] const boost::container::flat_map<std::string, FieldCollection>& get_conflict_fields() const {
            return conflict_fields_;
        }
        void set_conflict_fields(boost::container::flat_map<std::string, FieldCollection> conflict_fields) {
            conflict_fields_ = std::move(conflict_fields);
        }

    protected:
        boost::container::flat_map<std::string, FieldCollection> conflict_fields_;
    };
} // namespace demiplane::database
