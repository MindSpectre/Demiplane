set(DMP_BENCHMARKS ${PROJECT_NAME}.Benchmarks)

##############################################################################
#
##############################################################################
add_executable(${DMP_BENCHMARKS}.Http.Bomber
        http/http_bomber.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Http.Bomber
        PRIVATE
        Demiplane::Component::Http
)
##############################################################################