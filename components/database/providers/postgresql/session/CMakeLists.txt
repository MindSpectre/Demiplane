add_subdirectory(cylinder)

##############################################################################
# PostgreSql Session
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session STATIC
        postgres_session.hpp
        postgres_session.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Session.Cylinder
        ${DMP_DATABASE_POSTGRESQL}.Transaction
)
##############################################################################
