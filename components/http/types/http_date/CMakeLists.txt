##############################################################################
# Http Types — RFC 9110 IMF-fixdate rendering (per-second cached Date header)
##############################################################################
add_library(${DMP_HTTP}.Types.HttpDate STATIC http_date.cpp)

target_include_directories(${DMP_HTTP}.Types.HttpDate PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
# Renderer lives in chrono; only the .cpp needs it, so link PRIVATE.
target_link_libraries(${DMP_HTTP}.Types.HttpDate PRIVATE
        Demiplane::Common::Chrono
)
##############################################################################
