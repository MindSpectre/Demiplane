##############################################################################
# Field Primitive Class
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Field STATIC
        db_field.hpp
        db_field_def.hpp
        db_field_schema.hpp
        db_field_value.hpp
        db_field.cpp
        detail/db_field.inl
)

target_include_directories(${DMP_DATABASE}.Primitives.Field PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE}.Primitives.Field PUBLIC
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################