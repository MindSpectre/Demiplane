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
    if (data.contains(loc.file_name()) && data.contains(loc.function_name())
        && data.contains(std::to_string(loc.line()))) {
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
    const auto entry                   = demiplane::scroll::make_entry<DetailedEntry>(INF, message, loc);
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
    const auto entry                   = demiplane::scroll::make_entry<LightEntry>(INF, message, loc);
    std::string output;
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;

    EXPECT_NO_THROW({
        check_message(output, message);
        check_level(output, INF);
    });
    EXPECT_THROW(check_location_meta(output, loc), std::runtime_error);
}

TEST(TestEntries, ServiceEntry) {
    constexpr auto message             = "Hello service";
    constexpr std::source_location loc = std::source_location::current();

    const auto entry = demiplane::scroll::make_entry<ServiceEntry<ServiceTest>>(INF, message, loc);
    std::string output;
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;

    EXPECT_NO_THROW({
        check_message(output, message);
        check_level(output, INF);
        check_location_meta(output, loc);
    });
    EXPECT_TRUE(output.contains(ServiceTest::name));
}

TEST(TestEntries, CustomEntry) {
    constexpr auto message             = "Hello custom";
    constexpr std::source_location loc = std::source_location::current();
    auto cfg_ptr                       = std::make_shared<CustomEntryConfig>(CustomEntryConfig{});

    std::string output;
    auto mk_entry = [&] { return demiplane::scroll::make_entry<CustomEntry>(INF, message, loc, cfg_ptr); };

    auto entry = mk_entry();
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;
    check_message(output, message);
    check_level(output, INF);
    EXPECT_THROW(check_location_meta(output, loc), std::runtime_error);

    cfg_ptr->add_pretty_function = true;
    entry                        = mk_entry();
    EXPECT_NO_THROW(output = entry.to_string());
    std::cout << output;
    EXPECT_NO_THROW(check_location_meta(output, loc));
}
