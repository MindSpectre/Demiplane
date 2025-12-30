
#include <chrono>
#include <demiplane/scroll>
#include <thread>

#include <gtest/gtest.h>


class ServiceX final : demiplane::scroll::TestLoggerProvider {
public:
    void do_something() {
        std::size_t i = 1;
        std::vector<std::string> expected_messages;
        testing::internal::CaptureStdout();
        auto mk_message = [](const std::size_t n) { return "TestMessage" + std::to_string(n); };

        LOG_DBG() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        LOG_INF() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        LOG_WRN() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        LOG_ERR() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        LOG_FAT() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        EXPECT_EQ(i, 6);

        // Wait for async logger to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const std::string output = testing::internal::GetCapturedStdout();
        for (const auto& msg : expected_messages) {
            EXPECT_TRUE(output.find(msg) != std::string::npos) << "Message not found: " << msg;
        }
        demiplane::gears::enforce_non_const(this);
    }
    static constexpr std::string_view name = "ServiceX";
};

TEST(LoggerProviderTest, StreamStyleLogging) {
    ServiceX service;
    service.do_something();
}

TEST(LoggerProviderTest, FormatStyleLogging) {
    const auto logger = std::make_shared<demiplane::scroll::Logger>();
    logger->add_sink(std::make_unique<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(
        demiplane::scroll::ConsoleSinkConfig{}
            .threshold(demiplane::scroll::LogLevel::Debug)
            .enable_colors(false)
            .flush_each_entry(true)
            .finalize()));

    testing::internal::CaptureStdout();

    std::string username = "alice";
    int count            = 42;

    LOG_DIRECT_FMT_INF(logger, "User {} has {} items", username, count);
    LOG_DIRECT_FMT_DBG(logger, "Debug message with value {}", 123);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("alice") != std::string::npos);
    EXPECT_TRUE(output.find("42") != std::string::npos);
    EXPECT_TRUE(output.find("123") != std::string::npos);
}

TEST(LoggerProviderTest, OverloadedMacros) {
    const auto logger = std::make_shared<demiplane::scroll::Logger>();
    logger->add_sink(std::make_unique<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(
        demiplane::scroll::ConsoleSinkConfig{}
            .threshold(demiplane::scroll::LogLevel::Debug)
            .enable_colors(false)
            .flush_each_entry(true)
            .finalize()));


    testing::internal::CaptureStdout();

    // Test stream style (0 args)
    LOG_DIRECT_STREAM_INF(logger) << "Stream style message";

    // Test format style (1+ args)
    LOG_DIRECT_FMT_INF(logger, "Format style message {}", 123);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Stream style message") != std::string::npos);
    EXPECT_TRUE(output.find("Format style message") != std::string::npos);
    EXPECT_TRUE(output.find("123") != std::string::npos);
}
