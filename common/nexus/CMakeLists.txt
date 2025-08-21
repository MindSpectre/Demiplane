set(DMP_NEXUS ${DMP_COMMON}.Nexus)
##############################################################################
# Nexus policies classes
##############################################################################
add_library(${DMP_NEXUS}.Policies INTERFACE
        policies/policies.hpp
)
target_include_directories(${DMP_NEXUS}.Policies INTERFACE
        policies/
)
##############################################################################

# Nexus core files and structs
##############################################################################
add_library(${DMP_NEXUS}.Core STATIC
        core/nexus_traits.hpp
        core/source/nexus.cpp
        core/include/nexus.hpp
        core/details.hpp
        core/source/nexus.inl
)
target_include_directories(${DMP_NEXUS}.Core
        PUBLIC
        core/include
        PRIVATE
        core/
)
target_link_libraries(${DMP_NEXUS}.Core PUBLIC
        Boost::thread
        ${DMP_NEXUS}.Policies
        PRIVATE
        Demiplane::Common::Chrono
)
##############################################################################


##############################################################################
# Nexus exported library
##############################################################################
add_combined_library(${DMP_NEXUS}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/nexus
        LIBRARIES
        ${DMP_NEXUS}.Core
)
add_library(Demiplane::Common::Nexus ALIAS ${DMP_NEXUS})
##############################################################################


