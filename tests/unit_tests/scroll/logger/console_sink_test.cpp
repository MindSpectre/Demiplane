#include <demiplane/scroll>

#include <gtest/gtest.h>
using namespace demiplane::scroll;
class ConsoleSinkTest : public ::testing::Test {
protected:
    std::unique_ptr<Logger> logger;
    std::shared_ptr<ConsoleSink<LightEntry>> console_sink;
    ConsoleSinkConfig cfg =
        ConsoleSinkConfig{}.threshold(LogLevel::Debug).enable_colors(false).flush_each_entry(false).finalize();
    void TearDown() override {
        logger->shutdown();
        logger.reset();
    }

    void SetUp() override {
        reinit_logger(cfg);
    }

    void reinit_logger(ConsoleSinkConfig config) {
        logger       = std::make_unique<Logger>();
        auto sink    = std::make_shared<ConsoleSink<LightEntry>>(std::move(config));
        console_sink = sink;
        logger->add_sink(std::move(sink));
    }
};

// Test basic logging functionality with entry objects
TEST_F(ConsoleSinkTest, LogsEntryWhenAboveThreshold) {
    // Create a mock entry
    testing::internal::CaptureStdout();

    // Log the message
    logger->log(INF, "Test message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Get the output
    const std::string output = testing::internal::GetCapturedStdout();

    // Verify output contains the message
    EXPECT_TRUE(output.find("Test message") != std::string::npos);
    EXPECT_TRUE(output.find(log_level_to_string(INF)) != std::string::npos);
}

// Test that messages below the threshold are not logged
TEST_F(ConsoleSinkTest, FiltersEntriesBelowThreshold) {
    // Set threshold to ERROR
    testing::internal::CaptureStdout();
    console_sink->config().threshold(ERR);

    // Log an INFO message (below the threshold)
    logger->log(INF, "This should not appear");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Output should be empty
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
}

// Test direct logging with message and source location
TEST_F(ConsoleSinkTest, DirectLoggingWithSourceLocation) {
    // Log directly with a message
    testing::internal::CaptureStdout();
    logger->log(WRN, "Warning message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    const std::string output = testing::internal::GetCapturedStdout();

    // Verify output
    EXPECT_TRUE(output.find("Warning message") != std::string::npos);
    EXPECT_TRUE(output.find(log_level_to_string(WRN)) != std::string::npos);
}

// Test threshold changes
TEST_F(ConsoleSinkTest, ThresholdChangeAffectsLogging) {
    // First log with a DEBUG threshold
    testing::internal::CaptureStdout();
    logger->log(DBG, "Debug message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Verify debug message is logged
    const std::string output1 = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output1.find("Debug message") != std::string::npos);

    // Recreate logger with new threshold
    const ConsoleSinkConfig cfg =
        ConsoleSinkConfig{}.threshold(WRN).enable_colors(false).flush_each_entry(true).finalize();

    reinit_logger(cfg);
    // Recapture for next test
    testing::internal::CaptureStdout();

    // Try to log the DEBUG message again
    logger->log(DBG, "Another debug message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Verify nothing was logged
    const std::string svg = testing::internal::GetCapturedStdout();
    std::cout << svg << std::endl;
    EXPECT_TRUE(svg.empty());

    // Recreate logger again for final test
    reinit_logger(cfg);

    // Recapture for next test
    testing::internal::CaptureStdout();

    // Log WARNING message which should appear
    logger->log(WRN, "Warning message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Verify warning is logged
    const std::string output2 = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output2.find("Warning message") != std::string::npos);
}

// Test all log levels
TEST_F(ConsoleSinkTest, AllLogLevels) {
    // Make sure a threshold is at the lowest level
    console_sink->config().threshold(DBG);

    // Test each log level
    std::vector<std::pair<LogLevel, std::string>> levels = {
        {DBG, "DEBUG"  },
        {INF, "INFO"   },
        {WRN, "WARNING"},
        {ERR, "ERROR"  },
        {FAT, "FATAL"  }
    };

    for (const auto& [level, levelName] : levels) {
        // Recapture for each level
        testing::internal::CaptureStdout();

        // Log a message at this level
        std::string message = levelName + " test message";
        logger->log(level, message);

        // Small delay to allow async logging
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        logger->shutdown();

        // Get output
        std::string output = testing::internal::GetCapturedStdout();

        // Verify level name and message appear in the output
        EXPECT_TRUE(output.find(levelName) != std::string::npos) << "Log level " << levelName << " not found in output";
        EXPECT_TRUE(output.find(message) != std::string::npos) << "Message for " << levelName << " not found in output";

        // Recreate logger for next iteration
        if (level != FAT) {
            auto cfg = ConsoleSinkConfig{}.threshold(DBG).enable_colors(true).flush_each_entry(false).finalize();
            reinit_logger(std::move(cfg));
        }
    }
}
