##############################################################################
# PostgreSql Connection Cylinder
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Session.Cylinder STATIC
        config/cylinder_config.hpp
        config/cylinder_config.cpp
        janitor/cylinder_janitor.hpp
        janitor/cylinder_janitor.cpp
        connection/cylinder_connection.hpp
        connection/cylinder_connection.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Session.Cylinder PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Session.Cylinder
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE}.Base.Errors
        Demiplane::Common::Gears
        Demiplane::Common::Multithread
        PostgreSQL::PostgreSQL
)
##############################################################################
