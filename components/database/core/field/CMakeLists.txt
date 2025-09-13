##############################################################################
# Column
##############################################################################
add_library(${DMP_DATABASE_CORE}.Field STATIC
        include/db_field.hpp
        include/db_field_schema.hpp
        include/db_field_value.hpp
        source/db_field.cpp
)

target_include_directories(${DMP_DATABASE_CORE}.Field PUBLIC
        include/
)
target_link_libraries(${DMP_DATABASE_CORE}.Field PUBLIC
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################