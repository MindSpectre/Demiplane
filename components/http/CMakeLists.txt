set(DMP_HTTP ${DMP_COMPONENT}.HTTP)


##############################################################################
# Http configs
##############################################################################
add_library(${DMP_HTTP}.Config STATIC
        config/source/cfg.cpp
        config/include/firewall_config.hpp
        config/include/router_config.hpp
        config/include/tls_config.hpp
)
target_include_directories(${DMP_HTTP}.Config PUBLIC
        config/include
)

target_link_libraries(${DMP_HTTP}.Config PUBLIC
        Boost::container
)
##############################################################################


##############################################################################
# Http handler
##############################################################################
add_library(${DMP_HTTP}.Handler STATIC
        http_server/source/controller.cpp
        http_server/source/server.cpp
        http_server/source/request_context.cpp
        http_server/source/route_registry.cpp
        http_server/source/response_factory.cpp
)
target_include_directories(${DMP_HTTP}.Handler PUBLIC
        http_server/include
)

target_link_libraries(${DMP_HTTP}.Handler PUBLIC
        Demiplane::Common::Gears
        Boost::beast
        Boost::asio
)
##############################################################################


##############################################################################
# HTTP Exported library
##############################################################################
add_combined_library(${DMP_HTTP}
        DIRECTORIES
        export/
        LIBRARIES
        ${DMP_HTTP}.Handler
)

add_library(Demiplane::Component::Http ALIAS ${DMP_HTTP})
##############################################################################