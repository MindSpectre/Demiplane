##############################################################################
# PostgreSQL Free-Lock Session (ring buffer pool, fail-fast acquire)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session.FreeLock STATIC
        postgres_free_lock_session.hpp
        postgres_free_lock_session.cpp
        pool/connection_pool.hpp
        pool/connection_pool.cpp
        pool/slot_holder.hpp
        pool/pool_janitor.hpp
        pool/pool_janitor.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session.FreeLock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session.FreeLock
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionHolder
        ${DMP_DATABASE_POSTGRESQL}.Session.Shared
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE_POSTGRESQL}.Transaction
        ${DMP_DATABASE}.Base.Errors
        Demiplane::Common::Gears
        Demiplane::Common::Serialization
        Demiplane::Common::Multithread
        PostgreSQL::PostgreSQL
)
##############################################################################
