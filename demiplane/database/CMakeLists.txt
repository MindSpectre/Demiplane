##############################################################################
# Database General
##############################################################################
add_library(${PROJECT_NAME}_Database INTERFACE)
target_link_libraries(${PROJECT_NAME}_Database
        INTERFACE
        Database::Factory
        Database::Pool
        Database::Transactions
)
add_library(Demiplane::Database::Database ALIAS ${PROJECT_NAME}_Database)
##############################################################################



##############################################################################
# Database main classes NECESSARY UTILITY
##############################################################################
add_library(${PROJECT_NAME}_Database_Base INTERFACE
        base/db_conditions.hpp
        base/db_field.hpp
        base/db_record.hpp
        base/db_factory.hpp
        base/db_shortcuts.hpp
)

target_include_directories(${PROJECT_NAME}_Database_Base
        INTERFACE
        base/
)
target_link_libraries(${PROJECT_NAME}_Database_Base
        INTERFACE
        JsonCpp::JsonCpp
)
add_library(Demiplane::Database::Base ALIAS ${PROJECT_NAME}_Database_Base)
##############################################################################

##############################################################################
# Database Client Interface
##############################################################################
add_library(${PROJECT_NAME}_Database_Interface
        INTERFACE
        interface/db_interface.hpp
        interface/db_connect_params.hpp
)

target_link_libraries(${PROJECT_NAME}_Database_Interface
        INTERFACE
        JsonCpp::JsonCpp
        Demiplane::Database::Base
)
target_include_directories(${PROJECT_NAME}_Database_Interface
        INTERFACE
        interface/
)
add_library(Demiplane::Database::Interface ALIAS ${PROJECT_NAME}_Database_Interface)
##############################################################################

##############################################################################
# Error handling
##############################################################################
add_library(${PROJECT_NAME}_Database_Exceptions INTERFACE
        exceptions/db_exceptions.hpp
        exceptions/db_error_codes.hpp)
target_include_directories(${PROJECT_NAME}_Database_Exceptions INTERFACE
        exceptions
)
add_library(Demiplane::Database::Exceptions ALIAS ${PROJECT_NAME}_Database_Exceptions)
##############################################################################

##############################################################################


add_subdirectory(creational)

add_subdirectory(behavioral)

add_subdirectory(postgres/pqxx_impl)

add_subdirectory(mock/basic_mock)

add_subdirectory(mock/silent_mock)

