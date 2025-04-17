###############################################
#
###############################################
add_library(${PROJECT_NAME}.Result INTERFACE
        result/ires.hpp
)
target_include_directories(${PROJECT_NAME}.Result INTERFACE
        result
)

add_library(Demiplane::Result ALIAS ${PROJECT_NAME}.Result)
###############################################
add_library(${PROJECT_NAME}.Traits INTERFACE
        traits/traits_classes.hpp
)
target_include_directories(${PROJECT_NAME}.Traits
        INTERFACE
        traits/
)

add_library(Demiplane::Traits ALIAS ${PROJECT_NAME}.Traits)
###############################################

##############################################################################
# Common utilities can be used outside
##############################################################################
add_library(${PROJECT_NAME}.Utilities
        INTERFACE
        utilities/
)
target_include_directories(${PROJECT_NAME}.Utilities
        INTERFACE
        utilities/
)
add_library(Demiplane::Utilities ALIAS ${PROJECT_NAME}.Utilities)

##############################################################################

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${PROJECT_NAME}.Stopwatch INTERFACE
        stopwatch/stopwatch.hpp
)
target_include_directories(${PROJECT_NAME}.Stopwatch
        INTERFACE
        stopwatch/
)
target_link_libraries(${PROJECT_NAME}.Stopwatch
        INTERFACE
        Demiplane::Scroll
)
add_library(Demiplane::Stopwatch ALIAS ${PROJECT_NAME}.Stopwatch)
##############################################################################

add_subdirectory(crypto)

add_subdirectory(database)

add_subdirectory(multithread)

add_subdirectory(scroll)

#add_subdirectory(interceptors)

add_subdirectory(3rdparty)

# TODO: monitor development got delayed due to design problems
#add_subdirectory(monitor)

