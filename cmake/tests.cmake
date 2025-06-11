# Core generic function
function(add_custom_test TARGET_NAME)
    set(options "")
    set(oneValueArgs LABEL)
    set(multiValueArgs LINK_LIBS)
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

    # Set custom label or default to "custom"
    if(NOT TEST_LABEL)
        set(TEST_LABEL "custom")
    endif()

    set_tests_properties(${TARGET_NAME}
            PROPERTIES LABELS ${TEST_LABEL})
endfunction()

# Specialized unit test alias with default "unit" label
function(add_unit_test TARGET_NAME)
    add_custom_test(${TARGET_NAME} ${ARGN} LABEL "unit")
endfunction()

function(add_functional_test TARGET_NAME)
    add_custom_test(${TARGET_NAME} ${ARGN} LABEL "functional")
endfunction()

function(add_integration_test TARGET_NAME)
    add_custom_test(${TARGET_NAME} ${ARGN} LABEL "integration")
endfunction()
