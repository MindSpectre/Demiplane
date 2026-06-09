##############################################################################
# Http Types — protocol-agnostic core types
##############################################################################
add_library(${DMP_HTTP}.Types STATIC)

target_include_directories(${DMP_HTTP}.Types PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types
        PUBLIC
        Demiplane::Common::Gears
        Demiplane::Common::Scroll
        Boost::beast
        Boost::asio
        Boost::container
        JsonCpp::JsonCpp
)

# Linkable alias for unit tests and early consumers. The component umbrella
# (Demiplane::Component::Http) aggregates only the old Handler lib until PR 7,
# so it does NOT propagate this layer's include dirs.
add_library(Demiplane::Component::Http::Types ALIAS ${DMP_HTTP}.Types)

# Source files added in subsequent tasks via target_sources.
target_sources(${DMP_HTTP}.Types PRIVATE types_placeholder.cpp)
target_sources(${DMP_HTTP}.Types PRIVATE url_decode/url_decode.cpp)
target_sources(${DMP_HTTP}.Types PRIVATE headers/headers.cpp)
##############################################################################
