set(DMP_HTTP ${DMP_COMPONENT}.HTTP)


##############################################################################
# HTTP Exported library
##############################################################################
add_combined_library(${DMP_HTTP}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/http
        LIBRARIES
)

add_library(Demiplane::Component::Http ALIAS ${DMP_HTTP})
##############################################################################