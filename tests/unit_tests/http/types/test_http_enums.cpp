#include <gtest/gtest.h>
#include <http_enums.hpp>

using namespace demiplane::http;

TEST(HttpEnumsTest, MethodToString) {
    EXPECT_EQ(to_string(HttpMethod::get),     std::string_view{"GET"});
    EXPECT_EQ(to_string(HttpMethod::post),    std::string_view{"POST"});
    EXPECT_EQ(to_string(HttpMethod::del),     std::string_view{"DELETE"});
    EXPECT_EQ(to_string(HttpMethod::options), std::string_view{"OPTIONS"});
}

TEST(HttpEnumsTest, MethodFromBeast) {
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::get),     HttpMethod::get);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::delete_), HttpMethod::del);
    EXPECT_EQ(method_from_beast(boost::beast::http::verb::unknown), HttpMethod::unknown);
}

TEST(HttpEnumsTest, StatusCodeNumericValue) {
    EXPECT_EQ(static_cast<int>(HttpStatus::ok),                    200);
    EXPECT_EQ(static_cast<int>(HttpStatus::no_content),            204);
    EXPECT_EQ(static_cast<int>(HttpStatus::not_found),             404);
    EXPECT_EQ(static_cast<int>(HttpStatus::method_not_allowed),    405);
    EXPECT_EQ(static_cast<int>(HttpStatus::payload_too_large),     413);
    EXPECT_EQ(static_cast<int>(HttpStatus::internal_server_error), 500);
}

TEST(HttpEnumsTest, VersionNumeric) {
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_1_1), 11u);
    EXPECT_EQ(static_cast<unsigned>(HttpVersion::http_2),   20u);
}
