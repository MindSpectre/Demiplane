#include <demiplane/scroll>
#include <gtest/gtest.h>

class ConsoleLoggerTest : public ::testing::Test {
protected:
    demiplane::scroll::ConsoleLogger<demiplane::scroll::LightEntry> console_logger;
};

// Test basic logging functionality with entry objects
TEST_F(ConsoleLoggerTest, LogsEntryWhenAboveThreshold) {
    // Create a mock entry
    testing::internal::CaptureStdout();
    const auto entry = demiplane::scroll::make_entry<demiplane::scroll::LightEntry>(
        demiplane::scroll::INF, "Test message", std::source_location::current());

    // Log the entry
    console_logger.log(entry);

    // Get the output
    const std::string output = testing::internal::GetCapturedStdout();

    // Verify output contains the message
    EXPECT_TRUE(output.find("Test message") != std::string::npos);
    EXPECT_TRUE(output.find("INFO") != std::string::npos);
}

// Test that messages below the threshold are not logged
TEST_F(ConsoleLoggerTest, FiltersEntriesBelowThreshold) {
    // Set threshold to ERROR
    testing::internal::CaptureStdout();
    console_logger.set_threshold(demiplane::scroll::ERR);

    // Create and log an INFO entry (below the threshold)
    auto entry = demiplane::scroll::make_entry<demiplane::scroll::LightEntry>(
        demiplane::scroll::INF, "This should not appear", std::source_location::current());

    console_logger.log(entry);

    // Output should be empty
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
}

// Test direct logging with message and source location
TEST_F(ConsoleLoggerTest, DirectLoggingWithSourceLocation) {
    // Log directly with a message
    testing::internal::CaptureStdout();
    console_logger.log(demiplane::scroll::WRN, "Warning message", std::source_location::current());

    std::string output = testing::internal::GetCapturedStdout();

    // Verify output
    EXPECT_TRUE(output.find("Warning message") != std::string::npos);
    EXPECT_TRUE(output.find("WARNING") != std::string::npos);
}

// Test threshold changes
TEST_F(ConsoleLoggerTest, ThresholdChangeAffectsLogging) {
    // First log with a DEBUG threshold
    testing::internal::CaptureStdout();
    console_logger.log(demiplane::scroll::DBG, "Debug message", std::source_location::current());

    // Verify debug message is logged
    std::string output1 = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output1.find("Debug message") != std::string::npos);

    // Recapture for next test
    testing::internal::CaptureStdout();

    // Change the threshold to WARNING
    console_logger.set_threshold(demiplane::scroll::WRN);

    // Try to log the DEBUG message again
    console_logger.log(demiplane::scroll::DBG, "Another debug message", std::source_location::current());

    // Verify nothing was logged
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());

    // Recapture for next test
    testing::internal::CaptureStdout();

    // Log WARNING message which should appear
    console_logger.log(demiplane::scroll::WRN, "Warning message", std::source_location::current());

    // Verify warning is logged
    std::string output2 = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output2.find("Warning message") != std::string::npos);
}

// Test all log levels
TEST_F(ConsoleLoggerTest, AllLogLevels) {
    // Make sure a threshold is at the lowest level
    console_logger.set_threshold(demiplane::scroll::DBG);

    // Test each log level
    std::vector<std::pair<demiplane::scroll::LogLevel, std::string>> levels = {{demiplane::scroll::DBG, "DEBUG"},
        {demiplane::scroll::INF, "INFO"}, {demiplane::scroll::WRN, "WARNING"}, {demiplane::scroll::ERR, "ERROR"},
        {demiplane::scroll::FAT, "FATAL"}};

    for (const auto& [level, levelName] : levels) {
        // Recapture for each level
        testing::internal::CaptureStdout();

        // Log a message at this level
        std::string message = levelName + " test message";
        console_logger.log(level, message, std::source_location::current());

        // Get output
        std::string output = testing::internal::GetCapturedStdout();

        // Verify level name and message appear in the output
        EXPECT_TRUE(output.find(levelName) != std::string::npos) << "Log level " << levelName << " not found in output";
        EXPECT_TRUE(output.find(message) != std::string::npos) << "Message for " << levelName << " not found in output";
    }
}

// Test constructor with the threshold
TEST_F(ConsoleLoggerTest, ConstructorWithThreshold) {
    // Create a logger with the ERROR threshold
    demiplane::scroll::ConsoleLogger<demiplane::scroll::LightEntry> error_logger{demiplane::scroll::ERR};

    // Verify the threshold is set
    EXPECT_EQ(error_logger.get_threshold(), demiplane::scroll::ERR);

    // Verify INFO messages are filtered
    testing::internal::CaptureStdout();
    error_logger.log(demiplane::scroll::INF, "Should not appear", std::source_location::current());
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());

    // Verify ERROR messages are logged
    testing::internal::CaptureStdout();
    error_logger.log(demiplane::scroll::ERR, "Should appear", std::source_location::current());
    EXPECT_FALSE(testing::internal::GetCapturedStdout().empty());
}
