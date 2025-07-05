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