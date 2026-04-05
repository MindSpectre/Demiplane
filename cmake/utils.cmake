set(MSG_SEPARATOR "_______________________________________________________________________________________________________________")

function(PrintLineSeparator)
    message(${MSG_SEPARATOR})
endfunction()

# query_tool_version(<variable> <program> [<args>...])
# Runs `<program> <args>` and stores the first line of output in <variable>.
# If the program is not found, stores "not found".
function(query_tool_version out_var program)
    # Use a unique cache variable per program to avoid find_program caching issues
    string(TOUPPER "${program}" _qtv_upper)
    string(REPLACE "+" "X" _qtv_upper "${_qtv_upper}")
    set(_qtv_cache_var "_QTV_${_qtv_upper}_EXE")

    find_program(${_qtv_cache_var} ${program})
    if (NOT ${_qtv_cache_var})
        set(${out_var} "not found" PARENT_SCOPE)
        return()
    endif ()
    execute_process(
            COMMAND ${${_qtv_cache_var}} ${ARGN}
            OUTPUT_VARIABLE _qtv_out
            ERROR_VARIABLE _qtv_out
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _qtv_rc
    )
    if (NOT _qtv_rc EQUAL 0)
        set(${out_var} "unknown" PARENT_SCOPE)
        return()
    endif ()
    # Keep only the first line
    string(REGEX REPLACE "\n.*" "" _qtv_first "${_qtv_out}")
    set(${out_var} "${_qtv_first}" PARENT_SCOPE)
endfunction()

# vcpkg_commit_hash(<variable> <vcpkg_root>)
# Reads the current HEAD commit of the vcpkg checkout.
function(vcpkg_commit_hash out_var vcpkg_root)
    find_program(_vch_git git)
    if (NOT _vch_git OR NOT EXISTS "${vcpkg_root}/.git")
        set(${out_var} "unknown" PARENT_SCOPE)
        return()
    endif ()
    execute_process(
            COMMAND ${_vch_git} -C "${vcpkg_root}" rev-parse --short HEAD
            OUTPUT_VARIABLE _vch_out
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _vch_rc
    )
    if (NOT _vch_rc EQUAL 0)
        set(${out_var} "unknown" PARENT_SCOPE)
        return()
    endif ()
    set(${out_var} "${_vch_out}" PARENT_SCOPE)
endfunction()