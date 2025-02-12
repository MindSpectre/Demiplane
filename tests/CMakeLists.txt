add_executable(${PROJECT_NAME}_Tests_Sandbox
    sandbox/main.cpp
)
target_link_libraries(${PROJECT_NAME}_Tests_Sandbox
PRIVATE
        Demiplane::Database::Base
)