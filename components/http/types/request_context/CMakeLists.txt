##############################################################################
# Http Types — RequestContext (accessors, params, bag, ctx factories)
##############################################################################
add_library(${DMP_HTTP}.Types.RequestContext STATIC request_context.cpp)

target_include_directories(${DMP_HTTP}.Types.RequestContext PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.RequestContext
        PUBLIC
        ${DMP_HTTP}.Types.Request
        ${DMP_HTTP}.Types.Response
        ${DMP_HTTP}.Types.Body
        ${DMP_HTTP}.Types.Headers
        ${DMP_HTTP}.Types.Enums
        Boost::container
        PRIVATE
        ${DMP_HTTP}.Types.UrlDecode
)
##############################################################################
