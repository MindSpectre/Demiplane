set(DMP_BENCHMARKS ${PROJECT_NAME}.Benchmarks)

if (BUILD_HTTP)
    add_subdirectory(http)
endif ()

add_subdirectory(logger)