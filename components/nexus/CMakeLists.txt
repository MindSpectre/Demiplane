set(DMP_NEXUS ${DMP_COMPONENT}.Nexus)

add_library(${DMP_NEXUS}.Policies INTERFACE
        policies/include/life_policy.hpp
        policies/include/access_policy.hpp)
target_include_directories(${DMP_NEXUS}.Policies INTERFACE
        policies/include
)
add_library(${DMP_NEXUS}.Pylon INTERFACE
        pylon/view_pylon.hpp
        pylon/unique_pylon.hpp
        pylon/safe_pylon.hpp
        pylon/basic_pylon.hpp
        pylon/include/pylon.hpp
)
target_include_directories(${DMP_NEXUS}.Pylon INTERFACE
        pylon/include
)
target_link_libraries(${DMP_NEXUS}.Pylon INTERFACE
        Demiplane::Gears
)

add_library(${DMP_NEXUS}.Core STATIC
        core/nexus_traits.hpp
        core/source/nexus.cpp
        core/nexus_fwd.hpp
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
        ${DMP_NEXUS}.Policies
        ${DMP_NEXUS}.Pylon
        Demiplane::Scroll::Tracer
)

##############################################################################
# Nexus components
##############################################################################
add_combined_library(${DMP_NEXUS}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_NEXUS}.Core
        ALIAS
        Demiplane::Component::Nexus
)
##############################################################################


