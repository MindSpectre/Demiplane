set(DMP_HTTP ${DMP_COMPONENT}.HTTP)


##############################################################################
# Http Types layer (new, PR 1 of redesign)
##############################################################################
add_subdirectory(types)
##############################################################################


##############################################################################
# Http Routing layer (PR 2 of redesign)
##############################################################################
add_subdirectory(routing)
##############################################################################


##############################################################################
# Http Connection layer (PR 3 of redesign)
##############################################################################
add_subdirectory(connection)
##############################################################################


##############################################################################
# Http Drivers layer (PR 3 of redesign)
##############################################################################
add_subdirectory(drivers)
##############################################################################


##############################################################################
# Http Config layer (PR 6 of redesign)
##############################################################################
add_subdirectory(config)
##############################################################################


##############################################################################
# Http Listeners layer (PR 4 of redesign)
##############################################################################
add_subdirectory(listeners)
##############################################################################


##############################################################################
# Http Server layer (PR 5 of redesign)
##############################################################################
add_subdirectory(server)
##############################################################################


##############################################################################
# HTTP Exported library
##############################################################################
add_combined_library(${DMP_HTTP}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/http
        LIBRARIES
        ${DMP_HTTP}.Types
        ${DMP_HTTP}.Routing
        ${DMP_HTTP}.Connection
        ${DMP_HTTP}.Drivers
        ${DMP_HTTP}.Listeners
        ${DMP_HTTP}.Config
        ${DMP_HTTP}.Server
)

# Asio's thread_info_base is sized by this macro (max_mem_index = 6 * cache_size)
# and lives inside io_context. A consumer TU that compiles it at the default 2
# against ours gets a differently-laid-out struct — silent corruption. The root
# CMakeLists sets it tree-wide; this re-exports it to anyone linking the umbrella.
# Both read DMP_ASIO_RECYCLE_CACHE, so they cannot drift.
target_compile_definitions(${DMP_HTTP} INTERFACE
        BOOST_ASIO_RECYCLING_ALLOCATOR_CACHE_SIZE=${DMP_ASIO_RECYCLE_CACHE}
)

add_library(Demiplane::Component::Http ALIAS ${DMP_HTTP})
##############################################################################