#define ENABLE_LOGGING

#include <demiplane/scroll2>
#include <gtest/gtest.h>
using namespace demiplane::scroll;
constexpr int pseudo_line  = 7;
constexpr auto pseudo_file = "CURRENT_FILE";
constexpr auto pseudo_func = "CURRENT_FUNC";
class ServiceX final : TestLoggerProvider {
public:
    void do_something() {
        LOG2S_INF() << "Hello World";
    }
    static const char* name() {
        return "ServiceX";
    }
};
TEST(TestEntries, DetailedEntry) {
    const auto entry = demiplane::scroll::make_entry<DetailedEntry>(INF, "Hello Detailed");
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, LightEntry) {
    const auto entry = demiplane::scroll::make_entry<LightEntry>(INF, "Hello light");
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, ServiceEntry) {
    const auto entry = demiplane::scroll::make_entry<ServiceEntry<ServiceX>>(INF, "Hello service" );
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

TEST(TestEntries, CustomEntry) {
    const auto entry = demiplane::scroll::make_entry<int>(INF, "Hello custom");
    std::string dump;
    // EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

int main(int argc, char** argv) {

    TestLoggerProvider xyz;

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
