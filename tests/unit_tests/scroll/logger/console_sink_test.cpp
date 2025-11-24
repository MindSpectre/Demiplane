#include <demiplane/scroll>

#include <gtest/gtest.h>

class ConsoleSinkTest : public ::testing::Test {
protected:
    std::unique_ptr<demiplane::scroll::Logger> logger;
    std::shared_ptr<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>> console_sink;
    demiplane::scroll::ConsoleSinkConfig cfg = {.threshold      = demiplane::scroll::DBG,
                                                     .enable_colors  = false,
                                                     .flush_each_entry = false};
    void TearDown() override {
        logger->shutdown();
        logger.reset();
    }

    void SetUp() override {

        logger = std::make_unique<demiplane::scroll::Logger>();
        auto sink = std::make_shared<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(cfg);
        console_sink = sink;
        logger->add_sink(std::move(sink));
    }
};

// Test basic logging functionality with entry objects
TEST_F(ConsoleSinkTest, LogsEntryWhenAboveThreshold) {
    // Create a mock entry
    testing::internal::CaptureStdout();

    // Log the message
    logger->log(demiplane::scroll::INF, "Test message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Get the output
    const std::string output = testing::internal::GetCapturedStdout();

    // Verify output contains the message
    EXPECT_TRUE(output.find("Test message") != std::string::npos);
    EXPECT_TRUE(output.find(demiplane::scroll::log_level_to_string(demiplane::scroll::INF)) != std::string::npos);
}

// Test that messages below the threshold are not logged
TEST_F(ConsoleSinkTest, FiltersEntriesBelowThreshold) {
    // Set threshold to ERROR
    testing::internal::CaptureStdout();
    console_sink->config().threshold = demiplane::scroll::ERR;

    // Log an INFO message (below the threshold)
    logger->log(demiplane::scroll::INF, "This should not appear");

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
    logger->log(demiplane::scroll::WRN, "Warning message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    const std::string output = testing::internal::GetCapturedStdout();

    // Verify output
    EXPECT_TRUE(output.find("Warning message") != std::string::npos);
    EXPECT_TRUE(output.find(demiplane::scroll::log_level_to_string(demiplane::scroll::WRN)) != std::string::npos);
}

// Test threshold changes
TEST_F(ConsoleSinkTest, ThresholdChangeAffectsLogging) {
    // First log with a DEBUG threshold
    testing::internal::CaptureStdout();
    logger->log(demiplane::scroll::DBG, "Debug message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Verify debug message is logged
    const std::string output1 = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output1.find("Debug message") != std::string::npos);

    // Recreate logger with new threshold
    demiplane::scroll::ConsoleSinkConfig cfg{.threshold      = demiplane::scroll::WRN,
                                             .enable_colors  = false,
                                             .flush_each_entry = false};
    logger = std::make_unique<demiplane::scroll::Logger>();
    auto sink = std::make_shared<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(cfg);
    console_sink = sink;
    logger->add_sink(std::move(sink));

    // Recapture for next test
    testing::internal::CaptureStdout();

    // Try to log the DEBUG message again
    logger->log(demiplane::scroll::DBG, "Another debug message");

    // Small delay to allow async logging
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger->shutdown();

    // Verify nothing was logged
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());

    // Recreate logger again for final test
    logger = std::make_unique<demiplane::scroll::Logger>();
    sink = std::make_shared<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(cfg);
    console_sink = sink;
    logger->add_sink(std::move(sink));

    // Recapture for next test
    testing::internal::CaptureStdout();

    // Log WARNING message which should appear
    logger->log(demiplane::scroll::WRN, "Warning message");

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
    console_sink->config().threshold = demiplane::scroll::DBG;

    // Test each log level
    std::vector<std::pair<demiplane::scroll::LogLevel, std::string>> levels = {
        {demiplane::scroll::DBG, "DEBUG"  },
        {demiplane::scroll::INF, "INFO"   },
        {demiplane::scroll::WRN, "WARNING"},
        {demiplane::scroll::ERR, "ERROR"  },
        {demiplane::scroll::FAT, "FATAL"  }
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
        if (level != demiplane::scroll::FAT) {
            demiplane::scroll::ConsoleSinkConfig cfg{.threshold      = demiplane::scroll::DBG,
                                                     .enable_colors  = false,
                                                     .flush_each_entry = false};
            logger = std::make_unique<demiplane::scroll::Logger>();
            auto sink = std::make_shared<demiplane::scroll::ConsoleSink<demiplane::scroll::LightEntry>>(cfg);
            console_sink = sink;
            logger->add_sink(std::move(sink));
        }
    }
}
