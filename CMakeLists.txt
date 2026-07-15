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

# Toggles. Included before the compile options because DMP_KEEP_FRAME_POINTERS
# decides one of them.
include(cmake/features.cmake)

list(APPEND general_compile_options
        -Wall -Wextra -Wpedantic
        -Wthread-safety
        -Wconversion -Wshadow
        -Wdouble-promotion -Wnull-dereference
        -Wnon-virtual-dtor
        -Werror
        -fno-stack-protector
        -fvisibility=hidden
        -g
)

# Target compile options land after CMAKE_CXX_FLAGS on the command line, so a
# preset that merely adds -fno-omit-frame-pointer to the flags loses to this.
# The `release-perf` preset flips this option instead.
if (DMP_KEEP_FRAME_POINTERS)
    list(APPEND general_compile_options -fno-omit-frame-pointer)
else ()
    list(APPEND general_compile_options $<$<CONFIG:Release>:-fomit-frame-pointer>)
endif ()

add_compile_options(
        ${general_compile_options}
)

message("\tCOMPILE OPTIONS")
message("Compile options are ${general_compile_options}")

# Boost.Asio recycles handler/coroutine memory in a per-thread, per-purpose slot
# cache. The default of 2 slots is too few for the h1 hot path, which frees three
# differently-sized awaitable frames per request (Router::dispatch, the handler,
# write_response); on a size miss thread_info_base::allocate EVICTS a cached chunk
# and falls back to operator new.
#
# This MUST be global. `max_mem_index = 6 * cache_size` sizes thread_info_base
# itself, which is embedded in io_context. Two TUs disagreeing on the value give
# that struct two layouts — silent memory corruption, not a link error. So it goes
# here rather than on any single target, and Demiplane.Component.HTTP re-exports it
# INTERFACE so external consumers of the umbrella inherit it too.
set(DMP_ASIO_RECYCLE_CACHE "4" CACHE STRING "Asio per-thread recycled-memory slots per purpose tag")
add_compile_definitions(BOOST_ASIO_RECYCLING_ALLOCATOR_CACHE_SIZE=${DMP_ASIO_RECYCLE_CACHE})
message("Asio recycling allocator cache size: ${DMP_ASIO_RECYCLE_CACHE} (tree-wide; ABI-visible, see comment)")

# Fail compilation on any use of asio's deprecated interfaces (io_service,
# null_buffers, executor query shims, ...). Keeps the tree on the modern API
# surface — a prerequisite for the newer completion machinery (immediate
# executors) and for future asio upgrades.
add_compile_definitions(BOOST_ASIO_NO_DEPRECATED)

# GEARS_UNRECOVERABLE_NOEXCEPT (gears_macros.hpp) → noexcept: allocation
# failure in functions so marked terminates instead of unwinding. ABI-visible
# for the same reason as the recycling-cache size above — tree-wide, never
# per-target.
if (DMP_UNRECOVERABLE_EXCEPTIONS_TERMINATE)
    add_compile_definitions(DMP_UNRECOVERABLE_EXCEPTIONS_TERMINATE)
    message("Unrecoverable exceptions terminate: GEARS_UNRECOVERABLE_NOEXCEPT = noexcept (tree-wide; ABI-visible)")
endif ()

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
        unordered
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

##############################################################################
# Examples
##############################################################################
if (BUILD_EXAMPLES)
    message("Examples are enabled")
    add_subdirectory(examples)
endif ()
##############################################################################
PrintLineSeparator()