set(DMP_BENCHMARKS ${PROJECT_NAME}.Benchmarks)

add_subdirectory(logger)

add_subdirectory(multithread)

if (BUILD_DATABASE)
    message("Benchmarks for Database will be built")
    add_subdirectory(database)
endif ()