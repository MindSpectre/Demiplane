#pragma once
#include <list>

#include "field/db_record.hpp"

namespace demiplane::database {
    template <typename T>
    using ArrayField      = Field<std::vector<T>>;
    using TextField       = Field<std::string>;
    using IndexField      = Field<Uuid>;
    using SharedFieldPtr  = std::shared_ptr<FieldBase>;
    using UniqueFieldPtr  = std::unique_ptr<FieldBase>;
    using FieldCollection = std::list<std::shared_ptr<FieldBase>>;
    using Records         = std::vector<Record>;
    using Columns         = std::vector<Column>;
} // namespace demiplane::database
