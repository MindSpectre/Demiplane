##############################################################################
# Database interface Factory class
##############################################################################
add_library(${DMP_DATABASE}.Factory
        STATIC
        factory/db_factory.cpp
        factory/db_factory.hpp
)
target_link_libraries(${DMP_DATABASE}.Factory
        PUBLIC
        Demiplane::Database::Mock::Basic
        Demiplane::Database::Mock::Silent
        Demiplane::Database::Pqxx
)
target_include_directories(${DMP_DATABASE}.Factory
        PUBLIC
        factory
)
##############################################################################


#############################################################################
# Database interface Pool class
#############################################################################

add_library(${DMP_DATABASE}.Pool
        STATIC
        pool/source/db_interface_pool.cpp
        pool/include/db_interface_pool.hpp
        pool/include/db_pool_manager.hpp
        pool/include/db_pool_observer.hpp
        pool/include/db_pool_alert.hpp
)

target_include_directories(${DMP_DATABASE}.Pool
        PUBLIC
        pool/include
)
add_library(Demiplane::Database::Pool ALIAS ${DMP_DATABASE}.Pool)
#############################################################################