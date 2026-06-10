##############################################################################
# Http Routing — registry, controllers, bake step, groups, router
#
# Same convention as types/: each thing is its own leaf target owning its
# include dir (headers cross-reference as <route_registry.hpp>, never ../).
# The dotted ${DMP_HTTP}.Routing target below is an INTERFACE aggregate.
##############################################################################
add_subdirectory(middleware)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Routing INTERFACE)

target_link_libraries(${DMP_HTTP}.Routing INTERFACE
        ${DMP_HTTP}.Routing.Middleware
)
##############################################################################
