set(DMP_BENCHMARKS ${PROJECT_NAME}.Benchmarks)

if (BUILD_HTTP)
    message("Benchmarks for HTTP will be built")
    add_subdirectory(http)
endif ()

add_subdirectory(logger)

add_subdirectory(disruptor)

if (BUILD_DATABASE)
    message("Benchmarks for Database will be built")
    add_subdirectory(database)
endif ()