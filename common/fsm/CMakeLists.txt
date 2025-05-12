set(DMP_FSM ${DMP_COMMON}.FSM)

##############################################################################
# FSM Parsers
##############################################################################
add_library(${DMP_FSM}.Core INTERFACE
    core/state.hpp
        core/transition.hpp
)

target_include_directories(${DMP_FSM}.Core
        INTERFACE
        core/
)
#target_link_libraries(${DMP_FSM}.Core
#        INTERFACE
#        JsonCpp::JsonCpp
#)
##############################################################################

##############################################################################
# FSM Parsers
##############################################################################
add_library(${DMP_FSM}.Parser INTERFACE
        parser/parser.hpp
        parser/shema_checker.hpp)

target_include_directories(${DMP_FSM}.Parser
        INTERFACE
        parser/
)
target_link_libraries(${DMP_FSM}.Parser
    INTERFACE
        JsonCpp::JsonCpp
        ${DMP_FSM}.Core
)
##############################################################################

##############################################################################
# Machine
##############################################################################
add_library(${DMP_FSM}.Machine STATIC
        machines/source/machine.cpp
        machines/include/machine.hpp
)

target_include_directories(${DMP_FSM}.Machine
        PUBLIC
        machines/include
)
target_link_libraries(${DMP_FSM}.Machine
        PRIVATE
        ${DMP_FSM}.Parser
)
##############################################################################


add_combined_library(${DMP_FSM}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_FSM}.Machine
        ALIAS
        Demiplane::FSM
)