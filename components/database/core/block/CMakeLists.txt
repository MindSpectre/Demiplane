##############################################################################
# DB Block Interface
##############################################################################
add_library(${DMP_DATABASE_CORE}.Block.Interface INTERFACE
        interface/
        interface/db_block.hpp
)
target_include_directories(${DMP_DATABASE_CORE}.Block.Interface INTERFACE
        interface/
)
target_link_libraries(${DMP_DATABASE_CORE}.Block.Interface INTERFACE
        ${DMP_DATABASE_CORE}.Field
)
##############################################################################