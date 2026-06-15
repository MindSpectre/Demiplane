##############################################################################
# Http Drivers — Http11Driver (real HTTP/1.1 over Boost.Beast)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http11 STATIC
        http11_driver.cpp
        http11_driver.hpp
        http11_config.hpp
)

target_include_directories(${DMP_HTTP}.Drivers.Http11 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http11
        PUBLIC
        ${DMP_HTTP}.Types
        ${DMP_HTTP}.Routing.Router
        ${DMP_HTTP}.Connection.Concepts
        Boost::beast
        Boost::asio
)
##############################################################################
