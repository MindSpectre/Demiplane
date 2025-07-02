function(add_combined_library TargetName)
    cmake_parse_arguments(ARG
            ""
            "DIRECTORIES"
            "LIBRARIES"
            ${ARGN}
    )

    add_library(${TargetName} INTERFACE)

    if(ARG_DIRECTORIES)
        target_include_directories(${TargetName}
                INTERFACE
                ${ARG_DIRECTORIES}
        )
    endif()

    if(ARG_LIBRARIES)
        target_link_libraries(${TargetName}
                INTERFACE
                ${ARG_LIBRARIES}
        )
    endif()
endfunction()
