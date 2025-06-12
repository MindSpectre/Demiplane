#define ENABLE_LOGGING

#include <gtest/gtest.h>

#include <demiplane/scroll2>
using namespace demiplane::scroll;
constexpr int pseudo_line = 7;
constexpr auto pseudo_file = "CURRENT_FILE";
constexpr auto pseudo_func = "CURRENT_FUNC";
class ServiceX final : TestLoggerProvider {
public:
    void do_something(){
        LOG2S_INF() << "Hello World";
    }
    static const char* name() {
        return "ServiceX";
    }
};
TEST(TestEntries, DetailedEntry) {
    const DetailedEntry entry(INF, "Hello World", pseudo_file, pseudo_line, pseudo_func);
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, LightEntry) {
    const LightEntry entry(INF, "Hello World", pseudo_file, pseudo_line, pseudo_func);
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}
TEST(TestEntries, ServiceEntry) {
    const ServiceEntry<ServiceX> entry(INF, "Hello World", pseudo_file, pseudo_line, pseudo_func);
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

TEST(TestEntries, CustomEntry) {
    const DetailedEntry entry(INF, "Hello World", pseudo_file, pseudo_line, pseudo_func);
    std::string dump;
    EXPECT_NO_THROW(dump = entry.to_string());
    std::cout << dump;
}

int main(int argc, char** argv) {

    TestLoggerProvider xyz;

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
