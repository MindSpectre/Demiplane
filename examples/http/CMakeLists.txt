##############################################################################
# Minimal HTTP server (spec §10.3 standalone path) — living documentation
# for the public <demiplane/http> API. Successor of the pre-redesign manual
# handler example (spec §12.1).
##############################################################################
add_executable(${DMP_EXAMPLES}.Http.MinimalServer
        minimal_http_server.cpp
)
target_link_libraries(${DMP_EXAMPLES}.Http.MinimalServer
        PRIVATE
        Demiplane::Component::Http
)
##############################################################################
