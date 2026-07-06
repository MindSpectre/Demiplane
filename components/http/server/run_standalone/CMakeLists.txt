##############################################################################
# Http Server — run_standalone ("HTTP owns the process" convenience, spec §9.1)
##############################################################################
add_library(${DMP_HTTP}.Server.RunStandalone STATIC run_standalone.cpp)

target_include_directories(${DMP_HTTP}.Server.RunStandalone PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.RunStandalone
        PUBLIC
        ${DMP_HTTP}.Server.Core
        Boost::asio
)
##############################################################################
