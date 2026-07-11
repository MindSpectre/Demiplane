#include "http11_driver.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <utility>

namespace demiplane::http {

    RequestContext detail::build_request_context(Http11Request& req, const std::pmr::polymorphic_allocator<> arena) {
        Request request{Headers::view_of_beast(req.base())};
        request.method  = method_from_beast(req.method());
        request.version = version_from_beast(req.version());
        request.target  = std::string_view{req.target().data(), req.target().size()};

        const auto& body = req.body();
        request.body     = Body::beast_view(std::span{reinterpret_cast<const std::byte*>(body.data()), body.size()});

        return RequestContext{std::move(request), arena};
    }

    detail::Http11Response detail::make_beast_response(Response& resp) {
        namespace http = boost::beast::http;

        // Field nodes are bump-allocated out of resp's arena. Beast's default
        // std::allocator would malloc/free one per header, per response.
        const std::pmr::polymorphic_allocator<char> field_alloc{resp.alloc.resource()};
        Http11Response msg{std::piecewise_construct, std::forward_as_tuple(), std::forward_as_tuple(field_alloc)};
        msg.result(static_cast<unsigned>(resp.status));
        msg.version(version_to_beast(resp.version));
        for (const auto& [name, value] : resp.headers)
            msg.insert(name, value);

        // TODO
        //  v1 writes only non-streaming responses, so buffered_view() is always
        //  present (Empty/Owned/BeastView). A future streaming body returns
        //  nullopt here — assert rather than silently truncate to an empty body.
        assert(resp.body.buffered_view().has_value() &&
               "make_beast_response: streaming response bodies not supported yet");
        const std::string_view bytes = resp.body.buffered_view().value_or(std::string_view{});
        msg.body().data              = const_cast<char*>(bytes.data());
        msg.body().size              = bytes.size();
        msg.body().more              = false;
        msg.content_length(bytes.size());  // set() semantics — overrides any Content-Length header
        msg.keep_alive(resp.keep_alive);   // sets the Connection header
        return msg;
    }

    bool detail::is_malformed_request(const boost::beast::error_code& ec) noexcept {
        namespace he = boost::beast::http;
        return ec == he::error::bad_line_ending || ec == he::error::bad_method || ec == he::error::bad_target ||
               ec == he::error::bad_version || ec == he::error::bad_field || ec == he::error::bad_value ||
               ec == he::error::bad_content_length || ec == he::error::bad_transfer_encoding ||
               ec == he::error::bad_chunk || ec == he::error::bad_chunk_extension || ec == he::error::bad_obs_fold;
    }

    void Http11Driver::stamp_common_headers(Response& resp) {
        if (!resp.headers.contains("Date")) {
            // TODO Move out this standard to the common::chrono
            // TODO: cache the formatted IMF-fixdate and rebuild it at most once per
            //  second — re-running gmtime_r + snprintf on every response is wasted
            //  work when many responses fall within the same wall-clock second.

            // IMF-fixdate (RFC 9110 §5.6.7) uses FIXED English day/month names.
            // Build them from tables rather than strftime("%a"/"%b"), which honor
            // the process's global LC_TIME locale — a reusable server must not
            // emit a locale-dependent (non-compliant) Date header.
            static constexpr const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            static constexpr const char* months[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm{};
            ::gmtime_r(&t, &tm);
            char buf[32];
            const int n = std::snprintf(buf,
                                        sizeof buf,
                                        "%s, %02d %s %04d %02d:%02d:%02d GMT",
                                        days[tm.tm_wday],
                                        tm.tm_mday,
                                        months[tm.tm_mon],
                                        tm.tm_year + 1900,
                                        tm.tm_hour,
                                        tm.tm_min,
                                        tm.tm_sec);
            if (n > 0)
                resp.set_header("Date", std::string_view{buf, static_cast<std::size_t>(n)});
        }
        if (!resp.headers.contains("Server"))
            resp.set_header("Server", "Demiplane");
    }

}  // namespace demiplane::http
