set(DMP_DATABASE ${DMP_COMPONENT}.Database)

##############################################################################
# Database General
##############################################################################
add_combined_library(${DMP_DATABASE}
        DIRECTORIES
        export
        LIBRARIES
        Demiplane::Database::Pool
        Demiplane::Database::Transactions

        Demiplane::Database::Mock::Basic
        Demiplane::Database::Mock::Silent

        ${DMP_DATABASE}.Factory
        ALIAS
        Demiplane::Database
)
##############################################################################


##############################################################################
# Database main classes NECESSARY UTILITY
##############################################################################
add_library(${DMP_DATABASE}.Core STATIC
        core/field/db_field.hpp
        core/field/db_record.hpp
        core/field/db_column.hpp
        core/field/db_factory.hpp
        core/db_shortcuts.hpp
        core/conditions/db_query.hpp

        core/source/db_core.cpp
        core/conditions/contexts/where.hpp
        core/conditions/contexts/order_by.hpp
        core/conditions/contexts/table.hpp
        core/conditions/contexts/limit_offset.hpp
        core/conditions/contexts/similarity.hpp
        core/conditions/contexts/returning.hpp
)

target_include_directories(${DMP_DATABASE}.Core
        PUBLIC
        core/include
)
target_link_libraries(${DMP_DATABASE}.Core
        PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Gears
)
##############################################################################

##############################################################################
# Database Client Interface
##############################################################################
add_library(${DMP_DATABASE}.Base
        STATIC
        base/db_connect_params.hpp
        base/db_config_interface.hpp
        base/include/db_base.hpp
        base/include/traits/transaction_trait.hpp
        base/include/traits/table_management_trait.hpp
        base/include/traits/unique_constraint_trait.hpp
        base/include/traits/search_trait.hpp
)

target_link_libraries(${DMP_DATABASE}.Base
        PUBLIC
        Demiplane::Scroll
        ${DMP_DATABASE}.Core
        ${DMP_DATABASE}.Exceptions
)
target_include_directories(${DMP_DATABASE}.Base
        PUBLIC
        base/include
)
##############################################################################

##############################################################################
# Error handling
##############################################################################
add_library(${DMP_DATABASE}.Exceptions INTERFACE
        exceptions/db_exceptions.hpp
        exceptions/db_error_codes.hpp
)
target_include_directories(${DMP_DATABASE}.Exceptions INTERFACE
        exceptions
)
##############################################################################

##############################################################################


add_subdirectory(creational)

add_subdirectory(behavioral)

add_subdirectory(mock)

add_subdirectory(postgres)