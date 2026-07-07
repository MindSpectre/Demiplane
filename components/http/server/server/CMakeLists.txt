##############################################################################
# Http Server — Server (executor-injected lifecycle orchestrator, spec §9)
##############################################################################
add_library(${DMP_HTTP}.Server.Core STATIC server.cpp)

target_include_directories(${DMP_HTTP}.Server.Core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.Core
        PUBLIC
        ${DMP_HTTP}.Server.Observer
        ${DMP_HTTP}.Routing
        ${DMP_HTTP}.Listeners
        ${DMP_HTTP}.Config
        Boost::asio
        Demiplane::Common::Chrono
        Demiplane::Common::Gears
        Demiplane::Common::Nexus
        Demiplane::Common::Scroll
)
##############################################################################
