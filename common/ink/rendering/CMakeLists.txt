##############################################################################
# Ink Table — value-type fluent builder with column-DSL and row-DSL
##############################################################################
add_library(${DMP_INK}.Table INTERFACE
        table/table.hpp
)
target_include_directories(${DMP_INK}.Table INTERFACE table)
target_link_libraries(${DMP_INK}.Table INTERFACE
        ${DMP_INK}.Base
)
##############################################################################

##############################################################################
# Ink Section — title + (label, value) pairs, multi-line value alignment
##############################################################################
add_library(${DMP_INK}.Section INTERFACE
        section/section.hpp
)
target_include_directories(${DMP_INK}.Section INTERFACE section)
target_link_libraries(${DMP_INK}.Section INTERFACE
        ${DMP_INK}.Base
)
##############################################################################

##############################################################################
# Ink Box — titled bordered block around body
##############################################################################
add_library(${DMP_INK}.Box INTERFACE
        box/box.hpp
)
target_include_directories(${DMP_INK}.Box INTERFACE box)
target_link_libraries(${DMP_INK}.Box INTERFACE
        ${DMP_INK}.Base
)
##############################################################################
