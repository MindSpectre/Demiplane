##############################################################################
# Http Types — RFC 9110 IMF-fixdate rendering (per-second cached Date header)
##############################################################################
add_library(${DMP_HTTP}.Types.HttpDate STATIC http_date.cpp)

target_include_directories(${DMP_HTTP}.Types.HttpDate PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
