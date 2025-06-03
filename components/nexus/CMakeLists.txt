set(DMP_NEXUS ${DMP_COMPONENT}.Nexus)
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

)
##############################################################################

##############################################################################
# Nexus provider
##############################################################################
add_library(${DMP_NEXUS}.Provider INTERFACE
    provider/nexus_provider.hpp
)
target_include_directories(${DMP_NEXUS}.Provider INTERFACE
    provider/
)

target_link_libraries(${DMP_NEXUS}.Provider INTERFACE
    ${DMP_NEXUS}.Core
)
##############################################################################


##############################################################################
# Nexus components
##############################################################################
add_combined_library(${DMP_NEXUS}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_NEXUS}.Provider
        ALIAS
        Demiplane::Component::Nexus
)
#add_library(Demiplane::Component::Nexus ALIAS  ${DMP_NEXUS})
##############################################################################


