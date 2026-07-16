#include <array>
#include <ctime>
#include <demiplane/chrono>
#include <span>
#include <string_view>

#include <gtest/gtest.h>
#include <http_date.hpp>
namespace {

    using demiplane::chrono::format_imf_fixdate;
    using demiplane::chrono::IMF_FIXDATE_LEN;
    using demiplane::http::imf_fixdate_now;

    std::string_view render(const std::time_t t, std::array<char, IMF_FIXDATE_LEN + 1>& buf) {
        const auto len = format_imf_fixdate(t, std::span<char, IMF_FIXDATE_LEN + 1>{buf});
        return {buf.data(), len};
    }

    TEST(HttpDate, EpochRendersRfc9110Fixdate) {
        std::array<char, IMF_FIXDATE_LEN + 1> buf{};
        EXPECT_EQ(render(0, buf), "Thu, 01 Jan 1970 00:00:00 GMT");
    }

    TEST(HttpDate, Rfc9110SectionExampleRenders) {
        // The RFC 9110 §5.6.7 example date.
        std::array<char, IMF_FIXDATE_LEN + 1> buf{};
        EXPECT_EQ(render(784111777, buf), "Sun, 06 Nov 1994 08:49:37 GMT");
    }

    TEST(HttpDate, LeapDayRenders) {
        // 2024-02-29 12:00:00 UTC
        std::array<char, IMF_FIXDATE_LEN + 1> buf{};
        EXPECT_EQ(render(1709208000, buf), "Thu, 29 Feb 2024 12:00:00 GMT");
    }

    TEST(HttpDate, LengthIsAlwaysFixed) {
        std::array<char, IMF_FIXDATE_LEN + 1> buf{};
        for (const std::time_t t :
             {std::time_t{0}, std::time_t{784111777}, std::time_t{1709208000}, std::time(nullptr)})
            EXPECT_EQ(render(t, buf).size(), IMF_FIXDATE_LEN);
    }

    TEST(HttpDate, CachedNowMatchesDirectRender) {
        // Retry across a possible second rollover between the two reads.
        for (int attempt = 0; attempt < 3; ++attempt) {
            const std::time_t before      = std::time(nullptr);
            const std::string_view cached = imf_fixdate_now();
            const std::time_t after       = std::time(nullptr);
            std::array<char, IMF_FIXDATE_LEN + 1> buf{};
            if (before != after)
                continue;  // rolled over mid-check; try again
            EXPECT_EQ(cached, render(before, buf));
            return;
        }
        GTEST_SKIP() << "second rolled over on every attempt";
    }

    TEST(HttpDate, RepeatedCallsWithinASecondReturnIdenticalBytes) {
        const std::string_view a = imf_fixdate_now();
        const std::string_view b = imf_fixdate_now();
        EXPECT_EQ(a.data(), b.data());  // same thread_local storage
        EXPECT_EQ(a, b);
    }

}  // namespace
