set(DMP_DATABASE ${DMP_COMPONENT}.Database)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_library(${DMP_DATABASE}.Core STATIC
        core/include/db_field_schema.hpp
        core/include/db_record.hpp
        core/include/db_field.hpp
        core/include/db_record_factory.hpp
        core/source/db_table_schema.cpp
        core/include/db_field_value.hpp
        core/source/db_field.cpp
        core/source/db_record.cpp
        core/source/db_table_schema.inl
        core/source/db_field.inl
        core/include/db_column.hpp
        core/include/db_core_fwd.hpp
)

target_include_directories(${DMP_DATABASE}.Core PUBLIC
        core/include
)
target_link_libraries(${DMP_DATABASE}.Core PUBLIC
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################


add_subdirectory(dialects)

add_subdirectory(query)

add_subdirectory(clients)