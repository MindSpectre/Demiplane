##############################################################################
# Http Server — ServerObserver (single typed observer interface, header-only)
##############################################################################
add_library(${DMP_HTTP}.Server.Observer INTERFACE server_observer.hpp)

target_include_directories(${DMP_HTTP}.Server.Observer INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.Observer INTERFACE
        ${DMP_HTTP}.Routing.Router
        ${DMP_HTTP}.Types
        Boost::asio
)
##############################################################################
