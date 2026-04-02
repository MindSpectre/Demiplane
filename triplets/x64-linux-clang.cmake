set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Use clang with libc++ instead of gcc/libstdc++
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/clang-toolchain.cmake")

set(VCPKG_C_FLAGS "")
set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_LINKER_FLAGS "-lc++abi")

# Per-configuration flags
set(VCPKG_C_FLAGS_RELEASE "-O3")
set(VCPKG_CXX_FLAGS_RELEASE "-O3")
set(VCPKG_C_FLAGS_DEBUG "-O0 -g")
set(VCPKG_CXX_FLAGS_DEBUG "-O0 -g")
