##############################################################################
# Http Types — Body (value-type SBO type-erasure)
##############################################################################
add_library(${DMP_HTTP}.Types.Body STATIC body.cpp)

target_include_directories(${DMP_HTTP}.Types.Body PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Body
        PUBLIC
        ${DMP_HTTP}.Types.AsyncOutcome
        ${DMP_HTTP}.Types.Errors
        Boost::asio
        Demiplane::Common::Gears
        JsonCpp::JsonCpp
        PRIVATE
        ${DMP_HTTP}.Types.UrlDecode
)
##############################################################################
