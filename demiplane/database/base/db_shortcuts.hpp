#pragma once
#include "db_field.hpp"
#include "db_record.hpp"

namespace common::database {
    template <typename T>
    using ArrayField = Field<std::vector<T>>;
    using TextField = Field<std::string>;
    using IndexField = Field<Uuid>;
    using SharedFieldPtr     = std::shared_ptr<FieldBase>;
    using UniqueFieldPtr     = std::unique_ptr<FieldBase>;
    using FieldCollection = std::vector<std::shared_ptr<FieldBase>>;
    using Records = std::vector<Record>;
    using ViewRecords = std::vector<std::unique_ptr<ViewRecord>>;
} // namespace common::database
