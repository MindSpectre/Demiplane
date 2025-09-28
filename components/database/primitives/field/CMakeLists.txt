##############################################################################
# Field Primitive Class
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Field STATIC
        include/db_field.hpp
        include/db_field_schema.hpp
        include/db_field_value.hpp
        source/db_field.cpp
)

target_include_directories(${DMP_DATABASE}.Primitives.Field PUBLIC
        include/
)
target_link_libraries(${DMP_DATABASE}.Primitives.Field PUBLIC
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################