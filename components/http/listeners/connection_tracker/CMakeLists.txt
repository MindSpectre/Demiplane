##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${DMP_HTTP}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${DMP_HTTP}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.ConnectionTracker
        PUBLIC
        ${DMP_HTTP}.Types.Executor
        Boost::asio
        Demiplane::Common::Gears
)
##############################################################################
