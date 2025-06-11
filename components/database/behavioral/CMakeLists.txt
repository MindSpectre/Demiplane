
###############################################################################
## Database: Behavioral: Operation Strategies for DB
###############################################################################
#add_library(${DMP_DATABASE}.OperationsStrategies
#        operations_strategies/controller/source/strategy_control.cpp
#        operations_strategies/controller/include/strategy_control.hpp
#)
#target_include_directories(${DMP_DATABASE}.OperationsStrategies
#        PUBLIC
#        operations_strategies/controller/include
#        operations_strategies/controller/source
#        operations_strategies/interface
#        operations_strategies/patterns
#)
#target_link_libraries(${DMP_DATABASE}.OperationsStrategies
#        PUBLIC
#        Demiplane::Database::Interface
#)
#add_library(Demiplane::Database::Strategies ALIAS ${DMP_DATABASE}.OperationsStrategies)
###############################################################################


##############################################################################
# Database: Behavioral: transaction manager
##############################################################################
add_library(${DMP_DATABASE}.TransactionManager
        transaction_manager/include/transaction_manager.hpp
        transaction_manager/source/transaction_manager.cpp
)
target_include_directories(${DMP_DATABASE}.TransactionManager
        PUBLIC
        transaction_manager/include
)
target_link_libraries(${DMP_DATABASE}.TransactionManager
        PUBLIC
        ${DMP_DATABASE}.Base
)
add_library(Demiplane::Database::Transactions ALIAS ${DMP_DATABASE}.TransactionManager)
##############################################################################