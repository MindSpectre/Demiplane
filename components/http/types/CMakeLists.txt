##############################################################################
# Http Types — protocol-agnostic core types
#
# Each type is its own leaf target with its own include directory, so headers
# cross-reference each other as <body.hpp>/<headers.hpp>/... (never ../). The
# dotted ${DMP_HTTP}.Types target below is an INTERFACE aggregate that links
# every leaf — link it when you want the whole layer. There is intentionally
# no Demiplane::Component::Http::Types alias.
##############################################################################
add_subdirectory(enums)
add_subdirectory(executor)
add_subdirectory(async_outcome)
add_subdirectory(url_decode)
add_subdirectory(http_date)
add_subdirectory(headers)
add_subdirectory(errors)
add_subdirectory(body)
add_subdirectory(response)
add_subdirectory(request)
add_subdirectory(response_factory)
add_subdirectory(request_context)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Types INTERFACE)

target_link_libraries(${DMP_HTTP}.Types INTERFACE
        ${DMP_HTTP}.Types.Enums
        ${DMP_HTTP}.Types.Executor
        ${DMP_HTTP}.Types.AsyncOutcome
        ${DMP_HTTP}.Types.UrlDecode
        ${DMP_HTTP}.Types.HttpDate
        ${DMP_HTTP}.Types.Headers
        ${DMP_HTTP}.Types.Errors
        ${DMP_HTTP}.Types.ErrorResponses
        ${DMP_HTTP}.Types.Body
        ${DMP_HTTP}.Types.Response
        ${DMP_HTTP}.Types.Request
        ${DMP_HTTP}.Types.ResponseFactory
        ${DMP_HTTP}.Types.RequestContext
)
##############################################################################
