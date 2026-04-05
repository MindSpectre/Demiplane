set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Self-referencing chainload: vcpkg reads the VCPKG_* vars from here as a triplet,
# then chainloads this same file as a toolchain to apply CMAKE_* vars to each port.
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/x64-linux-clang.cmake")

set(VCPKG_C_FLAGS "")
set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_LINKER_FLAGS "-lc++abi -fuse-ld=mold")

# Per-configuration flags
set(VCPKG_C_FLAGS_RELEASE "-O3")
set(VCPKG_CXX_FLAGS_RELEASE "-O3")
set(VCPKG_C_FLAGS_DEBUG "-O0 -g")
set(VCPKG_CXX_FLAGS_DEBUG "-O0 -g")

# === Toolchain settings (applied when chainloaded by vcpkg for port builds) ===
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi -fuse-ld=mold" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi -fuse-ld=mold" CACHE STRING "" FORCE)

# ccache for port builds
find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif ()
