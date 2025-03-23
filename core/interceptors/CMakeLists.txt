add_library(${PROJECT_NAME}.Interceptors INTERFACE
        interceptors.hpp
)
target_include_directories(${PROJECT_NAME}.Interceptors INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
add_library(Demiplane::Interceptors ALIAS ${PROJECT_NAME}.Interceptors)