set(DMP_DATABASE_CLIENTS ${DMP_DATABASE}.Clients)

if (BUILD_POSTGRESQL)
    message("PostgreSQL Client will be built")
    add_subdirectory(postgresql)
endif ()