##############################################################################
# Features Target
##############################################################################
add_library(${DMP_DATABASE}.Features INTERFACE
        providers.hpp
)
target_include_directories(${DMP_DATABASE}.Features INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Provider compile definitions (CMake BUILD_X -> C++ X_ENABLED)
if (BUILD_POSTGRESQL)
    target_compile_definitions(${DMP_DATABASE}.Features INTERFACE POSTGRESQL_ENABLED)
endif ()
##############################################################################