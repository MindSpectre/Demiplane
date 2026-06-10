##############################################################################
# Http Types — Headers (allocator-bound tagged union)
##############################################################################
add_library(${DMP_HTTP}.Types.Headers STATIC headers.cpp)

target_include_directories(${DMP_HTTP}.Types.Headers PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Headers PUBLIC
        Boost::beast
        Boost::container
        Demiplane::Common::Gears
)
##############################################################################
