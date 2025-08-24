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


void check_location_meta(const std::string& data, const detail::MetaSource& loc) {
    if (data.contains(loc.source_file) && data.contains(loc.source_func)
        && data.contains(std::to_string(loc.source_line))) {
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
    constexpr auto message = "Hello Detailed";
    auto loc               = detail::MetaSource{__FILE__, __PRETTY_FUNCTION__, __LINE__};
    const auto entry       = demiplane::scroll::make_entry<DetailedEntry>(INF, message,
                                                                          detail::MetaSource{
                                                                              __FILE__,
                                                                              __FUNCTION__,
                                                                              __LINE__
                                                                          });
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
    auto loc = detail::MetaSource{__FILE__, __FUNCTION__, __LINE__};
    const auto entry                   = demiplane::scroll::make_entry<LightEntry>(INF, message, detail::MetaSource{__FILE__, __FUNCTION__, __LINE__});
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
    auto loc = detail::MetaSource{__FILE__, __FUNCTION__, __LINE__};

    const auto entry = demiplane::scroll::make_entry<ServiceEntry<ServiceTest>>(INF, message, detail::MetaSource{__FILE__, __FUNCTION__, __LINE__});
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
    auto cfg_ptr                       = std::make_shared<CustomEntryConfig>(CustomEntryConfig{});

    std::string output;
    auto loc = detail::MetaSource{__FILE__, __FUNCTION__, __LINE__};
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
