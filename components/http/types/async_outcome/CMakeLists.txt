##############################################################################
# Http Types — AsyncOutcome / AsyncResponse aliases (header-only)
##############################################################################
add_library(${DMP_HTTP}.Types.AsyncOutcome INTERFACE async_outcome.hpp)

target_include_directories(${DMP_HTTP}.Types.AsyncOutcome INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.AsyncOutcome INTERFACE
        Boost::asio
        Demiplane::Common::Gears
)
##############################################################################
