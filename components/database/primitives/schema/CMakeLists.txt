##############################################################################
# Schema Primitive - Compile-time schema reflection with member mapping
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Schema INTERFACE)

target_include_directories(${DMP_DATABASE}.Primitives.Schema INTERFACE
        include/
)
target_link_libraries(${DMP_DATABASE}.Primitives.Schema INTERFACE
        Demiplane::Common::Gears
        ${DMP_DATABASE}.Primitives.Field
)
##############################################################################
