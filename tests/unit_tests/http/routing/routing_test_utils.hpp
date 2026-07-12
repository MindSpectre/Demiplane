#pragma once

#include <deque>
#include <memory_resource>
#include <string>
#include <utility>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body.hpp>
#include <headers.hpp>
#include <request.hpp>
#include <request_context.hpp>

namespace http_routing_test {

    template <typename T>
    T run_awaitable(boost::asio::awaitable<T, demiplane::http::Strand> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc.get_executor(), std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();  // rethrows handler exceptions
    }

    class RoutingTestBase : public ::testing::Test {
    protected:
        std::pmr::monotonic_buffer_resource resource_{8192};
        std::pmr::polymorphic_allocator<> alloc_{&resource_};
        std::deque<std::string> target_storage_;  // stable backing for string_view targets

        demiplane::http::Request make_request(const demiplane::http::HttpMethod m, std::string target) {
            using namespace demiplane::http;
            Request req{Headers::owned(alloc_)};
            req.method = m;
            target_storage_.push_back(std::move(target));
            req.target = target_storage_.back();
            return req;
        }

        demiplane::http::RequestContext make_ctx(const demiplane::http::HttpMethod m, std::string target) {
            return demiplane::http::RequestContext{make_request(m, std::move(target)), alloc_};
        }
    };

}  // namespace http_routing_test
