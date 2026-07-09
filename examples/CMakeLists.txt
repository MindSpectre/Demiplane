set(DMP_EXAMPLES ${PROJECT_NAME}.Examples)

if (BUILD_HTTP)
    message("Examples for HTTP will be built")
    add_subdirectory(http)
endif ()
