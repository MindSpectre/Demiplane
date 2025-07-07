##############################################################################
# Test scroll
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Scroll
        logger_provider_test.cpp
        entry_tests.cpp
        main.cpp
        logger/file_logger_test.cpp
        logger/console_logger_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Scroll
        PRIVATE
        Demiplane::Common::Scroll
        ${TEST_LIBS}
)
##############################################################################

###############################################################################
## Test scroll
###############################################################################
#add_unit_test(${UNIT_TESTING_TARGET}.Scroll.ConsoleLogger
#        logger/console_logger_test.cpp
#)
#target_link_libraries(${UNIT_TESTING_TARGET}.Scroll.ConsoleLogger
#        PRIVATE
#        Demiplane::Scroll
#        ${TEST_LIBS}
#)
###############################################################################
#
###############################################################################
## Test scroll
###############################################################################
#add_unit_test(${UNIT_TESTING_TARGET}.Scroll.FileLogger
#        logger/file_logger_test.cpp
#)
#target_link_libraries(${UNIT_TESTING_TARGET}.Scroll.FileLogger
#        PRIVATE
#        Demiplane::Scroll
#        ${TEST_LIBS}
#)
###############################################################################