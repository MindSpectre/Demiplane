##############################################################################
# Http Connection — RequestArena (per-connection monotonic arena)
##############################################################################
add_library(${DMP_HTTP}.Connection.RequestArena STATIC
        request_arena.cpp
)

target_include_directories(${DMP_HTTP}.Connection.RequestArena PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.RequestArena PUBLIC
        Demiplane::Common::Gears
)
##############################################################################
