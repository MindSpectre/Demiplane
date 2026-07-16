##############################################################################
# HTTP/1.1 saturating load generator. Pure Asio client — no demiplane types.
# Links the umbrella for Boost propagation and so a non-test consumer exercises
# Demiplane::Component::Http.
##############################################################################
add_executable(${DMP_BENCHMARKS}.Http.Bomber
        http_bomber.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Http.Bomber
        PRIVATE
        Demiplane::Component::Http
        Threads::Threads
)
##############################################################################

##############################################################################
# The server under load — new-API counterpart the bomber points at.
##############################################################################
add_executable(${DMP_BENCHMARKS}.Http.BenchServer
        bench_server.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Http.BenchServer
        PRIVATE
        Demiplane::Component::Http
)
##############################################################################

##############################################################################
# Drogon reference server — the yardstick bench_server is measured against.
# Requires vcpkg feature 'drogon-benchmarks'.
##############################################################################
find_package(Drogon CONFIG QUIET)

if (Drogon_FOUND)
    message(STATUS "Drogon found — building HTTP reference server")
    add_executable(${DMP_BENCHMARKS}.Http.DrogonBenchServer
            drogon_bench_server.cpp
    )
    target_link_libraries(${DMP_BENCHMARKS}.Http.DrogonBenchServer
            PRIVATE
            Drogon::Drogon
    )
else ()
    message(STATUS "Drogon not found — skipping HTTP reference server (enable vcpkg feature 'drogon-benchmarks')")
endif ()
##############################################################################
