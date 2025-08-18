add_executable(${PROJECT_NAME}.Tests.Sandbox
        main.cpp
)
target_link_libraries(${PROJECT_NAME}.Tests.Sandbox
        PRIVATE
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)