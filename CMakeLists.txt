cmake_minimum_required(VERSION 3.30.5)

project("Demiplane" LANGUAGES CXX)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
set(VCPKG_FEATURE_FLAGS "manifests")
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
# Add options to enable or disable Boost and Qt5
# Toggles
include(cmake/togglers.cmake)
#custom directives
include(cmake/tests.cmake)
include(cmake/combined_library.cmake)
include(cmake/add_resources.cmake)
# Global compiler options (if necessary)
# It's better to set these per-target
# add_compile_options(-pthread -pipe -Wall -fPIC -Werror=return-type)
# Find necessary packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_package(jsoncpp REQUIRED)
#find_package(absl REQUIRED)
find_package(libpqxx REQUIRED)
#find_package(Libevent CONFIG REQUIRED)
#find_package(Drogon CONFIG REQUIRED)
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

    find_package(Boost CONFIG COMPONENTS container system uuid thread lockfree)
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
    message("Components will be builded")
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