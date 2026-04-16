##############################################################################
# PostgreSQL Session Shared Types (PoolConfig)
# Consumed by both free_lock/ and blocking/ session variants.
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session.Shared STATIC
        pool_config.hpp
        pool_config.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session.Shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session.Shared
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Config
        Demiplane::Common::Gears
        Demiplane::Common::Serialization
)
##############################################################################
