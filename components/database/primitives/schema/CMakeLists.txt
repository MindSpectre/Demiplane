##############################################################################
# Schema Primitive - Compile-time schema reflection with member mapping
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Schema INTERFACE
        db_schema_macros.hpp
        db_schema_member.hpp
)

target_include_directories(${DMP_DATABASE}.Primitives.Schema INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE}.Primitives.Schema INTERFACE
        Demiplane::Common::Gears
        ${DMP_DATABASE}.Primitives.Field
)
##############################################################################
