##############################################################################
# Common utilities can be used outside
##############################################################################
add_library(${PROJECT_NAME}_Utilities
        INTERFACE
        include/chrono_utils.hpp
        include/security_utils.hpp
        include/compile_time_utils.hpp
        include/random_utils.hpp
)
target_include_directories(${PROJECT_NAME}_Utilities
        INTERFACE
        include/
)
add_library(Demiplane::Utilities ALIAS ${PROJECT_NAME}_Utilities)

##############################################################################