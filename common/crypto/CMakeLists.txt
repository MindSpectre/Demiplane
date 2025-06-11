set(DMP_CRYPTO ${DMP_COMMON}.Crypto)



##############################################################################
# Hash Utilities: SHA256
##############################################################################
add_library(${DMP_CRYPTO}.Hasher STATIC
        hasher/source/hasher.cpp
        hasher/source/salt_generator.cpp
)

target_include_directories(${DMP_CRYPTO}.Hasher PUBLIC
        hasher/include
)

target_link_libraries(${DMP_CRYPTO}.Hasher
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
)
add_library(Demiplane::Crypto::Hasher ALIAS ${DMP_CRYPTO}.Hasher)
##############################################################################




##############################################################################
# Crypto utilities
##############################################################################
add_library(${DMP_CRYPTO}.Utilities INTERFACE)

target_include_directories(${DMP_CRYPTO}.Utilities
        INTERFACE
        utilities/
)
##############################################################################


add_combined_library(${DMP_CRYPTO}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_CRYPTO}.Utilities
        ${DMP_CRYPTO}.Hasher
        ALIAS
        Demiplane::Crypto
)