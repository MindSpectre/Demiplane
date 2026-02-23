# Core generic function
function(add_custom_test TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LINK_LIBS FIXTURES ENVIRONMENT LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Positional arguments are sources
    set(SOURCES ${TEST_UNPARSED_ARGUMENTS})

    if(NOT SOURCES)
        message(FATAL_ERROR "add_custom_test(${TARGET_NAME}) requires at least one source file")
    endif()

    # Create executable
    add_executable(${TARGET_NAME} ${SOURCES})

    # Link libraries if provided
    if(TEST_LINK_LIBS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${TEST_LINK_LIBS})
    endif()

    # Register as CTest
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})

    # Set labels or default to "custom"
    if(NOT TEST_LABELS)
        set(TEST_LABELS "custom")
    endif()

    # Set properties directly with proper quoting to preserve list values
    set_tests_properties(${TARGET_NAME} PROPERTIES LABELS "${TEST_LABELS}")

    # Add fixtures if provided
    if(TEST_FIXTURES)
        set_tests_properties(${TARGET_NAME} PROPERTIES FIXTURES_REQUIRED "${TEST_FIXTURES}")
    endif()

    # Add environment variables if provided
    if(TEST_ENVIRONMENT)
        set_tests_properties(${TARGET_NAME} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
    endif()
endfunction()

function(add_manual_test TARGET_NAME)
    # Parse arguments to extract existing LABELS
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LINK_LIBS FIXTURES ENVIRONMENT LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Merge labels
    set(MERGED_LABELS "manual" ${TEST_LABELS})

    # Reconstruct arguments with merged labels
    add_custom_test(${TARGET_NAME} ${TEST_UNPARSED_ARGUMENTS}
        LINK_LIBS ${TEST_LINK_LIBS}
        FIXTURES ${TEST_FIXTURES}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LABELS ${MERGED_LABELS}
    )
endfunction()

function(add_unit_test TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LINK_LIBS FIXTURES ENVIRONMENT LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(MERGED_LABELS "unit" ${TEST_LABELS})

    add_custom_test(${TARGET_NAME} ${TEST_UNPARSED_ARGUMENTS}
        LINK_LIBS ${TEST_LINK_LIBS}
        FIXTURES ${TEST_FIXTURES}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LABELS ${MERGED_LABELS}
    )
endfunction()

function(add_functional_test TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LINK_LIBS FIXTURES ENVIRONMENT LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(MERGED_LABELS "functional" ${TEST_LABELS})

    add_custom_test(${TARGET_NAME} ${TEST_UNPARSED_ARGUMENTS}
        LINK_LIBS ${TEST_LINK_LIBS}
        FIXTURES ${TEST_FIXTURES}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LABELS ${MERGED_LABELS}
    )
endfunction()

function(add_integration_test TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LINK_LIBS FIXTURES ENVIRONMENT LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(MERGED_LABELS "integration" ${TEST_LABELS})

    add_custom_test(${TARGET_NAME} ${TEST_UNPARSED_ARGUMENTS}
        LINK_LIBS ${TEST_LINK_LIBS}
        FIXTURES ${TEST_FIXTURES}
        ENVIRONMENT ${TEST_ENVIRONMENT}
        LABELS ${MERGED_LABELS}
    )
endfunction()


