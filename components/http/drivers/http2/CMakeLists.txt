##############################################################################
# Http Drivers — Http2Driver (SCAFFOLD; nghttp2 not linked yet)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Http2 INTERFACE
        http2_driver.hpp
)

target_include_directories(${DMP_HTTP}.Drivers.Http2 INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Http2 INTERFACE
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
)
##############################################################################
