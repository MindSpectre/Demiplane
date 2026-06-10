##############################################################################
# Http Types — built-in error structs (header-only) + to_http_response defs
##############################################################################
# Error structs + arena-free to_http_response DECLARATIONS. Header-only and
# deliberately light (depends only on Enums) so Body can use the error structs
# without pulling in Response/ResponseFactory — that would form a target cycle
# (Response -> Body -> Errors -> Response).
add_library(${DMP_HTTP}.Types.Errors INTERFACE errors.hpp)

target_include_directories(${DMP_HTTP}.Types.Errors INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Errors INTERFACE
        ${DMP_HTTP}.Types.Enums
)

# to_http_response DEFINITIONS (cold path). Separate target so the Response /
# ResponseFactory dependency stays off the struct-only Errors target above.
add_library(${DMP_HTTP}.Types.ErrorResponses STATIC errors.cpp)

target_link_libraries(${DMP_HTTP}.Types.ErrorResponses
        PUBLIC
        ${DMP_HTTP}.Types.Errors
        PRIVATE
        ${DMP_HTTP}.Types.Response
        ${DMP_HTTP}.Types.ResponseFactory
)
##############################################################################
