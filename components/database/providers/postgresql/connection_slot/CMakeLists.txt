##############################################################################
# PostgreSql Connection Slot (INTERFACE — shared by executors and pool)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.ConnectionSlot INTERFACE
        connection_slot.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.ConnectionSlot INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.ConnectionSlot INTERFACE
        Demiplane::Common::Gears
        PostgreSQL::PostgreSQL
)
##############################################################################