##############################################################################
# Http Server — orchestration layer: the ServerObserver interface, the Server
# (executor-injected lifecycle orchestrator), and run_standalone convenience.
# Per-leaf convention; the dotted ${DMP_HTTP}.Server target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(server_observer)
add_subdirectory(server)
add_subdirectory(run_standalone)
add_subdirectory(attach_default_listeners)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Server INTERFACE)

target_link_libraries(${DMP_HTTP}.Server INTERFACE
        ${DMP_HTTP}.Server.Observer
        ${DMP_HTTP}.Server.Core
        ${DMP_HTTP}.Server.RunStandalone
        ${DMP_HTTP}.Server.AttachDefaultListeners
)
##############################################################################
