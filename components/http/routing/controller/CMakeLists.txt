##############################################################################
# Http Routing — HttpController + verb DSL + bake step
##############################################################################
add_library(${DMP_HTTP}.Routing.Controller STATIC controller.cpp)

target_include_directories(${DMP_HTTP}.Routing.Controller PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Controller
        PUBLIC
        ${DMP_HTTP}.Routing.Middleware
        ${DMP_HTTP}.Types.Errors
        # collapse_outcome instantiates ADL to_http_response calls in consumer
        # TUs — the built-in definitions must travel with this target:
        ${DMP_HTTP}.Types.ErrorResponses
        Demiplane::Common::Gears
        Demiplane::Common::Nexus
        Demiplane::Common::Scroll
        PRIVATE
        ${DMP_HTTP}.Routing.RouteRegistry
)
##############################################################################
