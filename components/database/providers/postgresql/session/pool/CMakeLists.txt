##############################################################################
# PostgreSql Connection Pool
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session.Pool STATIC
        config/pool_config.hpp
        config/pool_config.cpp
        janitor/pool_janitor.hpp
        janitor/pool_janitor.cpp
        connection/connection_pool.hpp
        connection/connection_pool.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session.Pool PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session.Pool
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE}.Base.Errors
        Demiplane::Common::Gears
        Demiplane::Common::Serialization
        Demiplane::Common::Multithread
        PostgreSQL::PostgreSQL
)
##############################################################################
