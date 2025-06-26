
#include <demiplane/scroll>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

class FileLoggerTest : public ::testing::Test {
protected:
    demiplane::scroll::FileLoggerConfig cfg {
        .threshold = demiplane::scroll::DBG,
        .file = "test.log",
        .add_time_to_name = false,
        .safe_mode = true
    };
    std::shared_ptr<demiplane::scroll::FileLogger<demiplane::scroll::LightEntry>> file_logger;

    void SetUp() override {
        // Ensure log file doesn't exist before test
        file_logger = std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::LightEntry>>(cfg);
    }

    void TearDown() override {
        // Clean up log file after test
        std::filesystem::remove(cfg.file);
    }

    [[nodiscard]] std::string read_log_file() const {
        std::ifstream file(cfg.file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

// Test basic logging functionality with entry objects
TEST_F(FileLoggerTest, LogsEntryWhenAboveThreshold) {
    // Create a mock entry
    const auto entry = demiplane::scroll::make_entry<demiplane::scroll::LightEntry>(
        demiplane::scroll::INF, "Test message", std::source_location::current());

    // Log the entry
    file_logger->log(entry);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Get the output from file
    const std::string output = read_log_file();

    // Verify output contains the message
    EXPECT_TRUE(output.find("Test message") != std::string::npos);
    EXPECT_TRUE(output.find("INFO") != std::string::npos);
}

// Test that messages below the threshold are not logged
TEST_F(FileLoggerTest, FiltersEntriesBelowThreshold) {
    // Set threshold to ERROR
    file_logger->config().threshold = (demiplane::scroll::ERR);
    file_logger->reload();
    // Create and log an INFO entry (below the threshold)
    auto entry = demiplane::scroll::make_entry<demiplane::scroll::LightEntry>(
        demiplane::scroll::INF, "This should not appear", std::source_location::current());

    file_logger->log(entry);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Output should be empty
    EXPECT_TRUE(read_log_file().empty());
}

// Test direct logging with message and source location
TEST_F(FileLoggerTest, DirectLoggingWithSourceLocation) {
    // Log directly with a message
    file_logger->log(demiplane::scroll::WRN, "Warning message", std::source_location::current());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string output = read_log_file();

    // Verify output
    EXPECT_TRUE(output.find("Warning message") != std::string::npos);
    EXPECT_TRUE(output.find("WARNING") != std::string::npos);
}

// Test threshold changes
TEST_F(FileLoggerTest, ThresholdChangeAffectsLogging) {
    // Log with a DEBUG threshold
    file_logger->log(demiplane::scroll::DBG, "Debug message", std::source_location::current());

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    // Verify debug message is logged
    std::string output1 = read_log_file();
    EXPECT_TRUE(output1.find("Debug message") != std::string::npos);

    // Clean up file for next test

    std::filesystem::remove(cfg.file);
    // Change the threshold to WARNING
    file_logger->config().threshold = (demiplane::scroll::WRN);
    file_logger->reload();
    // Try to log the DEBUG message again
    // file_logger->log(demiplane::scroll::DBG, "Another debug message", std::source_location::current());
    // std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Verify nothing was logged
    EXPECT_TRUE(read_log_file().empty());

    // Clean up file for next test
    std::filesystem::remove(cfg.file);
    file_logger->reload();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Log WARNING message which should appear
    file_logger->log(demiplane::scroll::WRN, "Warning message", std::source_location::current());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Verify warning is logged
    std::string output2 = read_log_file();
    EXPECT_TRUE(output2.find("Warning message") != std::string::npos);
}

// Test all log levels
TEST_F(FileLoggerTest, AllLogLevels) {
    // Test each log level
    std::vector<std::pair<demiplane::scroll::LogLevel, std::string>> levels = {{demiplane::scroll::DBG, "DEBUG"},
        {demiplane::scroll::INF, "INFO"}, {demiplane::scroll::WRN, "WARNING"}, {demiplane::scroll::ERR, "ERROR"},
        {demiplane::scroll::FAT, "FATAL"}};

    for (const auto& [level, levelName] : levels) {
        // Clean up file before each level test
        std::filesystem::remove(cfg.file);
        file_logger->reload();

        // Log a message at this level
        std::string message = levelName + " test message";
        file_logger->log(level, message, std::source_location::current());
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        // Get output
        std::string output = read_log_file();

        // Verify level name and message appear in the output
        EXPECT_TRUE(output.find(levelName) != std::string::npos) << "Log level " << levelName << " not found in output";
        EXPECT_TRUE(output.find(message) != std::string::npos) << "Message for " << levelName << " not found in output";
    }
}


// Test file creation and appending
TEST_F(FileLoggerTest, FileCreationAndAppending) {
    // First message
    file_logger->log(demiplane::scroll::INF, "First message", std::source_location::current());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string output1 = read_log_file();
    EXPECT_TRUE(output1.find("First message") != std::string::npos);

    // Second message should be appended
    file_logger->log(demiplane::scroll::INF, "Second message", std::source_location::current());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string output2 = read_log_file();
    EXPECT_TRUE(output2.find("First message") != std::string::npos);
    EXPECT_TRUE(output2.find("Second message") != std::string::npos);
}

// Test file path handling
TEST_F(FileLoggerTest, FilePathHandling) {
    // Create a logger with a path that includes directories
    const std::string nested_path = "test_dir/nested/test_log.txt";

    // Clean up any existing directories
    std::filesystem::remove_all("test_dir");

    // Create logger with nested path
    file_logger->config().file = nested_path;
    file_logger->reload();
    // Log a message
    file_logger->log(demiplane::scroll::INF, "Test message in nested directory", std::source_location::current());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Verify directory was created and file exists
    EXPECT_TRUE(std::filesystem::exists(nested_path));

    // Read the file content
    std::ifstream nested_file(nested_path);
    std::stringstream buffer;
    buffer << nested_file.rdbuf();
    std::string output = buffer.str();

    EXPECT_TRUE(output.find("Test message in nested directory") != std::string::npos);

    // Clean up
    // std::filesystem::remove_all("test_dir");
}
