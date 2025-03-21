add_library(${PROJECT_NAME}_Traits INTERFACE
        traits_classes.hpp
)
target_include_directories(${PROJECT_NAME}_Traits
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(Demiplane::Traits ALIAS ${PROJECT_NAME}_Traits)