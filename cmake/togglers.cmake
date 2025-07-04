option(USE_BOOST "Enable Boost library" ON)
option(USE_QT5 "Enable Qt5 library" OFF)
option(USE_PROTOBUF "Enable Protobuf" OFF)
option(BUILD_COMPONENTS "Build components" ON)
option(USE_TESTS "Tests activated" ON)
option(DO_BENCHMARKS "Benchmarks activated" ON)

#Component option
if (BUILD_COMPONENTS)
    option(BUILD_NEXUS "Build nexus" ON)
    option(BUILD_DATABASE "Build database" OFF)
endif ()