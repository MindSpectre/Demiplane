##############################################################################
# Database General
##############################################################################
add_library(${PROJECT_NAME}_Database INTERFACE)
target_link_libraries(${PROJECT_NAME}_Database
        INTERFACE
        Demiplane::Database::Pool
        Demiplane::Database::Factory
        Demiplane::Database::Transactions
)
add_library(Demiplane::Database ALIAS ${PROJECT_NAME}_Database)
##############################################################################



##############################################################################
# Database main classes NECESSARY UTILITY
##############################################################################
add_library(${PROJECT_NAME}_Database_Base STATIC
        base/field/db_field.hpp
        base/field/db_record.hpp
        base/field/db_factory.hpp
        base/db_shortcuts.hpp
        base/conditions/db_query.hpp
        base/include/db_base.hpp

        base/source/db_base.cpp
        base/conditions/contexts/where.hpp
        base/conditions/contexts/order_by.hpp
        base/conditions/contexts/table.hpp
        base/conditions/contexts/limit_offset.hpp
        base/conditions/contexts/similarity.hpp
        base/conditions/contexts/returning.hpp
)

target_include_directories(${PROJECT_NAME}_Database_Base
        PUBLIC
        base/include/
)
target_link_libraries(${PROJECT_NAME}_Database_Base
        PUBLIC
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
        Demiplane::Database::Base
        Demiplane::Traits
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
        exceptions/db_error_codes.hpp
)
target_include_directories(${PROJECT_NAME}_Database_Exceptions INTERFACE
        exceptions
)
add_library(Demiplane::Database::Exceptions ALIAS ${PROJECT_NAME}_Database_Exceptions)
##############################################################################

##############################################################################


add_subdirectory(creational)

add_subdirectory(behavioral)

#add_subdirectory(postgres/pqxx_impl)

add_subdirectory(mock/basic_mock)

add_subdirectory(mock/silent_mock)

