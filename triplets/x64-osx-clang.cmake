set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/x64-osx-clang.cmake")

set(VCPKG_C_FLAGS "")
set(VCPKG_CXX_FLAGS "-stdlib=libc++")

# Per-configuration flags
set(VCPKG_C_FLAGS_RELEASE "-O3")
set(VCPKG_CXX_FLAGS_RELEASE "-O3")
set(VCPKG_C_FLAGS_DEBUG "-O0 -g")
set(VCPKG_CXX_FLAGS_DEBUG "-O0 -g")

# === Toolchain settings ===
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
