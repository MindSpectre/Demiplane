option(USE_BOOST "Enable Boost library" ON)
option(BUILD_COMPONENTS "Build components" ON)
option(USE_TESTS "Tests activated" ON)
option(DO_BENCHMARKS "Benchmarks activated" ON)
option(DMP_ENABLE_LOGGING "Enable end user logging" ON)

if (DMP_ENABLE_LOGGING)
    option(DMP_COMPONENT_LOGGING "Enable logging in components" ON)
endif ()

#Component option
if (BUILD_COMPONENTS)
    option(BUILD_HTTP "Build http" ON)
    option(BUILD_DATABASE "Build database" ON)
endif ()

if (BUILD_DATABASE)
    option(BUILD_POSTGRESQL "Build PostgreSQL" ON)
endif ()
