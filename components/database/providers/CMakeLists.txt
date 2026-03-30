set(DMP_DATABASE_PROVIDERS ${DMP_DATABASE}.Providers)

if (BUILD_POSTGRESQL)
    message("PostgreSQL Client will be built")
    add_subdirectory(postgresql)
endif ()