set(CRYPTO Crypto)

##############################################################################
# Hash Creator Interface
##############################################################################
add_library(${PROJECT_NAME}.${CRYPTO}.HashCreator.Interface INTERFACE
        hash_creator/interface/hash_creator_interface.hpp
)

target_include_directories(${PROJECT_NAME}.${CRYPTO}.HashCreator.Interface
        INTERFACE
        hash_creator/interface
)
add_library(Demiplane::Crypto::Hash::Interface ALIAS ${PROJECT_NAME}.${CRYPTO}.HashCreator.Interface)
##############################################################################
# Hash Utilities: SHA256
##############################################################################
add_library(${PROJECT_NAME}.${CRYPTO}.HashCreator.SHA256
        hash_creator/sha_256/source/sha_256.cpp
        hash_creator/sha_256/include/sha_256.h
)

target_include_directories(${PROJECT_NAME}.${CRYPTO}.HashCreator.SHA256 PUBLIC
        hash_creator/sha_256/include
)

target_link_libraries(${PROJECT_NAME}.${CRYPTO}.HashCreator.SHA256
        PUBLIC
        Demiplane::Crypto::Hash::Interface
        OpenSSL::SSL
        OpenSSL::Crypto
)
add_library(Demiplane::Crypto::Hash::Sha256 ALIAS ${PROJECT_NAME}.${CRYPTO}.HashCreator.SHA256)
##############################################################################

##############################################################################
# Hash Utilities: PBKDF2
##############################################################################
add_library(${PROJECT_NAME}.${CRYPTO}.HashCreator.PBKDF2
        hash_creator/pbkdf2/source/pbkdf2.cpp
        hash_creator/pbkdf2/include/pbkdf2.hpp
)

target_include_directories(${PROJECT_NAME}.${CRYPTO}.HashCreator.PBKDF2 PUBLIC
        hash_creator/pbkdf2/include
)

target_link_libraries(${PROJECT_NAME}.${CRYPTO}.HashCreator.PBKDF2
        PUBLIC
        Demiplane::Crypto::Hash::Interface
        OpenSSL::SSL
        OpenSSL::Crypto
)
add_library(Demiplane::Crypto::Hash::PBKDF2 ALIAS ${PROJECT_NAME}.${CRYPTO}.HashCreator.PBKDF2)
##############################################################################

##############################################################################
# Hash Creator Factory
##############################################################################
add_library(${PROJECT_NAME}.${CRYPTO}.HashCreator.Factory STATIC
        hash_creator/factory/hash_creator_factory.cpp
        hash_creator/interface/hash_creator_interface.hpp
)

target_include_directories(${PROJECT_NAME}.${CRYPTO}.HashCreator.Factory PUBLIC
        hash_creator/factory
)

target_link_libraries(${PROJECT_NAME}.${CRYPTO}.HashCreator.Factory
        PUBLIC
        Demiplane::Crypto::Hash::Sha256
        Demiplane::Crypto::Hash::PBKDF2
)
add_library(Demiplane::Crypto::Hash::Factory ALIAS ${PROJECT_NAME}.${CRYPTO}.HashCreator.Factory)
##############################################################################


##############################################################################
# Salt Generator
##############################################################################
add_library(${PROJECT_NAME}.${CRYPTO}.SaltGenerator STATIC
        salt_generator/source/salt_generator.cpp
)

target_include_directories(${PROJECT_NAME}.${CRYPTO}.SaltGenerator PUBLIC
        salt_generator/include
)

target_link_libraries(${PROJECT_NAME}.${CRYPTO}.SaltGenerator
        PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
)
add_library(Demiplane::Crypto::SaltGenerator ALIAS ${PROJECT_NAME}.${CRYPTO}.SaltGenerator)
##############################################################################

add_library(${PROJECT_NAME}.${CRYPTO} INTERFACE)
target_link_libraries(${PROJECT_NAME}.${CRYPTO} INTERFACE
        Demiplane::Crypto::SaltGenerator
        Demiplane::Crypto::Hash::Factory
)
add_library(Demiplane::Crypto ALIAS ${PROJECT_NAME}.${CRYPTO})