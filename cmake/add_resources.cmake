# LinkResources.cmake
# Requires CMake 3.29+, Linux only

function(link_resources target)
    cmake_parse_arguments(RES
            "INSTALL_ONLY"
            ""
            ""          # no named args
            ${ARGN}     # positional args are file/glob patterns
    )

    # 1) Expand globs and collect sources
    set(_srcs "")
    foreach(_pat IN LISTS RES_UNPARSED_ARGUMENTS)
        if(_pat MATCHES "[*?\\[]")
            file(GLOB_RECURSE _matches CONFIGURE_DEPENDS "${_pat}")
            list(APPEND _srcs ${_matches})
        else()
            list(APPEND _srcs "${_pat}")
        endif()
    endforeach()

    # 2) If this is a library, stash these in an INTERFACE property
    get_target_property(_type ${target} TYPE)
    if(_type MATCHES "STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY")
        get_target_property(_old RES_LIST ${target} PROPERTY INTERFACE_RESOURCE_FILES)
        set(_new ${_old} ${_srcs})
        set_property(TARGET ${target}
                PROPERTY INTERFACE_RESOURCE_FILES "${_new}"
        )
        # And if user only wants install-time, register install(FILES)…
        if(RES_INSTALL_ONLY)
            foreach(_f IN LISTS _srcs)
                get_filename_component(_n ${_f} NAME)
                install(FILES "${_f}"
                        DESTINATION $<TARGET_FILE_DIR:${target}>
                        RENAME "${_n}"
                )
            endforeach()
        endif()
        return()
    endif()

    # 3) Otherwise (executable or module) — do post-build or install copy
    foreach(_f IN LISTS _srcs)
        get_filename_component(_n ${_f} NAME)
        if(RES_INSTALL_ONLY)
            install(FILES "${_f}"
                    DESTINATION $<TARGET_FILE_DIR:${target}>
                    RENAME "${_n}"
            )
        else()
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_f}"
                    "$<TARGET_FILE_DIR:${target}>/${_n}"
                    COMMENT "Copy resource ${_n} → $<TARGET_FILE_DIR:${target}>"
            )
        endif()
    endforeach()

    # 4) “Propagate” library resources: look at all linked libs and copy THEIR INTERFACE_RESOURCE_FILES
    if(_type STREQUAL "EXECUTABLE" OR _type STREQUAL "MODULE_LIBRARY")
        get_target_property(_libs ${target} LINK_LIBRARIES)
        foreach(_lib IN LISTS _libs)
            get_target_property(_libres ${_lib} PROPERTY INTERFACE_RESOURCE_FILES)
            if(_libres)
                foreach(_lf IN LISTS _libres)
                    get_filename_component(_ln ${_lf} NAME)
                    add_custom_command(TARGET ${target} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_lf}"
                            "$<TARGET_FILE_DIR:${target}>/${_ln}"
                            COMMENT "Copy lib(${_lib}) resource ${_ln}"
                    )
                endforeach()
            endif()
        endforeach()
    endif()
endfunction()
