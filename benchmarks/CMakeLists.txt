set(DMP_BENCHMARKS ${PROJECT_NAME}.Benchmarks)

if (BUILD_HTTP)
    message("Benchmarks for HTTP will be built")
    add_subdirectory(http)
endif ()

add_subdirectory(logger)