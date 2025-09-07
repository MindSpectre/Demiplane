cmake_minimum_required(VERSION 3.30.5)

project("Demiplane" LANGUAGES CXX VERSION 0.2)
# Specify the C++ standard

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")


if (${CMAKE_CXX_STANDARD_REQUIRED})
    message("Standard is ${CMAKE_CXX_STANDARD} and required")
else ()
    message("Standard is ${CMAKE_CXX_STANDARD} and not required")
endif ()

message("CXX Flags are ${CMAKE_CXX_FLAGS}")

set(VCPKG_FEATURE_FLAGS "manifests")

message("VCPKG Flags are ${VCPKG_FEATURE_FLAGS}")

set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND general_compile_options
        -O2
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
        -Wdouble-promotion -Wnull-dereference
        -Wnon-virtual-dtor
        -Werror
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
        -fvisibility=hidden
        $<$<CONFIG:Release>:-fomit-frame-pointer>
)

add_compile_options(
        ${general_compile_options}
)

message("Compile options are ${general_compile_options}")

# Add options to enable or disable Boost and Qt5
# Toggles
include(cmake/features.cmake)

# Add compile definitions based on options
if (DMP_ENABLE_LOGGING)
    message("Scroll logging is enabled")
    add_compile_definitions(DMP_ENABLE_LOGGING)
endif ()

if (DMP_COMPONENT_LOGGING)
    message("Component logging is enabled")
    add_compile_definitions(DMP_COMPONENT_LOGGING)
endif ()

#custom directives
include(cmake/tests.cmake)
include(cmake/combined_library.cmake)
include(cmake/add_resources.cmake)

# Find necessary packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(jsoncpp REQUIRED)
#find_package(absl REQUIRED)
#find_package(libpqxx REQUIRED)
#find_package(Libevent CONFIG REQUIRED)
# Conditionally find Boost if enabled


##############################################################################
# Lib includes
##############################################################################
if (USE_BOOST)
    # Boost settings (adjust as needed)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED OFF)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(Boost_NO_SYSTEM_PATHS ON)
    # set(BOOST_ROOT /opt/vcpkg/installed/x64-linux) # Adjust if necessary

    find_package(Boost CONFIG COMPONENTS container thread beast asio)
    if (NOT Boost_FOUND)
        message(FATAL_ERROR "Boost libraries not found, but USE_BOOST is ON.")
    endif ()

endif ()
##############################################################################
add_subdirectory(common)

##############################################################################
# Components
##############################################################################
if (BUILD_COMPONENTS)
    message("Components will be built")
    add_subdirectory(components)
endif ()
##############################################################################

##############################################################################
# TESTS
##############################################################################
if (USE_TESTS)
    message("Tests are enabled")
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    if (NOT GTest_FOUND)
        message(FATAL_ERROR "Gtest libraries not found, but Tests are activated.")
    endif ()
    add_subdirectory(tests)
endif ()
##############################################################################

##############################################################################
# Benchmarks
##############################################################################
if (DO_BENCHMARKS)
    message("Benchmarks are enabled")
    add_subdirectory(benchmarks)
endif ()
##############################################################################