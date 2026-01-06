#include <demiplane/gears>
#include <demiplane/scroll>

#include <gtest/gtest.h>

using namespace demiplane::scroll;


class ServiceTest {
public:
    void do_something() const {
        std::cout << "Doing something" << std::endl;
        demiplane::gears::enforce_non_static(this);
    }

    constexpr static std::string_view name = "ServiceTest";
};


void check_location_meta(const std::string& data, const std::source_location& loc) {
    const char* fname = loc.file_name();
    if (const char* last_slash = std::strrchr(fname, '/')) {
        fname = last_slash + 1;
    }
    if (data.contains(fname) && data.contains(std::to_string(loc.line()))) {
        return;
    }
    throw std::runtime_error("Some location meta not found");
}

void check_message(const std::string& data, const std::string& message) {
    if (data.contains(message)) {
        return;
    }
    throw std::runtime_error("Message not found");
}

void check_level(const std::string& data, const LogLevel level) {
    if (data.contains(log_level_to_string(level))) {
        return;
    }
    throw std::runtime_error("Level not found");
}

TEST(TestEntries, DetailedEntry) {
    constexpr auto message             = "Hello Detailed";
    constexpr std::source_location loc = std::source_location::current();
    const auto entry                   = make_entry<DetailedEntry>(INF, message, loc);
    std::string output;
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;
    EXPECT_NO_THROW({
        check_message(output, message);
        check_level(output, INF);
        check_location_meta(output, loc);
    });
}

TEST(TestEntries, LightEntry) {
    constexpr auto message             = "Hello light";
    constexpr std::source_location loc = std::source_location::current();
    const auto entry                   = make_entry<LightEntry>(INF, message, loc);
    std::string output;
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;

    EXPECT_NO_THROW({
        check_message(output, message);
        check_level(output, INF);
    });
    EXPECT_THROW(check_location_meta(output, loc), std::runtime_error);
}

TEST(TestEntries, MakeEntryFromEvent) {
    // Test the new make_entry_from_event function
    LogEvent event{INF, "Test message from event", std::source_location::current()};

    // Create DetailedEntry from LogEvent
    auto detailed_entry         = make_entry_from_event<DetailedEntry>(event);
    std::string detailed_output = detailed_entry.to_string();

    EXPECT_TRUE(detailed_output.find("Test message from event") != std::string::npos);
    EXPECT_TRUE(detailed_output.find("INF") != std::string::npos);

    // Create LightEntry from same LogEvent
    auto light_entry         = make_entry_from_event<LightEntry>(event);
    std::string light_output = light_entry.to_string();

    EXPECT_TRUE(light_output.find("Test message from event") != std::string::npos);
    EXPECT_TRUE(light_output.find("INF") != std::string::npos);
}
