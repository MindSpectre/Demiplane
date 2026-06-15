##############################################################################
# Http Drivers — Http3Driver (SCAFFOLD; ngtcp2 + nghttp3 not linked yet)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http3 INTERFACE
        http3_driver.hpp
)

target_include_directories(${DMP_HTTP}.Drivers.Http3 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http3 INTERFACE
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
)
##############################################################################
