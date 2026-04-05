cmake_minimum_required(VERSION 3.30.5)

project("Demiplane" LANGUAGES CXX VERSION 0.2)
# Specify the C++ standard

include(cmake/utils.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_CLANG_TIDY
    clang-tidy
    --header-filter=${CMAKE_SOURCE_DIR}/.*
)

# ── Tool versions ────────────────────────────────────────────────────────────
query_tool_version(CLANG_VERSION clang++ --version)
query_tool_version(CMAKE_VER_STR cmake --version)
query_tool_version(NINJA_VERSION ninja --version)
query_tool_version(CCACHE_VERSION ccache --version)
query_tool_version(MOLD_VERSION mold --version)

# vcpkg lives inside the source tree (toolchain is at vcpkg/scripts/buildsystems/vcpkg.cmake)
get_filename_component(_vcpkg_root "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
get_filename_component(_vcpkg_root "${_vcpkg_root}/../.." ABSOLUTE)
vcpkg_commit_hash(VCPKG_COMMIT "${_vcpkg_root}")

PrintLineSeparator()
message("\tTOOLCHAIN")
message("Clang:    ${CLANG_VERSION}")
message("CMake:    ${CMAKE_VER_STR}")
message("Ninja:    ${NINJA_VERSION}")
message("ccache:   ${CCACHE_VERSION}")
message("mold:     ${MOLD_VERSION}")
PrintLineSeparator()

message("\tBUILD CONFIGURATION")
message("Build type:   ${CMAKE_BUILD_TYPE}")
message("C++ standard: ${CMAKE_CXX_STANDARD} (required: ${CMAKE_CXX_STANDARD_REQUIRED})")
message("CXX flags:    ${CMAKE_CXX_FLAGS}")
message("Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message("Compiler:     ${CMAKE_CXX_COMPILER}")
PrintLineSeparator()

message("\tVCPKG")
message("Toolchain: ${CMAKE_TOOLCHAIN_FILE}")
message("Triplet:   ${VCPKG_TARGET_TRIPLET}")
message("Commit:    ${VCPKG_COMMIT}")
PrintLineSeparator()

set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND general_compile_options
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow
        -Wdouble-promotion -Wnull-dereference
        -Wnon-virtual-dtor
        -Werror
        -fstack-protector-strong
        -fvisibility=hidden
        $<$<CONFIG:Release>:-fomit-frame-pointer>
)

add_compile_options(
        ${general_compile_options}
)

message("\tCOMPILE OPTIONS")
message("Compile options are ${general_compile_options}")

# Add options to enable or disable Boost and Qt5
# Toggles
include(cmake/features.cmake)

# Add compile definitions based on options
if (DMP_ENABLE_LOGGING)
    message("Scroll logging is enabled")
    add_compile_definitions(DMP_ENABLE_LOGGING)
    message("Added DMP_ENABLE_LOGGING compile definition")
endif ()

if (DMP_COMPONENT_LOGGING)
    message("Component logging is enabled")
    add_compile_definitions(DMP_COMPONENT_LOGGING)
    message("Added DMP_COMPONENT_LOGGING compile definition")
endif ()
PrintLineSeparator()
#custom directives
include(cmake/tests.cmake)
include(cmake/combined_library.cmake)
include(cmake/add_resources.cmake)

message("\tPACKAGES")
# Find necessary packages
find_package(PkgConfig REQUIRED)
message("Using PkgConfig")
find_package(Threads REQUIRED)
message("Using Threads")
find_package(OpenSSL REQUIRED)
message("Using OpenSSL")
find_package(jsoncpp REQUIRED)
message("Using JsonCpp")
#find_package(absl REQUIRED)
find_package(PostgreSQL REQUIRED)
message("Using PostgreSQL")
#find_package(Libevent CONFIG REQUIRED)

##############################################################################
# Lib includes
##############################################################################

# Boost settings (adjust as needed)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED OFF)
set(Boost_USE_STATIC_RUNTIME ON)
set(Boost_NO_SYSTEM_PATHS ON)
# set(BOOST_ROOT /opt/vcpkg/installed/x64-linux) # Adjust if necessary
list(APPEND boost-libs
        container
        stacktrace_backtrace
        thread
        beast
        asio
        system
)
message("Following Boost libraries will be installed: ${boost-libs}")

find_package(Boost REQUIRED COMPONENTS
        ${boost-libs}
)
if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost libraries are not found.")
endif ()


PrintLineSeparator()
##############################################################################

message("\tPROJECT TARGETS LOGS")

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
PrintLineSeparator()