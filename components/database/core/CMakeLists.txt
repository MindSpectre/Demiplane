##############################################################################
# Fundamental structures for orm
##############################################################################
add_library(${DMP_DATABASE}.Core STATIC
        column/source/db_column.cpp
        field/source/db_field.cpp
        table/source/db_table_schema.cpp
        row/source/db_record.cpp
        row/include/db_record_factory.hpp
        export/db_core_objects.hpp
        row/interface/row.hpp
)

target_include_directories(${DMP_DATABASE}.Core PUBLIC
        export
        column/include
        field/include
        row/include
        table/include
)
target_link_libraries(${DMP_DATABASE}.Core PUBLIC
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################