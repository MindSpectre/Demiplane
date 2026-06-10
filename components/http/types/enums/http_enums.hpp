#pragma once

#include <cstdint>
#include <string_view>

#include <boost/beast/http/verb.hpp>

namespace demiplane::http {

    enum class Protocol : std::uint8_t { http1, http2, http3 };

    enum class HttpMethod : std::uint8_t {
        unknown,
        get,
        post,
        put,
        patch,
        del /* not 'delete' */,
        head,
        options,
    };

    enum class HttpStatus : std::uint16_t {
        ok                     = 200,
        created                = 201,
        accepted               = 202,
        no_content             = 204,
        moved_permanently      = 301,
        found                  = 302,
        see_other              = 303,
        not_modified           = 304,
        temporary_redirect     = 307,
        permanent_redirect     = 308,
        bad_request            = 400,
        unauthorized           = 401,
        forbidden              = 403,
        not_found              = 404,
        method_not_allowed     = 405,
        conflict               = 409,
        gone                   = 410,
        payload_too_large      = 413,
        unsupported_media_type = 415,
        unprocessable_entity   = 422,
        too_many_requests      = 429,
        internal_server_error  = 500,
        not_implemented        = 501,
        bad_gateway            = 502,
        service_unavailable    = 503,
        gateway_timeout        = 504,
    };

    enum class HttpVersion : std::uint8_t {
        http_1_0 = 10,
        http_1_1 = 11,
        http_2   = 20,
        http_3   = 30,
    };

    constexpr std::string_view to_string(HttpMethod m) noexcept {
        switch (m) {
            case HttpMethod::get:
                return "GET";
            case HttpMethod::post:
                return "POST";
            case HttpMethod::put:
                return "PUT";
            case HttpMethod::patch:
                return "PATCH";
            case HttpMethod::del:
                return "DELETE";
            case HttpMethod::head:
                return "HEAD";
            case HttpMethod::options:
                return "OPTIONS";
            case HttpMethod::unknown:
                return "UNKNOWN";
        }
        return "UNKNOWN";
    }

    constexpr HttpMethod method_from_beast(boost::beast::http::verb v) noexcept {
        using V = boost::beast::http::verb;
        switch (v) {
            case V::get:
                return HttpMethod::get;
            case V::post:
                return HttpMethod::post;
            case V::put:
                return HttpMethod::put;
            case V::patch:
                return HttpMethod::patch;
            case V::delete_:
                return HttpMethod::del;
            case V::head:
                return HttpMethod::head;
            case V::options:
                return HttpMethod::options;
            default:
                return HttpMethod::unknown;
        }
    }

    constexpr boost::beast::http::verb method_to_beast(HttpMethod m) noexcept {
        using V = boost::beast::http::verb;
        switch (m) {
            case HttpMethod::get:
                return V::get;
            case HttpMethod::post:
                return V::post;
            case HttpMethod::put:
                return V::put;
            case HttpMethod::patch:
                return V::patch;
            case HttpMethod::del:
                return V::delete_;
            case HttpMethod::head:
                return V::head;
            case HttpMethod::options:
                return V::options;
            case HttpMethod::unknown:
                return V::unknown;
        }
        return V::unknown;
    }

    constexpr HttpVersion version_from_beast(unsigned v) noexcept {
        switch (v) {
            case 10:
                return HttpVersion::http_1_0;
            case 11:
                return HttpVersion::http_1_1;
            case 20:
                return HttpVersion::http_2;
            case 30:
                return HttpVersion::http_3;
            default:
                return HttpVersion::http_1_1;
        }
    }

    constexpr unsigned version_to_beast(HttpVersion v) noexcept {
        return static_cast<unsigned>(v);
    }

}  // namespace demiplane::http
