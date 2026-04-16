##############################################################################
# PostgreSQL Blocking Session (mutex + CV + FIFO waiter pool)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session.Blocking STATIC
        postgres_blocking_session.hpp
        postgres_blocking_session.cpp
        pool/blocking_pool.hpp
        pool/blocking_pool.cpp
        pool/queued_holder.hpp
        pool/queued_holder.cpp
        pool/waiter.hpp
        pool/async_waiter.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session.Blocking PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session.Blocking
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionHolder
        ${DMP_DATABASE_POSTGRESQL}.Session.Shared
        ${DMP_DATABASE_POSTGRESQL}.Transaction
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE}.Base.Errors
        Demiplane::Common::Gears
        PostgreSQL::PostgreSQL
)
##############################################################################
