##############################################################################
# Http Types — static cold-path ResponseFactory
##############################################################################
add_library(${DMP_HTTP}.Types.ResponseFactory STATIC response_factory.cpp)

target_include_directories(${DMP_HTTP}.Types.ResponseFactory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.ResponseFactory PUBLIC
        ${DMP_HTTP}.Types.Response
        ${DMP_HTTP}.Types.Enums
)
##############################################################################
