#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

#include <json/json.hpp>
#include <timeouts.hpp>

using namespace demiplane::http;
using namespace std::chrono_literals;

TEST(TimeoutsConfigTest, DefaultsMatchSpec) {
    const auto t = Timeouts::deserialize<Json::Value>(Json::Value{Json::objectValue});
    EXPECT_EQ(t.header(), 10s);
    EXPECT_EQ(t.body(), 30s);
    EXPECT_EQ(t.idle(), 60s);
}

TEST(TimeoutsConfigTest, RoundTripsThroughJson) {
    const auto t    = Timeouts{5000ms, 10000ms, 30000ms};  // public full ctor (spec §10.1)
    const auto json = t.serialize<Json::Value>();
    EXPECT_EQ(json["header_ms"].asInt64(), 5000);
    EXPECT_EQ(json["body_ms"].asInt64(), 10000);
    EXPECT_EQ(json["idle_ms"].asInt64(), 30000);
    const auto back = Timeouts::deserialize<Json::Value>(json);
    EXPECT_EQ(back.header(), 5000ms);
    EXPECT_EQ(back.body(), 10000ms);
    EXPECT_EQ(back.idle(), 30000ms);
}

TEST(TimeoutsConfigTest, BuilderBuildsAndValidates) {
    const auto t = Timeouts::Builder{}.header(1s).body(2s).idle(3s).finalize();
    EXPECT_EQ(t.header(), 1s);
    EXPECT_EQ(t.body(), 2s);
    EXPECT_EQ(t.idle(), 3s);
}

TEST(TimeoutsConfigTest, ValidateRejectsNonPositive) {
    EXPECT_THROW((void)Timeouts::Builder{}.header(0ms).finalize(), std::invalid_argument);
    EXPECT_THROW((void)Timeouts::Builder{}.body(-1ms).finalize(), std::invalid_argument);
    EXPECT_THROW((void)Timeouts::Builder{}.idle(0ms).finalize(), std::invalid_argument);
    EXPECT_THROW(((void)Timeouts{0ms, 1ms, 1ms}.serialize<Json::Value>()), std::invalid_argument);
}
