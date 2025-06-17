#define ENABLE_LOGGING
#define DMP_LOG_LEVEL WRN
#include <demiplane/scroll>
#include <gtest/gtest.h>


using namespace demiplane::scroll;
constexpr int pseudo_line  = 7;
constexpr auto pseudo_file = "CURRENT_FILE";
constexpr auto pseudo_func = "CURRENT_FUNC";
class ServiceX final : TestLoggerProvider {
public:
    void do_something() {
        std::size_t i = 0;
        SCROLL_LOG() << "TestMessage" << ++i;
        SCROLL_LOG_DBG() << "TestMessage" << ++i;
        SCROLL_LOG_INF() << "TestMessage" << ++i;
        SCROLL_LOG_WRN() << "TestMessage" << ++i;
        SCROLL_LOG_ERR() << "TestMessage" << ++i;
        SCROLL_LOG_FAT() << "TestMessage" << ++i;
        SCROLL_LOG_MESSAGE("TestMessage" + std::to_string(++i));
        SCROLL_LOG_MESSAGE_DBG("TestMessage" + std::to_string(++i));
        SCROLL_LOG_MESSAGE_INF("TestMessage" + std::to_string(++i));
        SCROLL_LOG_MESSAGE_WRN("TestMessage" + std::to_string(++i));
        SCROLL_LOG_MESSAGE_ERR("TestMessage" + std::to_string(++i));
        SCROLL_LOG_MESSAGE_FAT("TestMessage" + std::to_string(++i));
        EXPECT_EQ(i, 12);
    }
    static const char* name() {
        return "ServiceX";
    }
};
TEST(TestEntries, DetailedEntry) {
    const auto entry =
        demiplane::scroll::make_entry<DetailedEntry>(INF, "Hello Detailed", std::source_location::current());
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, LightEntry) {
    const auto entry = demiplane::scroll::make_entry<LightEntry>(INF, "Hello light", std::source_location::current());
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, ServiceEntry) {
    const auto entry =
        demiplane::scroll::make_entry<ServiceEntry<ServiceX>>(INF, "Hello service", std::source_location::current());
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

TEST(TestEntries, CustomEntry) {
    CustomEntryConfig cfg;
    auto cfg_ptr = std::make_shared<CustomEntryConfig>(cfg);
    const auto entry = demiplane::scroll::make_entry<CustomEntry>(
        INF, "Hello custom", std::source_location::current(), cfg_ptr); // NOT COMPILED

    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

int main(int argc, char** argv) {
    ServiceX x;
    x.do_something();
    TestLoggerProvider xyz;

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
