#define DMP_LOG_LEVEL WRN
#include <demiplane/scroll>

#include <gtest/gtest.h>


class ServiceX final : demiplane::scroll::TestLoggerProvider {
public:
    void do_something() {
        std::size_t i = 1;
        std::vector<std::string> expected_messages;
        testing::internal::CaptureStdout();
        auto mk_message = [](const std::size_t n) { return "TestMessage" + std::to_string(n); };
        SCROLL_LOG_DBG() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        SCROLL_LOG_INF() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        SCROLL_LOG_WRN() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        SCROLL_LOG_ERR() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        SCROLL_LOG_FAT() << mk_message(i);
        expected_messages.emplace_back(mk_message(i++));
        EXPECT_EQ(i, 6);

        const std::string output = testing::internal::GetCapturedStdout();
        for (const auto& msg : expected_messages) {
            EXPECT_TRUE(output.find(msg) != std::string::npos) << "Message not found: " << msg;
        }
    }
    static constexpr std::string_view name = "ServiceX";
};

TEST(LoggerProviderTest, Test) {
    ServiceX service;
    service.do_something();
}
