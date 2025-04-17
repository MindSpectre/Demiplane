##############################################################################
# Database interface Factory class
##############################################################################
add_library(${PROJECT_NAME}.${DATABASE}.Factory
        STATIC
        factory/db_interface_factory.cpp
        factory/db_interface_factory.hpp
)
target_link_libraries(${PROJECT_NAME}.${DATABASE}.Factory
        PUBLIC
        Demiplane::Database::Mock::Basic
        Demiplane::Database::Mock::Silent
        Demiplane::Database::Postgres
)
target_include_directories(${PROJECT_NAME}.${DATABASE}.Factory
        PUBLIC
        factory/
)
add_library(Demiplane::Database::Factory ALIAS ${PROJECT_NAME}.${DATABASE}.Factory)
##############################################################################


#############################################################################
# Database interface Pool class
#############################################################################

add_library(${PROJECT_NAME}.${DATABASE}.Pool
        STATIC
        pool/source/db_interface_pool.cpp
        pool/include/db_interface_pool.hpp
        pool/include/db_pool_manager.hpp
        pool/include/db_pool_observer.hpp
        pool/include/db_pool_alert.hpp
)
target_link_libraries(${PROJECT_NAME}.${DATABASE}.Pool
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Database::Factory
)
target_include_directories(${PROJECT_NAME}.${DATABASE}.Pool
        PUBLIC
        pool/include/
)
add_library(Demiplane::Database::Pool ALIAS ${PROJECT_NAME}.${DATABASE}.Pool)
#############################################################################