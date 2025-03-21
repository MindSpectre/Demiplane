add_library(${PROJECT_NAME}_Result STATIC
        include/ires.hpp
        source/ires.cpp
)
target_include_directories(${PROJECT_NAME}_Result PUBLIC
       include/
)
#target_link_libraries(${PROJECT_NAME}_Scroll INTERFACE
#        Demiplane::Utilities
#        JsonCpp::JsonCpp
#)
add_library(Demiplane::Result ALIAS ${PROJECT_NAME}_Result)