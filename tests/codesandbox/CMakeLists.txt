add_executable(${PROJECT_NAME}.Tests.Sandbox
        main.cpp
        static_constexpr_audit.cpp
)
target_link_libraries(${PROJECT_NAME}.Tests.Sandbox
        PRIVATE
        Demiplane::Tests::Shared::Database
)