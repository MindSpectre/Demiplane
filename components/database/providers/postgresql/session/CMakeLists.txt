add_subdirectory(shared)
add_subdirectory(free_lock)
add_subdirectory(blocking)

add_combined_library(${DMP_DATABASE_POSTGRESQL}.Session
        DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        SOURCES
        postgres_session.hpp
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Session.FreeLock
        ${DMP_DATABASE_POSTGRESQL}.Session.Blocking
)